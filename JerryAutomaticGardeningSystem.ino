#include <LiquidCrystal.h>    // LCD control

#include <Time.h>             // System time

#include "DHT.h"              // Thermometer+Humidity sensor

#include <SparkFunTSL2561.h>  // Light Lux sensor
#include <Wire.h>

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

#define CHWatering  3   // Connect Digital Pin  on Arduino to CHWatering on Relay Module
#define CHThermo   12   // Connect Digital Pin 
#define CHLight     2   // Connect Digital Pin  on Arduino to CH3 on Relay Module
//#define LCDBacklight 13   // TODO: Not able to do it with current sainSmart LCD board, it's using vcc instead of digital pin for power supply

// SFE_TSL2561
SFE_TSL2561 luxSensor;
boolean luxGain;                      // Gain setting, 0 = X1, 1 = X16;
unsigned int luxDelay = 0;  // Integration ("shutter") time in milliseconds

// Thermometer+Humidity
#define DHTTYPE DHT11   // DHT 11
DHT dht(CHThermo, DHTTYPE);

// Button Index
#define buttonUp     1
#define buttonDown   2
#define buttonRight  0
#define buttonLeft   3
#define buttonSelect 4

// Keys
int adc_key_val[5] = {30, 150, 360, 535, 760 };
int NUM_KEYS = 5;
int adc_key_in;
int key = -1;
int oldkey = -1;

// Menu Index
int mainMenuIndex = 0;
int totalMenuItems = 4;

// Sub-Menu Index
int subMenuWateringIndex = 0;
int subMenuAirIndex = 0;
int subMenuLightIndex = 0;

// Init
int initDuration = 5;
bool firstSession = true;

// Status
bool isWatering = false;
bool isAiring = false;

// Water pump
unsigned long lastWaterPumpOnTime = 0;
#define kWateringOn  0
#define kWateringOff 1
unsigned long wateringMenuList[] = {0,
                                    0
                                   };
String wateringMenuStrList[] = {"ON Duration", "OFF Duration"};

// Air pump
unsigned long lastAirPumpOnTime = 0;
#define kAiringOn  0
#define kAiringOff 1
unsigned long airMenu[] = {0,
                           0
                          };
String airMenuStr[] = {"ON Duration", "OFF Duration"};

// Light
//int lightOnTime = 500;   // 5:00 am
//int lightOffTime = 2000; // 10:00 pm
#define kLightOnTime  0
#define kLightOffTime 1
unsigned long lightMenuList[] = {0,    // 5:00 am
                                 0    // 10:00 pm
                                };
String lightMenuStrList[] = {"ON Time", "OFF Time"};

double lightSwitchLuxThreshold = 4000;

// Relay Sign
#define relay_On  0
#define relay_Off 1

// Timer
unsigned long timeRef = 0;

// System Time Settings
int menuTimeIndex = 0;
bool isSettingTime = true;
int systemTimeList[] = {1, 1, 2016, 10, 0};
String systemTimeListStr[] = {"Month", "Day", "Year", "Hour ", "Minute "};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup()
{
  Serial.println(F("System Reset"));

  Serial.begin(9600);

  // Setup all the Pins
  pinMode(CHWatering, OUTPUT);  // Water pump
  pinMode(CHThermo, OUTPUT);  // Air pump
  pinMode(CHLight, OUTPUT);  // Light
  //pinMode(LCDBacklight, OUTPUT);

  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);

  lcd.clear();

  // Set init state
  turnOnWaterPump();
  turnOnAirPump();
  turnOnLight();

  setupLightLuxSensor();

  dht.begin();

  lastWaterPumpOnTime = millis();
  lastAirPumpOnTime = millis();
}


