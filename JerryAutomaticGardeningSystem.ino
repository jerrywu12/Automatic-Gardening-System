#include <LiquidCrystal.h>
#include <Time.h>

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

#define CH1 3   // Connect Digital Pin  on Arduino to CH1 on Relay Module
#define CH2 2   // Connect Digital Pin  on Arduino to CH2 on Relay Module
//#define CH3 1   // Connect Digital Pin  on Arduino to CH3 on Relay Module
//#define LCDBacklight 13   // TODO: Not able to do it with current sainSmart LCD board, it's using vcc instead of digital pin for power supply

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
int menuWateringIndex = 0;
int menuAirIndex = 0;
int menuLightIndex = 0;

// Init
int initDuration = 5;
bool firstSession = true;

// Status
bool isWatering = false;
bool isAiring = false;
bool isLighting = false;

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

long airMenu[] = {airPumpOnDuration,
                  airPumpOffDuration
                 };
String airMenuStr[] = {"ON Duration", "OFF Duration"};

// Relay Sign
#define relay_On  0
#define relay_Off 1

// Timer
unsigned long timeRef = 0;

// System Time
int currentMonth = 1;
int currentDay = 1;
int currentYear = 2016;
int currentHour = 10;
int currentMinute = 59;

// System Time Settings
int menuTimeIndex = 0;
int currentTime[] = {currentMonth, currentDay, currentYear, currentHour, currentMinute};
String currentTimeStr[] = {"Month", "Day", "Year", "Hour ", "Minute "};


void setup() {

  Serial.println("System Reset");

  Serial.begin(9600);

  // Setup all the Pins
  pinMode(CH1, OUTPUT);  // Water pump
  pinMode(CH2, OUTPUT);  // Air pump
  //  pinMode(CH3, OUTPUT);  // Light
  //pinMode(LCDBacklight, OUTPUT);

  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);

  // Set init state
  turnOnWaterPump();
  turnOnAirPump();

  lastWaterPumpOnTime = millis();
  lastAirPumpOnTime = millis();

  waterPumpOnDurationNight = setMin(2);
  waterPumpOffDurationNight = setHr(2);

  airPumpOnDurationNight = setMin(2);
  airPumpOffDurationNight = setHr(2);
}


void loop() {

  //digitalWrite(LCDBacklight, LOW);

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

    int durationOn = 2;
    int durationOff = 30;

    wateringMenu[kWateringOn] = waterPumpOnDurationDay = setMin(durationOn);
    wateringMenu[kWateringOff] = waterPumpOffDurationDay = setMin(durationOff);

    airMenu[0] = airPumpOnDurationDay = setMin(durationOn);
    airMenu[1] = airPumpOffDurationDay = setMin(durationOff);

    firstSession = false;
  }

  // Night Time Override - 11pm ~ 8am
  if (currentHour < 8 || currentHour > 23) {
    
    wateringMenu[kWateringOn] = waterPumpOnDurationNight;
    wateringMenu[kWateringOff] = waterPumpOffDurationNight;

    airMenu[0] = airPumpOnDurationNight;
    airMenu[1] = airPumpOffDurationNight;
  }
  else {

    wateringMenu[kWateringOn] = waterPumpOnDurationDay;
    wateringMenu[kWateringOff] = waterPumpOffDurationDay;

    airMenu[0] = airPumpOnDurationDay;
    airMenu[1] = airPumpOffDurationDay;
  }

  // set up systems
  toggleWaterPump();
  toggleAirPump();

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
              selectMenu(loopItems(mainMenuIndex, 3, 1));
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

    // switch back to home menu (currently clock)
    selectMenu(0);

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
 * Menu Selection 
 */
 
// Select Menu
void selectMenu(int selectIndex)
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
 * System Time
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
  int totalItems = (sizeof(currentTime) / sizeof(int));

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
      currentTime[menuTimeIndex] = setTimeValue(currentTime[menuTimeIndex], 1);
      showTimeSettings();
      break;

    case buttonDown: // Down
        currentTime[menuTimeIndex] = setTimeValue(currentTime[menuTimeIndex], -1);
      showTimeSettings();
      break;

    case buttonSelect:
      saveTimeSetting();
      break;
    default:
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
  lcd.print(currentTimeStr[menuTimeIndex]);

  lcd.setCursor(0, 1);
  lcd.print(currentTime[menuTimeIndex]);
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

  menuWateringIndex = loopItems(menuWateringIndex, totalItems, directionDelta);

  printWateringTime();
}

void printWateringTime()
{
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(wateringMenuStr[menuWateringIndex]);

  lcd.setCursor(0, 1);
  lcd.print(convertTimeToString(wateringMenu[menuWateringIndex]));
}

void addWateringTime(long timeValue)
{
  if (wateringMenu[menuWateringIndex] + timeValue >= 0) {
    wateringMenu[menuWateringIndex] += timeValue;
  }
  else {
    wateringMenu[menuWateringIndex] = 0;
  }

  waterPumpOnDurationDay = wateringMenu[menuWateringIndex];
}

void turnOnWaterPump() {
  Serial.println("Water pump On");
  digitalWrite(CH1, relay_On);
  isWatering = true;
}

void turnOffWaterPump() {
  Serial.println("Water pump Off");
  digitalWrite(CH1, relay_Off);
  isWatering = false;
}

void toggleWaterPump()
{
  //  if (lastWaterPumpOnTime <= 0) {
  //    lastWaterPumpOnTime = millis();
  //  }

  long timeLapsed = millis() - lastWaterPumpOnTime;

  if (!isWatering) {
    if (timeLapsed > wateringMenu[1]) {
      turnOnWaterPump();
      lastWaterPumpOnTime = millis();
    }
  }
  else {
    if (timeLapsed > wateringMenu[0]) {
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

  menuAirIndex = loopItems(menuAirIndex, totalItems, directionDelta);

  printAirTime();
}

void printAirTime()
{
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(airMenuStr[menuAirIndex]);

  lcd.setCursor(0, 1);
  lcd.print(convertTimeToString(airMenu[menuAirIndex]));
}

void addAirTime(long timeValue)
{
  Serial.print("add timeValue: ");
  Serial.println(timeValue);

  if (airMenu[menuAirIndex] + timeValue >= 0) {
    airMenu[menuAirIndex] += timeValue;
  }
  else {
    airMenu[menuAirIndex] = 0;
  }

  airPumpOnDurationDay = airMenu[menuAirIndex];
}

void turnOnAirPump() {
  Serial.println("Air pump On");
  digitalWrite(CH2, relay_On);
  isAiring = true;
}

void turnOffAirPump() {
  Serial.println("Air pump Off");
  digitalWrite(CH2, relay_Off);
  isAiring = false;
}

void toggleAirPump()
{
  //  if (lastAirPumpOnTime <= 0) {
  //    lastAirPumpOnTime = millis();
  //  }

  long timeLapsed = millis() - lastAirPumpOnTime;

  if (!isAiring) {
    if (timeLapsed > airMenu[1]) {
      turnOnAirPump();
      lastAirPumpOnTime = millis();
    }
  }
  else {
    if (timeLapsed > airMenu[0]) {
      turnOffAirPump();
      lastAirPumpOnTime = millis();
    }
  }
}


/*
   Convenient Functions
*/

int loopItems(int index, int totalItems, bool delta)
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




