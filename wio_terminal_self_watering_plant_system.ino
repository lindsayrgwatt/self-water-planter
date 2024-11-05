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

// Debounce interrupts
#define DEBOUNCE_DELAY 50
//#define HOLD_DURATION 100

#define INCREMENT 5 // Target moisture level increments

bool serialEnabled = true;

// Two variables below used for determining water depth
unsigned char low_data[8] = {0};
unsigned char high_data[12] = {0};

volatile int target_moisture = 20; /* % between 0-100 */
volatile int current_moisture_level = 0;
volatile int old_moisture_level = 0;

// Debounce interrupts & buttons
volatile unsigned long lastInterruptTime = 0;
unsigned long lastDebounceTime = 0;
volatile bool buttonPressed = false;
volatile unsigned long pressStartTime = 0;

// Reduce thrash
// Introduce hysteresis so relay won't thrash on/off around target_moisture. Overshoot
// Use counters so that don't turn on pump if there is a spurious sensor reading
int hysteresis = 3;
int consecutiveLowReadings = 0;
int consecutiveHighReadings = 0;
const int requiredConsecutiveReadings = 3;

int min_water_level = 25;
int actual_water_level = 0;

bool out_of_water = false;
bool out_of_water_display = false;
bool pump_on = false;
bool lcd_on = false;
bool update_target = true;

// Non-blocking delay variables
unsigned long previousMillis = 0;
const long interval = 1000;

// Display
TFT_eSPI tft; //initialize TFT LCD 

void setupSensors() {
  pinMode(0, OUTPUT); // Relay for pump
  pinMode(6, INPUT); // Moisture sensor
  pinMode(WIO_KEY_A, INPUT); // Button A
}

void initializeDisplay() {
  tft.begin(); //start TFT LCD 
  tft.setRotation(3); //set screen rotation 
  tft.setTextSize(2);
  tft.setTextColor(TFT_YELLOW); //set text color
  tft.fillScreen(TFT_RED);

  // Empty screen
  tft.setTextSize(3);
  tft.drawString("Moisture Level", 40,90);
  tft.setTextSize(2);
  tft.drawString("Target", 60, 140);
  tft.drawString("Actual", 190, 140);
}

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
  int input;
  input = analogRead(6); //connect sensor to Analog 1

  if (serialEnabled) {
    SERIAL.print("raw moisture reading = ");
    SERIAL.println(input);
  }

  int air = 780; // From testing, air immersion value: 300
  int water = 425; // From testing, water immersion value: 250
  int converted;

  if (input >= air) {
    converted = 0;
  } else if (input <= water) {
    converted = 99;
  } else {
    converted = 100 * (air - input)/(air - water);
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
  if (lcd_on == true) {
    unsigned long currentTime = millis();
    if (currentTime - lastInterruptTime > DEBOUNCE_DELAY) {
      lastInterruptTime = currentTime;
      target_moisture += INCREMENT;
      if (target_moisture >= 95) {
        target_moisture = 95; // 100% throws off formatting
      }
      update_target = true;
    }
  }
}

void decreaseTargetMoistureLevel() {
  if (lcd_on == true) {
      unsigned long currentTime = millis();
      if (currentTime - lastInterruptTime > DEBOUNCE_DELAY) {
        lastInterruptTime = currentTime;
        target_moisture -= INCREMENT;
        if (target_moisture <= 10) {
          target_moisture = 10; // <10% throws off formatting
        }
        update_target = true;
      }
    }
  }

void toggleLCDBacklight() {
  if (lcd_on == true) {
    lcd_on = false;
  } else {
    lcd_on = true;
  }
}

void updateTarget(int target) {
  tft.setTextSize(5);
  tft.fillRect(55,170,59,70, TFT_RED);
  tft.drawNumber(target, 55, 170);
  tft.setTextSize(4);
  tft.drawString("%", 115, 177);
}

