#include <Wire.h>

#ifdef ARDUINO_SAMD_VARIANT_COMPLIANCE
#define SERIAL SerialUSB
#else
#define SERIAL Serial
#endif

#define NO_TOUCH       0xFE
#define THRESHOLD      100
#define ATTINY1_HIGH_ADDR   0x78
#define ATTINY2_LOW_ADDR   0x77

// Two variables below used for determining water depth
unsigned char low_data[8] = {0};
unsigned char high_data[12] = {0};

int target_moisture = 700;
int actual_moisture = 0;

int min_water_level = 25;
int actual_water_level = 0;

bool out_of_water = false;
bool pump_on = false;

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

void turnOnPump() {
  SERIAL.println("Inside turnOnPump");
  digitalWrite(0, HIGH);
  SERIAL.print("Value of pump_on before change: ");
  SERIAL.println(pump_on);
  pump_on = true;
  SERIAL.print("Value of pump_on after change: ");
  SERIAL.println(pump_on);

}

void turnOffPump() {
  SERIAL.println("Inside turnOffPump");
  digitalWrite(0, LOW);
  SERIAL.print("Value of pump_on before change: ");
  SERIAL.println(pump_on);
  pump_on = false;
  SERIAL.print("Value of pump_on after change: ");
  SERIAL.println(pump_on);
}

void setup() {
  SERIAL.begin(115200);
  Wire.begin();

  pinMode(0, OUTPUT); // Relay for pump
  pinMode(6, INPUT); // Moisture sensor
}

void loop() {
  actual_water_level = getWaterLevel();
  SERIAL.print("water level = ");
  SERIAL.print(actual_water_level);
  SERIAL.print(" min water level = ");
  SERIAL.println(min_water_level);
  SERIAL.print("pump_on = ");
  SERIAL.println(pump_on);
  if (actual_water_level <= min_water_level) {
    SERIAL.println("out of water");
    if (pump_on == true) {
      SERIAL.println("Turn off pump");
      turnOffPump();
    }
  } else {
    actual_moisture = getSoilMoistureLevel();
    SERIAL.print("moisture level = ");
    SERIAL.print(actual_moisture);
    SERIAL.print(" target moisture level = ");
    SERIAL.println(target_moisture);
    SERIAL.print("pump_on = ");
    SERIAL.println(pump_on);
  
    // Remember: sensor sees value of 250 when wet and 300 when dry    
    if (actual_moisture > target_moisture) {
      SERIAL.println("Too dry");
      if (pump_on == false) {
        SERIAL.println("Turn on pump");
        turnOnPump();
      } else {
        SERIAL.println("Pump is already on");
      }
    } else {
      SERIAL.println("Sufficiently moist");
      if (pump_on == true) {
        SERIAL.println("Turn off pump");
        turnOffPump();
      } else {
        SERIAL.println("Pump is already off");
      }
    }
  }
  delay(5000);
  /*
  //turnOnPump();
  //delay(1000);
  //turnOffPump();
  delay(1000);
  actual_moisture = getSoilMoistureLevel();
  SERIAL.print("moisture level = ");
  SERIAL.print(actual_moisture);
  SERIAL.print(" target moisture level = ");
  SERIAL.println(target_moisture);
  */  
}