void loop()
{
  // set the cursor to column 0, line 1. (note: line 1 is the second row, since counting begins with 0):
  lcd.setCursor(0, 1);

  // System Init
  if (!isSystemTimeSet() && oldkey == -1) {
    showTimeSettings();
  }

  // Set default values
  if (firstSession) {

    Serial.println(F("First session"));
    Serial.println(F("//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n"));

    // Default duration values
    int defaultDurationOn = 30;
    int defaultDurationOff = 10;

    wateringMenuList[kWateringOn] = setMin(defaultDurationOn);
    wateringMenuList[kWateringOff] = setMin(defaultDurationOff);

    airMenu[kAiringOn] = setMin(defaultDurationOn);
    airMenu[kAiringOff] = setMin(defaultDurationOff);

    lightMenuList[kLightOnTime] = setHr(5) * 100 + setMin(0);
    lightMenuList[kLightOffTime] = setHr(20) * 100 + setMin(0);

    firstSession = false;
  }

  // Keys
  menuKeyAction();

  // updates - every 1 min
  if ((millis() - timeRef) > setMin(1)) {

    Serial.print(F("Time(HHMM): "));
    Serial.println(formattedCurrentTime());

    // set up systems
    toggleWaterPump();
    toggleAirPump();

    timeRef = millis();

    // Light
    switchLight();

    // Temperature+Humidity
    printTemperature();

    // switch back to home menu (currently clock)
    selectMainMenu(0);

    // update clock
    if (mainMenuIndex == 0) {
      if (isSystemTimeSet()) {
        printHomeMenuStatus();
      }
      else {
        adjustTimeSettings(key);
      }
    }

    // Turn off display after 1 min
    //    lcd.noDisplay();  //FIXME: suppose turn off the backlight, but this only turn off the display

    Serial.println(F("//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n"));
  }
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void menuKeyAction()
{
  // Keys
  adc_key_in = analogRead(0); // read the value from the sensor
  key = get_key(adc_key_in); // convert into key press
  //  if (key != oldkey) // if keypress is detected
  {
    delay(200); // wait for debounce time
    adc_key_in = analogRead(0); // read the value from the sensor
    key = get_key(adc_key_in); // convert into key press
    //    if (key != oldkey)
    {
      if (key >= 0)
      {
        //        lcd.display();

        // reset timeRef if user is interacting with device
        timeRef = millis();

        oldkey = key;

        if (!isSystemTimeSet()) {
          adjustTimeSettings(key);
        }
        else
        {
          switch (key) {
            case buttonRight:
              subMenuSelection(1);
              break;

            case buttonUp:
              menuAction(true);
              break;

            case buttonDown:
              menuAction(false);
              break;

            case buttonLeft:
              subMenuSelection(-1);
              break;

            case buttonSelect:
              selectMainMenu(loopItems(mainMenuIndex, totalMenuItems, 1));
              break;

            default:
              break;
          }
        }
      }
    }
  }
}

// Convert ADC value to key number
int get_key(unsigned int input)
{
  int k;

  for (k = 0; k < NUM_KEYS; k++) {
    if (input < adc_key_val[k]) {
      return k;
    }
  }

  if (k >= NUM_KEYS) {
    k = -1; // No valid key pressed
  }

  return k;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
   Menu Selection
*/

void selectMainMenu(int selectIndex)
{
  mainMenuIndex = selectIndex;

  lcd.clear();

  lcd.setCursor(0, 0);

  if (isSettingTime) {
    saveTimeSetting();
    mainMenuIndex = 0;
    return;
  }

  switch (selectIndex) {

    // Current Time
    case 0:
      printHomeMenuStatus();
      break;

    // Water Pump
    case 1:
      lcd.print("Water Pump");
      delay(1000);
      printWateringTime();
      break;

    // Air Pump
    case 2:
      lcd.print("Air Pump");
      delay(1000);
      printAirTime();
      break;

    // Light On/Off Time
    case 3:
      lcd.print("Light Source");
      delay(1000);
      printLightTime();
      break;

    default:
      break;
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
   System Time
*/

bool isSystemTimeSet()
{
  if (timeStatus() == 0) {
    return false;
  }

  return true;
}

void adjustTimeSettings(int buttonIndex)
{
  isSettingTime = true;

  int totalItems = (sizeof(systemTimeList) / sizeof(int));

  switch (buttonIndex) {

    case buttonRight: // Right
      menuTimeIndex = loopItems(menuTimeIndex, totalItems, 1);
      showTimeSettings();
      break;

    case buttonLeft: // Left
      menuTimeIndex = loopItems(menuTimeIndex, totalItems, -1);
      showTimeSettings();
      break;

    case buttonUp: // Up
      setTimeValue(systemTimeList[menuTimeIndex], 1);
      showTimeSettings();
      break;

    case buttonDown: // Down
      setTimeValue(systemTimeList[menuTimeIndex], -1);
      showTimeSettings();
      break;

    case buttonSelect:
    default:
      // Save time setting if hits the last object index
      if (menuTimeIndex == totalItems - 1) {
        saveTimeSetting();
      }
      else {
        menuTimeIndex = loopItems(menuTimeIndex, totalItems, 1);
        showTimeSettings();
      }
      break;
  }
}

void setTimeValue(int timeValue, int deltaValue)
{
  int newValue = timeValue + deltaValue;

  switch (menuTimeIndex) {

    // Month
    case 0:
      if (newValue < 1) {
        newValue = 12;
      }
      else if (newValue > 12) {
        newValue = 1;
      }
      break;

    // Day
    case 1:
      if (newValue < 1) {
        newValue = 1;
      }
      // TODO: determine the day of the month
      break;

    // Year
    case 2:
      if (newValue < 1970) {
        newValue = 1970;
      }
      break;

    // Hour
    case 3:
      if (newValue < 0) {
        newValue = 23;
      }
      else if (newValue >= 24) {
        newValue = 0;
      }
      break;

    // Minute
    case 4:
      if (newValue <= 0) {
        newValue = 59;
      }
      else if (newValue >= 59) {
        newValue = 0;
      }
      break;

    default:
      break;
  }

  systemTimeList[menuTimeIndex] = newValue;
}

void showTimeSettings()
{
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Set ");
  lcd.print(systemTimeListStr[menuTimeIndex]);

  lcd.setCursor(0, 1);
  lcd.print(systemTimeList[menuTimeIndex]);
}

void saveTimeSetting()
{
  //setTime(hour, minute, second, day, month, year);
  setTime(systemTimeList[3], systemTimeList[4], 0, systemTimeList[1], systemTimeList[0], systemTimeList[2]);

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(F("Time Saved!!"));
  Serial.println(F("Time Saved!!"));
  isSettingTime = false;
  menuTimeIndex = 0;
  delay(1000);

  printHomeMenuStatus();
}

void printHomeMenuStatus()
{
  lcd.clear();

  lcd.setCursor(0, 0);
  if (isWatering) {
    // TODO: blink the LCD to indicate the action in progress for fun!
    lcd.print(F("Watering Plants..."));
  }
  else {
    lcd.print(F("Current Time"));
  }

  lcd.setCursor(0, 1);
  printClockTime();
}

void printClockTime()
{
  if (!isSystemTimeSet()) {
    return;
  }

  time_t t = now();
  lcd.print(month(t));
  lcd.print("/");
  lcd.print(day(t));
  lcd.print("/");
  lcd.print(year(t));
  lcd.print(" ");
  int hrValue = hour(t);
  if (hrValue < 10) {
    lcd.print("0");
    lcd.print(hrValue);
  }
  else {
    lcd.print(hour(t));
  }
  lcd.print(":");
  int minValue = minute(t);
  if (minValue < 10) {
    lcd.print("0");
    lcd.print(minValue);
  }
  else {
    lcd.print(minute(t));
  }
}

long formattedCurrentTime() {
  time_t t = now();
  return hour(t) * 100 + minute(t);
}

long formattedCurrentTime(long timeValuems) {
  time_t t = timeValuems;
  return hour(t) * 100 + minute(t);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
   Menu Action
*/

// Sub-menu (RIGHT and LEFT keys)
void subMenuSelection(int indexChange)
{
  switch (mainMenuIndex) {

    // Home Menu - Current time
    case 0:
      //      printHomeMenuStatus();
      isSettingTime = true;
      adjustTimeSettings(key);
      break;

    // Water pump
    case 1:
      switchWateringMenu(indexChange);
      break;

    // Air pump
    case 2:
      switchAirMenu(indexChange);
      break;

    // Light Source
    case 3:
      switchLightMenu(indexChange);
      break;

    default:
      break;
  }
}

// UP and DOWN keys
void menuAction(bool up)
{
  switch (mainMenuIndex) {

    // Home Menu - Current Time
    case 0:
      adjustTimeSettings(key);
      break;

    // Water pump
    case 1:
      if (up) {
        addWateringTime(setMin(1));
      }
      else {
        addWateringTime(setMin(-1));
      }
      printWateringTime();
      break;

    // Air pump
    case 2:
      if (up) {
        addAirTime(setMin(1));
      }
      else {
        addAirTime(setMin(-1));
      }
      printAirTime();
      break;

    // Light Source
    case 3:
      if (up) {
        addLightTime(setMin(15));
      }
      else {
        addLightTime(setMin(-15));
      }
      printLightTime();
      break;

    default:
      break;
  }
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
   Watering
*/

void switchWateringMenu(int directionDelta)
{
  int totalItems = (sizeof(wateringMenuList) / sizeof(long));

  subMenuWateringIndex = loopItems(subMenuWateringIndex, totalItems, directionDelta);

  printWateringTime();
}

void printWateringTime() {
  printMenuSettings(wateringMenuStrList, wateringMenuList, subMenuWateringIndex);
}

void addWateringTime(long timeValue) {
  addTime(wateringMenuList, subMenuWateringIndex, timeValue);
}

void turnOnWaterPump() {
  Serial.println(F("Water pump On"));
  digitalWrite(CHWatering, relay_On);
  isWatering = true;
}

void turnOffWaterPump() {
  Serial.println(F("Water pump Off"));
  digitalWrite(CHWatering, relay_Off);
  isWatering = false;
}

void toggleWaterPump()
{
  unsigned long timeLapsed = millis() - lastWaterPumpOnTime;

  if (!isWatering) {
    if (timeLapsed > wateringMenuList[kWateringOff]) {
      turnOnWaterPump();
      lastWaterPumpOnTime = millis();
    }
  }
  else {
    if (timeLapsed > wateringMenuList[kWateringOn]) {
      turnOffWaterPump();
      lastWaterPumpOnTime = millis();
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
   Airing
*/

void switchAirMenu(int directionDelta)
{
  int totalItems = (sizeof(airMenu) / sizeof(long));

  subMenuAirIndex = loopItems(subMenuAirIndex, totalItems, directionDelta);

  printAirTime();
}

void printAirTime() {
  printMenuSettings(airMenuStr, airMenu, subMenuAirIndex);
}

void addAirTime(long timeValue) {
  addTime(airMenu, subMenuAirIndex, timeValue);
}

void turnOnAirPump() {
  Serial.println(F("Air pump On"));
  digitalWrite(CHWatering, relay_On);
  isAiring = true;
}

void turnOffAirPump() {
  Serial.println(F("Air pump Off"));
  digitalWrite(CHWatering, relay_Off);
  isAiring = false;
}

void toggleAirPump()
{
  long timeLapsed = millis() - lastAirPumpOnTime;

  if (!isAiring) {
    if (timeLapsed > airMenu[kAiringOff]) {
      turnOnAirPump();
      lastAirPumpOnTime = millis();
    }
  }
  else {
    if (timeLapsed > airMenu[kAiringOn]) {
      turnOffAirPump();
      lastAirPumpOnTime = millis();
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
   Light
*/

void switchLightMenu(int directionDelta)
{
  int totalItems = (sizeof(lightMenuList) / sizeof(long));

  subMenuLightIndex = loopItems(subMenuLightIndex, totalItems, directionDelta);

  printLightTime();
}

void printLightTime() {
  printHHMM(lightMenuStrList, lightMenuList, subMenuLightIndex);
}

void printHHMM(String menuStr[], unsigned long menu[], int index)
{
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(menuStr[index]);

  lcd.setCursor(0, 1);
  lcd.print(convertTimeToHHMMString(menu[index]));
}

String convertTimeToHHMMString(long totalms)
{
  unsigned long totalSec = (long) (totalms / 1000);

  if (totalSec <= 0) {
    return String("Please Add Time");
  }

  long remainingValue = totalSec;
  long currentDay = totalSec / 24 / 60 / 60;
  remainingValue = (totalSec - currentDay * 24 * 60 * 60);
  long hr =  remainingValue / 60 / 60;
  remainingValue -= hr * 60 * 60;
  long minutes = remainingValue / 60;
  remainingValue -= minutes * 60;
  long sec = remainingValue;

  String timeStr = "";

  if (hr >= 10) {
    //    Serial.print(hr);
    //    Serial.print("h ");
    timeStr = String(timeStr + hr + ":");
  }
  else {
    timeStr = String(timeStr + "0" + hr + ":");
  }

  if (minutes >= 10) {
    //    Serial.print(minutes);
    //    Serial.print("m ");
    timeStr = String(timeStr + minutes);
  }
  else {
    timeStr = String(timeStr + "0" + minutes);
  }

  return timeStr;
}

void addLightTime(long timeValue) {
  addTime(lightMenuList, subMenuLightIndex, timeValue);
}

void switchLight()
{
  time_t timeNow = now();
  unsigned long currentTimeValue = setHr(hour(timeNow)) * 100 + setMin(minute(timeNow)) * 1000;

  // hourOn < hourOff. ex. 0:00 off -> 8:00 on -> 22:00 off
  if (currentTimeValue >= lightMenuList[kLightOnTime] && currentTimeValue < lightMenuList[kLightOffTime]) {

    double currentEnvironmentLuxValue = getLuxValue();
    // Save electricity by turn off the light if environment is bright enough
    if (currentEnvironmentLuxValue > lightSwitchLuxThreshold || currentEnvironmentLuxValue == 0) {
      Serial.println(F("Light saving mode"));
      turnOffLight();
    }
    else {
      turnOnLight();
    }
  }
  else {
    if (!isSettingTime) {
      turnOffLight();
    }
  }
}

void turnOnLight() {
  Serial.println(F("Light On"));
  digitalWrite(CHLight, relay_On);
}

void turnOffLight() {
  Serial.println(F("Light Off"));
  digitalWrite(CHLight, relay_Off);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
   Convenient Functions
*/

int loopItems(int index, int totalItems, int delta)
{
  int newIndex = index + delta;

  if (newIndex > totalItems - 1) {
    newIndex = 0;
  }
  else if (newIndex < 0) {
    newIndex = totalItems - 1;
  }

  return newIndex;
}

long setSec(long sec) {
  return sec * 1000;
}

long setMin(long minutes) {
  return minutes * 60 * 1000;
}

long setHr(long hr) {
  return hr * 60 * 60 * 1000;
}

String convertTimeToString(long totalms)
{
  unsigned long totalSec = (long) (totalms / 1000);

  if (totalSec <= 0) {
    return String("Please Add Time");
  }

  long remainingValue = totalSec;
  long currentDay = totalSec / 24 / 60 / 60;
  remainingValue = (totalSec - currentDay * 24 * 60 * 60);
  long hr =  remainingValue / 60 / 60;
  remainingValue -= hr * 60 * 60;
  long minutes = remainingValue / 60;
  remainingValue -= minutes * 60;
  long sec = remainingValue;

  String timeStr = "";

  if (currentDay > 0) {
    //    Serial.print(currentDay);
    //    Serial.print(" day ");
    timeStr = String(timeStr + currentDay + " day ");
  }
  if (hr > 0) {
    //    Serial.print(hr);
    //    Serial.print("h ");
    timeStr = String(timeStr + hr + "h ");
  }
  if (minutes > 0) {
    //    Serial.print(minutes);
    //    Serial.print("m ");
    timeStr = String(timeStr + minutes + "m ");
  }
  if (sec > 0) {
    //    Serial.print(sec);
    //    Serial.print("s ");
    timeStr = String(timeStr + sec + "s");
  }

  //  Serial.println("");

  return timeStr;
}

void printMenuSettings(String menuStr[], unsigned long menu[], int index)
{
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(menuStr[index]);

  lcd.setCursor(0, 1);
  lcd.print(convertTimeToString(menu[index]));
}

void addTime(unsigned long menu[], int index, long timeValue)
{
  if (menu[index] + timeValue >= 0) {
    menu[index] += timeValue;
  }
  else {
    menu[index] = 0;
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
  Lux
*/

void setupLightLuxSensor()
{
  // Initialize the SFE_TSL2561 library

  // You can pass nothing to luxSensor.begin() for the default I2C address (0x39), or use one of the following presets if you have changed
  // the ADDR jumper on the board:

  // TSL2561_ADDR_0 address with '0' shorted on board (0x29)
  // TSL2561_ADDR   default address (0x39)
  // TSL2561_ADDR_1 address with '1' shorted on board (0x49)

  // For more information see the hookup guide at: https://learn.sparkfun.com/tutorials/tsl2561-luminosity-sensor-hookup-guide?_ga=1.70644176.1938687081.1457811508

  luxSensor.begin();

  unsigned char ID;
  // Get factory ID from sensor:
  if (luxSensor.getID(ID)) {
    //    Serial.println("Lux Sensor - factory ID: 0X");
    //    Serial.print(ID, HEX);
    //    Serial.println(", should be 0X5X");
  }
  // Most library commands will return true if communications was successful and false if there was a problem. You can ignore this returned value, or check whether a command worked correctly and retrieve an error code:
  else {
    Serial.println(F("Error: failed to get lux sensor ID"));
    byte error = luxSensor.getError();
    printError(error);
  }

  // The light sensor has a default integration time of 402ms, and a default gain of low (1X).

  // If you would like to change either of these, you can do so using the setTiming() command.

  // If luxGain = false (0), device is set to low gain (1X)
  // If luxGain = high (1), device is set to high gain (16X)

  luxGain = 0;

  // If time = 0, integration will be 13.7ms
  // If time = 1, integration will be 101ms
  // If time = 2, integration will be 402ms
  // If time = 3, use manual start / stop to perform your own integration

  unsigned char time = 2;

  // setTiming() will set the third parameter (ms) to the requested integration time in ms (this will be useful later):

  luxSensor.setTiming(luxGain, time, luxDelay);

  // To start taking measurements, power up the sensor:

  luxSensor.setPowerUp();

  // The sensor will now gather light during the integration time.
  // After the specified time, you can retrieve the result from the sensor.
  // Once a measurement occurs, another integration period will start.

  getLuxValue();
}

double getLuxValue()
{
  Serial.print(F("getLuxValue. "));

  // Wait between measurements before retrieving the result (You can also configure the sensor to issue an interrupt when measurements are complete)
  // This sketch uses the TSL2561's built-in integration timer.
  // You can also perform your own manual integration timing by setting "time" to 3 (manual) in setTiming(),
  // then performing a manualStart() and a manualStop() as in the below commented statements:

  // luxDelay = 1000;
  // luxSensor.manualStart();
  // delay(luxDelay);
  // luxSensor.manualStop();

  // Once integration is complete, we'll retrieve the data.
  // There are two light sensors on the device, one for visible light and one for infrared. Both sensors are needed for lux calculations.
  // Retrieve the data from the device:
  unsigned int data0, data1;

  // getData() returned true, communication was successful
  if (luxSensor.getData(data0, data1))
  {
    //    Serial.print("data0: ");
    //    Serial.print(data0);
    //    Serial.print(" data1: ");
    //    Serial.print(data1);
    //    Serial.print("   ");

    // To calculate lux, pass all your settings and readings to the getLux() function.

    // The getLux() function will return 1 if the calculation was successful, or 0 if one or both of the sensors was saturated (too much light).
    // If this happens, you can reduce the integration time and/or gain.
    // For more information see the hookup guide at: https://learn.sparkfun.com/tutorials/getting-started-with-the-tsl2561-luminosity-sensor

    double lux;    // Resulting lux value
    boolean good;  // True if neither sensor is saturated

    // Perform lux calculation:
    good = luxSensor.getLux(luxGain, luxDelay, data0, data1, lux);

    // Print out the results:
    Serial.print(F("Lux: "));
    Serial.println(lux);

    if (!good) {
      // if sensor saturated
      if (data0 > 20000 || data1 > 10000) {
        Serial.println(F("Light lux sensor is saturated."));
        return 10000;
      }
      else {
        Serial.println(F("ERROR!!! Light lux sensor is not working properly."));
      }
    }
    else {
      return lux;
    }
  }
  else {
    Serial.println(F("ERROR!!! Light lux sensor is not working properly."));
    // getData() returned false because of an I2C error, inform the user.
    byte error = luxSensor.getError();
    printError(error);
    return 0.0;
  }
}

// If there's an I2C error, this function will print out an explanation.
void printError(byte error)
{
  Serial.print(F("I2C error: "));
  Serial.print(error, DEC);
  Serial.print(", ");

  switch (error)
  {
    case 0:
      Serial.println(F("success"));
      break;
    case 1:
      Serial.println(F("data too long for transmit buffer"));
      break;
    case 2:
      Serial.println(F("received NACK on address (disconnected?)"));
      break;
    case 3:
      Serial.println(F("received NACK on data"));
      break;
    case 4:
      Serial.println(F("other error"));
      break;
    default:
      Serial.println(F("unknown error"));
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void printTemperature()
{
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();

  // Check if any reads failed and exit early (to try again).
  if (isnan(h)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
  else {

    Serial.print(F("Humidity: "));
    Serial.print(h);
    Serial.print("%\t");

    // Read temperature as Celsius (the default)
    float t = dht.readTemperature();
    if (isnan(t)) {
      Serial.println(F("Failed to read from DHT sensor!"));
    }
    else {
      // Compute heat index in Celsius (isFahreheit = false)
      float hic = dht.computeHeatIndex(t, h, false);

      Serial.print(F("Temperature: "));
      Serial.print(t);
      Serial.print("C \t");

      Serial.print(F("Heat index: "));
      Serial.print(hic);
      Serial.println("C ");
    }

    //    // Read temperature as Fahrenheit (isFahrenheit = true)
    //    float f = dht.readTemperature(true);
    //    if (isnan(f)) {
    //      Serial.println("Failed to read from DHT sensor!");
    //    }
    //    else {
    //      // Compute heat index in Celsius (isFahreheit = false)
    //      float hif = dht.computeHeatIndex(f, h);
    //
    //      Serial.print("Temperature: ");
    //      Serial.print(f);
    //      Serial.print("F\t");
    //
    //      Serial.print("Heat index: ");
    //      Serial.print(hif);
    //      Serial.println("F");
    //    }
  }
}


