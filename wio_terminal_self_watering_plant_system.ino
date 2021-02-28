// Hints to future self:
// If you get an out of water message but the tank is full of water it is likely due to power loss
// Capactive water level sensor needs to be wiped clean after power reset
// It won't remember existing water levels

#include <Wire.h>
#include "TFT_eSPI.h" //include TFT LCD 

#define LCD_BACKLIGHT (72Ul) // Control Pin of LCD

#ifdef ARDUINO_SAMD_VARIANT_COMPLIANCE
#define SERIAL SerialUSB
#else
#define SERIAL Serial
#endif

#define NO_TOUCH       0xFE
#define THRESHOLD      100
#define ATTINY1_HIGH_ADDR   0x78
#define ATTINY2_LOW_ADDR   0x77

#define INCREMENT 5 // Target moisture level increments

// Two variables below used for determining water depth
unsigned char low_data[8] = {0};
unsigned char high_data[12] = {0};

volatile int target_moisture = 20; /* % between 0-100 */
int actual_moisture_raw = 0;
volatile int actual_moisture_converted = 0;
int old_moisture_converted = 0; /* Used to update screen on change */

int min_water_level = 25;
int actual_water_level = 0;

bool out_of_water = false;
bool out_of_water_display = false;
bool pump_on = false;
bool lcd_on = false;


// Display
TFT_eSPI tft; //initialize TFT LCD 

// Helper functions for getting water level
void getHigh12SectionValue(void)
{
  memset(high_data, 0, sizeof(high_data));
  Wire.requestFrom(ATTINY1_HIGH_ADDR, 12);
  while (12 != Wire.available());
 
  for (int i = 0; i < 12; i++) {
    high_data[i] = Wire.read();
  }
  delay(10);
}
 
void getLow8SectionValue(void)
{
  memset(low_data, 0, sizeof(low_data));
  Wire.requestFrom(ATTINY2_LOW_ADDR, 8);
  while (8 != Wire.available());
 
  for (int i = 0; i < 8 ; i++) {
    low_data[i] = Wire.read(); // receive a byte as character
  }
  delay(10);
}

int getWaterLevel()
{
  int sensorvalue_min = 250;
  int sensorvalue_max = 255;
  int low_count = 0;
  int high_count = 0;
  int max_touch_point = 0;
  while (1)
  {
    uint32_t touch_val = 0;
    uint8_t trig_section = 0;
    low_count = 0;
    high_count = 0;
    getLow8SectionValue();
    getHigh12SectionValue();
    for (int i = 0 ; i < 8; i++) {
      if (low_data[i] > THRESHOLD) {
        max_touch_point = i; 
      }
    }
    for (int i = 0 ; i < 12; i++) {
      if (high_data[i] > THRESHOLD) {
        max_touch_point = 8 + i;
      }
    }

    return(max_touch_point * 5);
  }
}

int getSoilMoistureLevel()
{
  return analogRead(6); //connect sensor to Analog 1
}

int convertRawInputToCalibratedValue(int input) {
  int air = 780; // From testing, air immersion value: 300
  int water = 425; // From testing, water immersion value: 250
  int converted;

  if (input >= air) {
    converted = 0;
  } else if (input <= water) {
    converted = 100;
  } else {
    converted = 100 - ((input - water) * 100)/(air - water);
  }

  return converted;
}

void turnOnPump() {
  digitalWrite(0, HIGH);
  pump_on = true;
}

void turnOffPump() {
  digitalWrite(0, LOW);
  pump_on = false;
}

void increaseTargetMoistureLevel() {
  target_moisture += INCREMENT;
  if (target_moisture >= 95) {
    target_moisture = 95; // 100% throws off formatting
  }
  updateTargetMoistureLevel(target_moisture);
}

void decreaseTargetMoistureLevel() {
  target_moisture -= INCREMENT;
  if (target_moisture <= 10) {
    target_moisture = 10; // <10% throws off formatting
  }
  updateTargetMoistureLevel(target_moisture);
}


// Following functions are all for drawing values on the screeen
void drawMoistureLevel(int target, int actual) {
  tft.setTextSize(5);
  tft.fillRect(0, 160, 320, 80, TFT_RED);
  tft.drawNumber(target, 55, 170);
  tft.setTextSize(4);
  tft.drawString("%", 115, 177);
  tft.setTextSize(5);
  tft.drawNumber(actual, 185, 170);
  tft.setTextSize(4);
  tft.drawString("%", 245, 177);
  tft.setTextSize(5);
  tft.setTextSize(2);
}

