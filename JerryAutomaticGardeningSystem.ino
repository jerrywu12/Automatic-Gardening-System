#include <LiquidCrystal.h>
#include <Time.h>
#include "DHT.h"    // Thermometer+Humidity sensors

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

#define CHWatering  3   // Connect Digital Pin  on Arduino to CHWatering on Relay Module
#define CHThermo   12   // Connect Digital Pin 
#define CHLight     2   // Connect Digital Pin  on Arduino to CH3 on Relay Module
//#define LCDBacklight 13   // TODO: Not able to do it with current sainSmart LCD board, it's using vcc instead of digital pin for power supply

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
long lastWaterPumpOnTime = 0;
long waterPumpOnDuration = 0;
long waterPumpOffDuration = 0;
long waterPumpOnDurationDay = 0;
long waterPumpOffDurationDay = 0;
long waterPumpOnDurationNight = 0;
long waterPumpOffDurationNight = 0;
#define kWateringOn  0
#define kWateringOff 1
long wateringMenu[] = {waterPumpOnDuration,
                       waterPumpOffDuration
                      };
String wateringMenuStr[] = {"ON Duration", "OFF Duration"};

// Air pump
long lastAirPumpOnTime = 0;
long airPumpOnDuration = 0;
long airPumpOffDuration = 0;
long airPumpOnDurationDay = 0;
long airPumpOffDurationDay = 0;
long airPumpOnDurationNight = 0;
long airPumpOffDurationNight = 0;
#define kAiringOn  0
#define kAiringOff 1
long airMenu[] = {airPumpOnDuration,
                  airPumpOffDuration
                 };
String airMenuStr[] = {"ON Duration", "OFF Duration"};

// Light
int lightOnTime = 500;   // 5:00
int lightOffTime = 2200; // 10:00

// Relay Sign
#define relay_On  0
#define relay_Off 1

// Timer
unsigned long timeRef = 0;

// Default System Time
int currentMonth = 1;
int currentDay = 1;
int currentYear = 2016;
int currentHour = 10;
int currentMinute = 00;

// System Time Settings
int menuTimeIndex = 0;
int menuSystemTimeList[] = {currentMonth, currentDay, currentYear, currentHour, currentMinute};
String menuSystemTimeListStr[] = {"Month", "Day", "Year", "Hour ", "Minute "};


void setup() {

  Serial.println("System Reset");

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

  dht.begin();

  lastWaterPumpOnTime = millis();
  lastAirPumpOnTime = millis();

  waterPumpOnDurationNight = setMin(2);
  waterPumpOffDurationNight = setHr(2);

  airPumpOnDurationNight = setMin(2);
  airPumpOffDurationNight = setHr(2);
}


void loop() {

  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  lcd.setCursor(0, 1);

  // System Init
  if (!isSystemTimeSet() && oldkey == -1) {
    showTimeSettings();
  }

  // Set Default duration for timers
  if (firstSession) {

    Serial.println("first session");

    int durationOn = 3;
    int durationOff = 30;

    wateringMenu[kWateringOn] = waterPumpOnDurationDay = setMin(durationOn);
    wateringMenu[kWateringOff] = waterPumpOffDurationDay = setMin(durationOff);

    airMenu[kAiringOn] = airPumpOnDurationDay = setMin(durationOn);
    airMenu[kAiringOff] = airPumpOffDurationDay = setMin(durationOff);

    firstSession = false;
  }

  // Night Time Override - 11pm ~ 8am
  //  if (currentHour < 8 || currentHour > 23) {
  //
  //    wateringMenu[kWateringOn] = waterPumpOnDurationNight;
  //    wateringMenu[kWateringOff] = waterPumpOffDurationNight;
  //
  //    airMenu[kAiringOn] = airPumpOnDurationNight;
  //    airMenu[kAiringOff] = airPumpOffDurationNight;
  //  }
  //  else {
  //
  //    wateringMenu[kWateringOn] = waterPumpOnDurationDay;
  //    wateringMenu[kWateringOff] = waterPumpOffDurationDay;
  //
  //    airMenu[kAiringOn] = airPumpOnDurationDay;
  //    airMenu[kAiringOff] = airPumpOffDurationDay;
  //  }

  // set up systems
  toggleWaterPump();
  toggleAirPump();

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
          loopTimeSettings(key);
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
              selectMainMenu(loopItems(mainMenuIndex, 3, 1));
              break;

            default:
              break;
          }
        }
      }
    }
  }

  // update clock every 1 minute
  if ((millis() - timeRef) > setMin(1)) {

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
        loopTimeSettings(key);
      }
    }

    // Turn off display after 1 min
    //    lcd.noDisplay();  //FIXME: suppose turn off the backlight, but this only turn off the display
  }
}