void updateDisplay(int target, int actual, int old) { 
  if (update_target == true) {
    updateTarget(target);
    update_target = false;
  }
  if (actual != old) {
    tft.setTextSize(5);
    tft.fillRect(185,170,59,70, TFT_RED);
    tft.drawNumber(actual, 185, 170);
    tft.setTextSize(4);
    tft.drawString("%", 245, 177);
  }

  tft.setTextSize(2);

  if (out_of_water_display == true) {
    tft.drawString("out of water", 5, 5);
  } else {
    tft.fillRect(0, 0, 320, 25, TFT_RED);
  }

  if (lcd_on == true) {
    digitalWrite(LCD_BACKLIGHT, HIGH);
  } else {
    digitalWrite(LCD_BACKLIGHT, LOW);
  }
}

void handleSensorsAndLogic() {
  actual_water_level = getWaterLevel();
  current_moisture_level = getSoilMoistureLevel();
  
  if (serialEnabled) {
    SERIAL.print("water level = ");
    SERIAL.print(actual_water_level);
    SERIAL.print(" vs. target = ");
    SERIAL.println(min_water_level);

    SERIAL.print("moisture value = ");
    SERIAL.print(current_moisture_level);
    SERIAL.print(" vs. target value = ");
    SERIAL.print(target_moisture);
    SERIAL.print(" and hysteresis = ");
    SERIAL.println(hysteresis);

    SERIAL.print("consecutive low readings = ");
    SERIAL.print(consecutiveLowReadings);
    SERIAL.print(" vs. threshold of ");
    SERIAL.println(requiredConsecutiveReadings);

    SERIAL.print("Pump is ");
    SERIAL.println(pump_on);
  }

  if (actual_water_level <= min_water_level) {
    if (lcd_on == false) {
      lcd_on = true; // Force light on
    }
    if (out_of_water_display == false) {
      out_of_water_display = true;
    }    
    if (pump_on == true) {
      turnOffPump();
    }
  } else {
    if (out_of_water_display == true) {
      out_of_water_display = false;
    }

    if (pump_on == true) {
      if (current_moisture_level > (target_moisture + hysteresis)) {
        consecutiveHighReadings++;
        if (consecutiveHighReadings >= requiredConsecutiveReadings) {
          turnOffPump();
          consecutiveHighReadings = 0;
        }
        consecutiveLowReadings = 0;
      } else {
        consecutiveHighReadings = 0;
      }
    } else {
      if (current_moisture_level < target_moisture) {
        // Increment the counter if the moisture level is below the target
        consecutiveLowReadings++;
        if (consecutiveLowReadings >= requiredConsecutiveReadings) {
          turnOnPump();
        }
        consecutiveHighReadings = 0;
      } else {
        // Reset counter if the moisture level is above the target
        consecutiveLowReadings = 0;
      }
    }
  }
}

void setup() {
  if (serialEnabled) {
       Serial.begin(115200);
     }

  Wire.begin();

  setupSensors();
  
  attachInterrupt(digitalPinToInterrupt(WIO_5S_RIGHT), increaseTargetMoistureLevel, RISING);
  attachInterrupt(digitalPinToInterrupt(WIO_5S_LEFT), decreaseTargetMoistureLevel, RISING);
  
  initializeDisplay();
  
  current_moisture_level = getSoilMoistureLevel();
  old_moisture_level = -1;

  lcd_on = true;
  update_target = true;
}

void loop() {
  //while(!Serial); // Wait for Serial to be ready
  
  updateDisplay(target_moisture, current_moisture_level, old_moisture_level);
  old_moisture_level = current_moisture_level;

  unsigned long currentMillis = millis();

  if (digitalRead(WIO_KEY_A) == LOW && !buttonPressed) {
      unsigned long currentTime = millis();
      if ((currentTime - lastDebounceTime) > DEBOUNCE_DELAY) {
          buttonPressed = true;
          lastDebounceTime = currentTime;
          toggleLCDBacklight();
          Serial.println("Button A pressed");
      }
  }

  // Reset button state after the debounce delay
  if (buttonPressed && (millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
      if (digitalRead(WIO_KEY_A) == HIGH) {
          buttonPressed = false;
      }
  }

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    handleSensorsAndLogic();
  }
}