void drawInitialScreen() {
  tft.fillRect(0,80,320, 40, TFT_RED);
  tft.setTextSize(3);
  tft.drawString("Moisture Level", 40,90);
  tft.setTextSize(2);
  tft.fillRect(0,120,320, 40, TFT_RED);
  tft.drawString("Target", 60, 140);
  tft.drawString("Actual", 190, 140);
 
}

void updateActualMoistureLevel(int actual) {
  tft.fillRect(185,170,59,70, TFT_RED);
  tft.setTextSize(5);
  tft.drawNumber(actual, 185, 170);
  tft.setTextSize(2);
}

void updateTargetMoistureLevel(int target) {
  tft.fillRect(55,170,59,70, TFT_RED);
  tft.setTextSize(5);
  tft.drawNumber(target, 55, 170);
  tft.setTextSize(2);
}

void displayOutOfWater() {
  tft.fillRect(0, 0, 320, 25, TFT_RED);
  tft.drawString("out of water", 5, 5);
};

void removeOutOfWaterMessage() {
  tft.fillRect(0, 0, 320, 25, TFT_BLACK);
}

void toggleLCDBacklight() {
  if (lcd_on == true) {
    digitalWrite(LCD_BACKLIGHT, LOW);
    lcd_on = false;
  } else {
    digitalWrite(LCD_BACKLIGHT, HIGH);
    lcd_on = true;
  }
}

void setup() {
  SERIAL.begin(115200);
  Wire.begin();

  pinMode(0, OUTPUT); // Relay for pump
  pinMode(6, INPUT); // Moisture sensor
  pinMode(WIO_KEY_A, INPUT); //set button A pin as input 
  pinMode(WIO_KEY_B, INPUT); //set button B pin as input 
  pinMode(WIO_KEY_C, INPUT); //set button C pin as input 

  attachInterrupt(digitalPinToInterrupt(WIO_5S_RIGHT), increaseTargetMoistureLevel, RISING);
  attachInterrupt(digitalPinToInterrupt(WIO_5S_LEFT), decreaseTargetMoistureLevel, RISING);
  attachInterrupt(digitalPinToInterrupt(WIO_KEY_A), toggleLCDBacklight, FALLING);
  attachInterrupt(digitalPinToInterrupt(WIO_KEY_B), toggleLCDBacklight, FALLING);
  attachInterrupt(digitalPinToInterrupt(WIO_KEY_C), toggleLCDBacklight, FALLING);

  tft.begin(); //start TFT LCD 
  tft.setRotation(3); //set screen rotation 
  tft.setTextSize(2);
  tft.setTextColor(TFT_YELLOW); //set text color
  tft.fillScreen(TFT_BLACK);

  drawInitialScreen();  

  lcd_on = true;

  old_moisture_converted = convertRawInputToCalibratedValue(getSoilMoistureLevel());
  actual_moisture_converted = old_moisture_converted;
  drawMoistureLevel(target_moisture, actual_moisture_converted);
}

void loop() {
  actual_water_level = getWaterLevel();
  old_moisture_converted = actual_moisture_converted;
  actual_moisture_converted = convertRawInputToCalibratedValue(getSoilMoistureLevel());
  
  /* Enabled for debug
  SERIAL.print("moisture value = ");
  SERIAL.println(actual_moisture_converted);
  SERIAL.print("raw moisture reading = ");
  SERIAL.println(getSoilMoistureLevel());
  SERIAL.print("target value = ");
  SERIAL.println(target_moisture);
  */
  
  // Display logic
  if (actual_moisture_converted != old_moisture_converted) {
    updateActualMoistureLevel(actual_moisture_converted);
  }

  // Watering logic
  if (actual_water_level <= min_water_level) {
    if (out_of_water_display == false) {
      displayOutOfWater();
      out_of_water_display = true;
    }
    
    if (pump_on == true) {
      turnOffPump();
    }
  } else {
    if (out_of_water_display == true) {
      removeOutOfWaterMessage();
      out_of_water_display = false;
    }
    if (actual_moisture_converted > target_moisture) {
      if (pump_on == true) {
        turnOffPump();
      }
    } else {
      if (pump_on == false) {
        turnOnPump();
      }
    }
  }
  delay(5000);
}