void printTemperature()
{
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();

  // Check if any reads failed and exit early (to try again).
  if (isnan(h)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  else {

    Serial.print("Humidity: ");
    Serial.print(h);
    Serial.print("%\t");

    // Read temperature as Celsius (the default)
    float t = dht.readTemperature();
    if (isnan(t)) {
      Serial.println("Failed to read from DHT sensor!");
    }
    else {
      // Compute heat index in Celsius (isFahreheit = false)
      float hic = dht.computeHeatIndex(t, h, false);

      Serial.print("Temperature: ");
      Serial.print(t);
      Serial.print("C \t");

      Serial.print("Heat index: ");
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

void printHumidity()
{
  float h = dht.readHumidity();

  // Check if any reads failed and exit early (to try again).
  if (isnan(h)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.print("%\t");
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

/*
   Menu Selection
*/

void selectMainMenu(int selectIndex)
{
  mainMenuIndex = selectIndex;

  lcd.clear();

  lcd.setCursor(0, 0);

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

    default:
      break;
  }
}

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

void loopTimeSettings(int buttonIndex)
{
  int totalItems = (sizeof(menuSystemTimeList) / sizeof(int));

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
      menuSystemTimeList[menuTimeIndex] = setTimeValue(menuSystemTimeList[menuTimeIndex], 1);
      showTimeSettings();
      break;

    case buttonDown: // Down
      menuSystemTimeList[menuTimeIndex] = setTimeValue(menuSystemTimeList[menuTimeIndex], -1);
      showTimeSettings();
      break;

    case buttonSelect:
    default:
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

int setTimeValue(int timeValue, int deltaValue)
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
      currentMonth = newValue;
      break;

    // Day
    case 1:
      if (newValue < 1) {
        newValue = 1;
      }
      // TODO: determine the day of the month
      currentDay = newValue;
      break;

    // Year
    case 2:
      if (newValue < 1970) {
        newValue = 1970;
      }
      currentYear = newValue;
      break;

    // Hour
    case 3:
      if (newValue <= 0) {
        newValue = 23;
      }
      else if (newValue >= 24) {
        newValue = 0;
      }
      currentHour = newValue;
      break;

    // Minute
    case 4:
      if (newValue <= 0) {
        newValue = 59;
      }
      else if (newValue >= 59) {
        newValue = 0;
      }
      currentMinute = newValue;
      break;

    default:
      break;
  }

  return newValue;
}

void showTimeSettings()
{
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Set ");
  lcd.print(menuSystemTimeListStr[menuTimeIndex]);

  lcd.setCursor(0, 1);
  lcd.print(menuSystemTimeList[menuTimeIndex]);
}

void saveTimeSetting()
{
  setTime(currentHour, currentMinute, 0, currentDay, currentMonth, currentYear);

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Time Saved!!");
  delay(1000);

  printHomeMenuStatus();
}

void printHomeMenuStatus()
{
  lcd.clear();

  lcd.setCursor(0, 0);
  if (isWatering) {
    // TODO: blink the LCD to indicate the action in progress for fun!
    lcd.print("Watering Plants...");
  }
  else {
    lcd.print("Current Time");
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

// Sub-menu
void subMenuSelection(int indexChange)
{
  switch (mainMenuIndex) {

    // Home Menu - Current time
    case 0:
      printHomeMenuStatus();
      break;

    // Water pump
    case 1:
      switchWateringMenu(indexChange);
      break;

    // Air pump
    case 2:
      switchAirMenu(indexChange);
      break;

    default:
      break;
  }
}

void menuAction(bool up)
{
  switch (mainMenuIndex) {

    // Home Menu - Current Time
    case 0:
      printHomeMenuStatus();
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

    default:
      break;
  }
}


/*
   Watering
*/

void switchWateringMenu(int directionDelta)
{
  int totalItems = (sizeof(wateringMenu) / sizeof(long));

  subMenuWateringIndex = loopItems(subMenuWateringIndex, totalItems, directionDelta);

  printWateringTime();
}

void printWateringTime()
{
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(wateringMenuStr[subMenuWateringIndex]);

  lcd.setCursor(0, 1);
  lcd.print(convertTimeToString(wateringMenu[subMenuWateringIndex]));
}

void addWateringTime(long timeValue)
{
  if (wateringMenu[subMenuWateringIndex] + timeValue >= 0) {
    wateringMenu[subMenuWateringIndex] += timeValue;
  }
  else {
    wateringMenu[subMenuWateringIndex] = 0;
  }

  waterPumpOnDurationDay = wateringMenu[subMenuWateringIndex];
}

void turnOnWaterPump() {
  Serial.println("Water pump On");
  digitalWrite(CHWatering, relay_On);
  isWatering = true;
}

void turnOffWaterPump() {
  Serial.println("Water pump Off");
  digitalWrite(CHWatering, relay_Off);
  isWatering = false;
}

void toggleWaterPump()
{
  long timeLapsed = millis() - lastWaterPumpOnTime;

  if (!isWatering) {
    if (timeLapsed > wateringMenu[kWateringOff]) {
      turnOnWaterPump();
      lastWaterPumpOnTime = millis();
    }
  }
  else {
    if (timeLapsed > wateringMenu[kWateringOn]) {
      turnOffWaterPump();
      lastWaterPumpOnTime = millis();
    }
  }
}

/*
   Airing
*/

void switchAirMenu(int directionDelta)
{
  int totalItems = (sizeof(airMenu) / sizeof(long));

  subMenuAirIndex = loopItems(subMenuAirIndex, totalItems, directionDelta);

  printAirTime();
}

void printAirTime()
{
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(airMenuStr[subMenuAirIndex]);

  lcd.setCursor(0, 1);
  lcd.print(convertTimeToString(airMenu[subMenuAirIndex]));
}

void addAirTime(long timeValue)
{
  Serial.print("add timeValue: ");
  Serial.println(timeValue);

  if (airMenu[subMenuAirIndex] + timeValue >= 0) {
    airMenu[subMenuAirIndex] += timeValue;
  }
  else {
    airMenu[subMenuAirIndex] = 0;
  }

  airPumpOnDurationDay = airMenu[subMenuAirIndex];
}

void turnOnAirPump() {
  Serial.println("Air pump On");
  digitalWrite(CHWatering, relay_On);
  isAiring = true;
}

void turnOffAirPump() {
  Serial.println("Air pump Off");
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


/*
   Light
*/

void switchLight()
{
  long currentTime = formatCurrentTime();

Serial.print("current time: ");
Serial.println(currentTime);
Serial.print("on time: ");
Serial.println(lightOnTime);
Serial.print("off time: ");
Serial.println(lightOffTime);

  // hourOn < hourOff. ex. 0:00 off -> 8:00 on -> 22:00 off
  if (currentTime >= lightOnTime && currentTime < lightOffTime) {
    turnOnLight();
  }
  else {
    return turnOffLight();
  }
}

long formatCurrentTime()
{
  time_t t = now();
  return hour(t) * 100 + minute(t);
}

void turnOnLight()
{
  Serial.println("Light On");
  digitalWrite(CHLight, relay_On);
}

void turnOffLight()
{
  Serial.println("Light Off");
  digitalWrite(CHLight, relay_Off);
}


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
    Serial.print(currentDay);
    Serial.print(" day ");
    timeStr = String(timeStr + currentDay + " day ");
  }
  if (hr > 0) {
    Serial.print(hr);
    Serial.print("h ");
    timeStr = String(timeStr + hr + "h ");
  }
  if (minutes > 0) {
    Serial.print(minutes);
    Serial.print("m ");
    timeStr = String(timeStr + minutes + "m ");
  }
  if (sec > 0) {
    Serial.print(sec);
    Serial.print("s ");
    timeStr = String(timeStr + sec + "s");
  }

  Serial.println("");

  return timeStr;
}




