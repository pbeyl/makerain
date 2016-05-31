#include <Arduino.h>

#include "Nextion.h"
#include "NextionPage.h"
#include "NextionButton.h"
#include "NextionText.h"
#include "SoftwareSerial.h"
#include "Wire.h"
#include "RTClib.h"
#include "EEPROM.h"
//#include "TimeLib.h"
//#include "TimeAlarms.h"

//#define SERIAL_TX_BUFFER_SIZE 32
//#define SERIAL_RX_BUFFER_SIZE 32
//#define _SS_MAX_RX_BUFF 64 // SoftwareSerial RX buffer size


// ID of the settings block
#define CONFIG_VERSION "ps"

// Tell it where to store your config data in EEPROM
#define CONFIG_START 32

RTC_DS1307 rtc;

//Also uncomment nextionSerial.begin() in setup() if using this code
SoftwareSerial nextionSerial(2, 3); // RX, TX using software serial on pins 10, 11
Nextion nex(nextionSerial);

//Also comment out nextionSerial.begin() in setup() if using this code
//Nextion nex(Serial);    // use default hardware serial pin 0, 1

// Schedule variable storage
struct StoreStruct {
  // This is for mere detection if they are your settings
  char version[3];
  int8_t Hour, Min, Period;
  unsigned char DayOfWeek;  // The settings variables
  bool watersave;
} schedule = {    // The default values
  CONFIG_VERSION,
  0, 0, 0,
  0x00,
  false
};

//This is nice but bad for dynamic memory
//char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
//char monthsOfTheYear[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};


char charBuf[6];

unsigned int settime_year = 0;
int8_t settime_month = 0;
int8_t settime_day = 0;
int8_t settime_hour = 0;
int8_t settime_minute = 0;

boolean ThasChanged = false;
boolean ShasChanged = false;
boolean Pause = false;

// GUI Declarations (nex, pgnum, objid, objname)
//
//Home GUI Elements
NextionText   hDD         (nex, 0, 4, "DD"); //Day
NextionText   hMM         (nex, 0, 9, "MM"); //Month
NextionText   hYY         (nex, 0, 14, "YY"); //Month
NextionText   hHH         (nex, 0, 8, "HH"); //Hour
NextionText   hmm         (nex, 0, 10, "mm"); //Minute
NextionButton hSet          (nex, 0, 1, "b0"); //Settings Button
NextionButton hShed         (nex, 0, 2, "b1"); //Shedule Button
NextionButton b1            (nex, 0, 15, "b2");
NextionButton b2            (nex, 0, 16, "b3");
NextionButton b3            (nex, 0, 17, "b4");
NextionButton b4            (nex, 0, 18, "b5");

//Scheduler GUI Elements
NextionText   sdHH          (nex, 2, 3, "H1"); //Hour
NextionText   sdmm          (nex, 2, 5, "m1"); //minute
NextionText   sdtt          (nex, 2, 7, "tt"); //duration minutes
NextionButton sdUp          (nex, 2, 14, "b8"); //Up
NextionButton sdDown        (nex, 2, 15, "b9"); //Down
NextionButton sdOk          (nex, 2, 16, "b10"); //OK
NextionButton sdd1          (nex, 2, 18, "d1"); //Sunday
NextionButton sdd2          (nex, 2, 1, "d2"); //Monday
NextionButton sdd3          (nex, 2, 9, "d3"); //Tuesday
NextionButton sdd4          (nex, 2, 10, "d4"); //Wednesday
NextionButton sdd5          (nex, 2, 11, "d5"); //Thursday
NextionButton sdd6          (nex, 2, 12, "d6"); //Friday
NextionButton sdd7          (nex, 2, 13, "d7"); //Saturday

//Settings GUI Elements
NextionText   setYY         (nex, 1, 6, "YY");
NextionText   setMM         (nex, 1, 8, "MM");
NextionText   setDD         (nex, 1, 10, "DD");
NextionText   setHH         (nex, 1, 11, "HH");
NextionText   setmm         (nex, 1, 13, "mm");
NextionButton setUp         (nex, 1, 2, "up"); //Up
NextionButton setDown       (nex, 1, 3, "do"); //Down
NextionButton setOk         (nex, 1, 4, "ok"); //OK

NextionPage   Home          (nex, 0,  0, "Pg1");
NextionPage   Settings      (nex, 1,  0, "Pg2");
NextionPage   SchedPage     (nex, 2,  0, "Pg3");


/*
 * Load stored schedule from EEPROM
 */
void loadConfig() {
  // To make sure there are settings, and they are YOURS!
  // If nothing is found it will use the default settings.
  if (EEPROM.read(CONFIG_START + 0) == CONFIG_VERSION[0] &&
      EEPROM.read(CONFIG_START + 1) == CONFIG_VERSION[1] &&
      EEPROM.read(CONFIG_START + 2) == CONFIG_VERSION[2])
    for (unsigned int t=0; t<sizeof(schedule); t++)
      *((char*)&schedule + t) = EEPROM.read(CONFIG_START + t);
}

/*
 * Save schedule to EEPROM
 */
void saveConfig() {
  for (unsigned int t=0; t<sizeof(schedule); t++)
    EEPROM.write(CONFIG_START + t, *((char*)&schedule + t));
}


/*
* Simple function to print the current time on serial
*/
void print_time(DateTime &now){

  Serial.print(now.day());
  Serial.print(F("/"));
  Serial.print(now.month());
  Serial.print(F("/"));
  Serial.print(now.year());
  Serial.print(F(" "));
  Serial.print(now.hour());
  Serial.print(F(":"));
  Serial.println(now.minute());
}

char* padStr(uint8_t i){
  memset(charBuf, 0, 2);
  sprintf(charBuf, "%02d",i);
  return charBuf;
}

/*
* Since we are using bit's in a byte to set dayofweek we check if bit is set using this
* When n = 1 (Mon) while n = 7 (Sun)
*/
int isDaySet(unsigned char c, uint8_t n) {
    static unsigned char mask[] = {1, 2, 4, 8, 16, 32, 64, 128};
    return ((c & mask[n-1]) != 0);
}

/*
* manually set the background colour of the home page buttons b2-b5
*/
void setZoneBackgroundColour(uint8_t day, uint16_t col) {
  size_t commandLen = 12;
  char commandBuffer[commandLen];
  snprintf(commandBuffer, commandLen, "b%d.bco=%d", day+1, col);
  nex.sendCommand(commandBuffer);

  snprintf(commandBuffer, commandLen, "ref b%d", day+1);
  nex.sendCommand(commandBuffer);
}

/*
* Function to trigger Zone 1-4 state
*/
void zoneTrigger(uint8_t Z, bool STATE){
  switch (Z) {
    case 0:
      digitalWrite(13, STATE);
      digitalWrite(12, STATE);
      digitalWrite(11, STATE);
      digitalWrite(10, STATE);
      setZoneBackgroundColour(1, 0);
      setZoneBackgroundColour(2, 0);
      setZoneBackgroundColour(3, 0);
      setZoneBackgroundColour(4, 0);
      break;
    case 1:
      digitalWrite(13, STATE);
      digitalWrite(12, LOW);
      digitalWrite(11, LOW);
      digitalWrite(10, LOW);

      if (STATE) {
        setZoneBackgroundColour(1, 1024);
      } else {
        setZoneBackgroundColour(1, 0);
      }

      setZoneBackgroundColour(2, 0);
      setZoneBackgroundColour(3, 0);
      setZoneBackgroundColour(4, 0);
      break;
    case 2:
      digitalWrite(13, LOW);
      digitalWrite(12, STATE);
      digitalWrite(11, LOW);
      digitalWrite(10, LOW);
      setZoneBackgroundColour(1, 0);

      if (STATE) {
        setZoneBackgroundColour(2, 1024);
      } else {
        setZoneBackgroundColour(2, 0);
      }

      setZoneBackgroundColour(3, 0);
      setZoneBackgroundColour(4, 0);
      break;
    case 3:
      digitalWrite(13, LOW);
      digitalWrite(12, LOW);
      digitalWrite(11, STATE);
      digitalWrite(10, LOW);
      setZoneBackgroundColour(1, 0);
      setZoneBackgroundColour(2, 0);

      if (STATE) {
        setZoneBackgroundColour(3, 1024);
      } else {
        setZoneBackgroundColour(3, 0);
      }

      setZoneBackgroundColour(4, 0);
      break;
    case 4:
      digitalWrite(13, LOW);
      digitalWrite(12, LOW);
      digitalWrite(11, LOW);
      digitalWrite(10, STATE);
      setZoneBackgroundColour(1, 0);
      setZoneBackgroundColour(2, 0);
      setZoneBackgroundColour(3, 0);

      if (STATE) {
        setZoneBackgroundColour(4, 1024);
      } else {
        setZoneBackgroundColour(4, 0);
      }

      break;
  }
}

/*
 * Display current time and date on home page
 *
 */
void refreshTime() {
    DateTime now = rtc.now();

    //Serial.println(F("refreshTime"));

    /*
     * Update home page time text
     */

    hDD.setText(padStr(now.day()));
    //hMM.setText(monthsOfTheYear[now.month()-1]);

    hMM.setText(padStr(now.month()));
    hYY.setTextAsNumber(now.year());

    hHH.setText(padStr(now.hour()));
    hmm.setText(padStr(now.minute()));


    /*
    * Algorith to check if current time is during irrigation schedule
    */
    DateTime alarm(now.year(),now.month(),now.day(),schedule.Hour, schedule.Min, 0);
    int16_t tDuration = (schedule.Period*60); //Duration of alarm in seconds
    int16_t tDiff = now.unixtime()-alarm.unixtime(); //time past since alarm
    bool inAlarm = false;

    if (isDaySet(schedule.DayOfWeek, now.dayOfTheWeek()+1)) {
      if ((tDiff > 0) && (tDiff <= (4*tDuration)))
        {
          inAlarm = true;
            if (tDiff<=tDuration){
              zoneTrigger(1,HIGH);
            }else if (tDiff<=(2*tDuration)) {
              zoneTrigger(2,HIGH);
            }else if (tDiff<=(3*tDuration)) {
              zoneTrigger(3,HIGH);
            }else {
              zoneTrigger(4,HIGH);
            }
        } else if (inAlarm) { //reset all flags at end of schedule when was in alarm state
          zoneTrigger(0,LOW);//0 special - set all zones same value
          inAlarm = false;
        }
    }
}

/*
 * Set the settings page text with current time values
 *
 */
void populateSettings() {
  //Serial.println(F("display Time"));

  DateTime now = rtc.now();

    settime_year = now.year();
    settime_month = now.month();
    settime_day = now.day();

    settime_hour = now.hour();
    settime_minute = now.minute();

    setYY.setTextAsNumber(settime_year);

    memset(charBuf, 0, sizeof(charBuf));
    sprintf(charBuf, "%02d",settime_month);
    setMM.setText(charBuf);

    memset(charBuf, 0, sizeof(charBuf));
    sprintf(charBuf, "%02d",settime_day);
    setDD.setText(charBuf);
    //hMM.setText(monthsOfTheYear[now.month()-1]);

    memset(charBuf, 0, sizeof(charBuf));
    sprintf(charBuf, "%02d",settime_hour);
    setHH.setText(charBuf);

    memset(charBuf, 0, sizeof(charBuf));
    sprintf(charBuf, "%02d",settime_minute);
    setmm.setText(charBuf);

}

/*
* manually set the background colour of the schdule page buttons d1-d7
*/
void setDayBackgroundColour(uint8_t day) {
  size_t commandLen = 12;
  char commandBuffer[commandLen];
  snprintf(commandBuffer, commandLen, "d%d.bco=1024", day);
  nex.sendCommand(commandBuffer);

  snprintf(commandBuffer, commandLen, "ref d%d", day);
  nex.sendCommand(commandBuffer);
}

/*
 * Set the schedule page text with current values
 *
 */
void populateSchedule() {

  memset(charBuf, 0, sizeof(charBuf));
  sprintf(charBuf, "%02d", schedule.Hour);
  sdHH.setText(charBuf);

  memset(charBuf, 0, sizeof(charBuf));
  sprintf(charBuf, "%02d", schedule.Min);
  sdmm.setText(charBuf);

  memset(charBuf, 0, sizeof(charBuf));
  sprintf(charBuf, "%02d", schedule.Period);
  sdtt.setText(charBuf);

  /*
  * This for will iterate through every day and set the relevant buttons active
  */
  for (uint8_t n=1; n<8; n++) {
    if (isDaySet(schedule.DayOfWeek, n)) {setDayBackgroundColour(n);}
  }

}


/*
 *
 * Function called when pressing the settings button
 *
 */
void hSetcallback(NextionEventType type, INextionTouchable *widget) {

  Pause = true;         // Pause the clock refresh
  Settings.show();
  populateSettings();

}

/*
 *
 * Function called when pressing the Shedule button
 *
 */
void hShedcallback(NextionEventType type, INextionTouchable *widget) {

  Pause = true;         // Pause the clock refresh
  SchedPage.show();
  populateSchedule();

}


void setUpcallback(NextionEventType type, INextionTouchable *widget)
{

  if (setYY.getBackgroundColour() == 1024)
  {
    ThasChanged = true;
    setYY.setTextAsNumber(++settime_year);
  } else if (setMM.getBackgroundColour() == 1024)
  {
    ThasChanged = true;

    settime_month++;
        if (settime_month > 12)
        {
          settime_month = 1;
        }

    setMM.setText(padStr(settime_month));

  } else if (setDD.getBackgroundColour() == 1024)
  {
    ThasChanged = true;

    settime_day++;
        if (settime_day > 31)
        {
          settime_day = 1;
        }
    setDD.setText(padStr(settime_day));

  } else if (setHH.getBackgroundColour() == 1024)
  {
    ThasChanged = true;

    settime_hour++;
        if (settime_hour > 23)
        {
          settime_hour = 0;
        }
    setHH.setText(padStr(settime_hour));

  } else if (setmm.getBackgroundColour() == 1024)
  {
    ThasChanged = true;

    settime_minute++;
        if (settime_minute > 59)
        {
          settime_minute = 0;
        }

    setmm.setText(padStr(settime_minute));

  }
}


void setDowncallback(NextionEventType type, INextionTouchable *widget)
{
  if (setYY.getBackgroundColour() == 1024)
  {
    ThasChanged = true;

    setYY.setTextAsNumber(--settime_year);

  } else if (setMM.getBackgroundColour() == 1024)
  {
    ThasChanged = true;

    settime_month--;
        if (settime_month < 1)
        {
          settime_month = 12;
        }

    setMM.setText(padStr(settime_month));

  } else if (setDD.getBackgroundColour() == 1024)
  {
    ThasChanged = true;

    settime_day--;
        if (settime_day < 1)
        {
          settime_day = 31;
        }

    setDD.setText(padStr(settime_day));

  } else if (setHH.getBackgroundColour() == 1024)
  {
    ThasChanged = true;

    settime_hour--;
        if (settime_hour < 0)
        {
          settime_hour = 23;
        }

    setHH.setText(padStr(settime_hour));

  } else if (setmm.getBackgroundColour() == 1024)
  {
    ThasChanged = true;

    settime_minute--;
        if (settime_minute < 0)
        {
          settime_minute = 59;
        }

    setmm.setText(padStr(settime_minute));

  }
}

void setOkcallback(NextionEventType type, INextionTouchable *widget)
{
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    if (ThasChanged)
    {
      rtc.adjust(DateTime(settime_year, settime_month, settime_day, settime_hour, settime_minute, 0));
      ThasChanged = false;
    }


    Home.show();
    refreshTime();

    Pause = false;
    //refreshTime();

}

void sdUpcallback(NextionEventType type, INextionTouchable *widget)
{
  //schedule.Hour
  //schedule.Min
  //schedule.Period

  if (sdHH.getBackgroundColour() == 1024)
  {
    ShasChanged = true;

    memset(charBuf, 0, sizeof(charBuf));
    schedule.Hour++;
        if (schedule.Hour > 23)
        {
          schedule.Hour = 0;
        }
    sprintf(charBuf, "%02d", schedule.Hour);
    //itoa(settime_minute, charBuf, 10);
    sdHH.setText(charBuf);
  } else if (sdmm.getBackgroundColour() == 1024)
  {
    ShasChanged = true;

    memset(charBuf, 0, sizeof(charBuf));
    schedule.Min+=5;
        if (schedule.Min > 59)
        {
          schedule.Min = 0;
        }
    sprintf(charBuf, "%02d", schedule.Min);
    //itoa(settime_minute, charBuf, 10);
    sdmm.setText(charBuf);
  } else if (sdtt.getBackgroundColour() == 1024)
  {
    ShasChanged = true;

    memset(charBuf, 0, sizeof(charBuf));
    schedule.Period++;
        if (schedule.Period > 59)
        {
          schedule.Period = 0;
        }
    sprintf(charBuf, "%02d", schedule.Period);
    //itoa(settime_minute, charBuf, 10);
    sdtt.setText(charBuf);
  }
}

void sdDowncallback(NextionEventType type, INextionTouchable *widget)
{

  if (sdHH.getBackgroundColour() == 1024)
  {
    ShasChanged = true;

    memset(charBuf, 0, sizeof(charBuf));
    schedule.Hour--;
        if (schedule.Hour < 0)
        {
          schedule.Hour = 23;
        }
    sprintf(charBuf, "%02d", schedule.Hour);
    //itoa(settime_minute, charBuf, 10);
    sdHH.setText(charBuf);
  } else if (sdmm.getBackgroundColour() == 1024)
  {
    ShasChanged = true;

    memset(charBuf, 0, sizeof(charBuf));
    schedule.Min-=5;
        if (schedule.Min < 0)
        {
          schedule.Min = 55;
        }
    sprintf(charBuf, "%02d", schedule.Min);
    //itoa(settime_minute, charBuf, 10);
    sdmm.setText(charBuf);
  } else if (sdtt.getBackgroundColour() == 1024)
  {
    ShasChanged = true;

    memset(charBuf, 0, sizeof(charBuf));
    schedule.Period--;
        if (schedule.Period < 0)
        {
          schedule.Period = 59;
        }
    sprintf(charBuf, "%02d", schedule.Period);
    //itoa(settime_minute, charBuf, 10);
    sdtt.setText(charBuf);
  }

}

void sdOkcallback(NextionEventType type, INextionTouchable *widget)
{

  if (ShasChanged)
  {
    saveConfig(); //Save schedule to EEPROM
    ShasChanged = false;
  }
  Home.show();
  refreshTime();

  Pause = false;

}

void ZoneOn1(NextionEventType type, INextionTouchable *widget)
{
  zoneTrigger(1,!digitalRead(13));
}

void ZoneOn2(NextionEventType type, INextionTouchable *widget)
{
  zoneTrigger(2,!digitalRead(12));
}

void ZoneOn3(NextionEventType type, INextionTouchable *widget)
{
  zoneTrigger(3,!digitalRead(11));
}

void ZoneOn4(NextionEventType type, INextionTouchable *widget)
{
  zoneTrigger(4,!digitalRead(10));
}


void sdd1callback(NextionEventType type, INextionTouchable *widget)
{

  //Mon 0000 0001 0x01
  //Tue 0000 0010 0x02
  //Wed 0000 0100 0x04
  //Thu 0000 1000 0x08
  //Fri 0001 0000 0x10
  //Sat 0010 0000 0x20
  //Sun 0100 0000 0x40

  uint8_t dayTest = 1; //Day 1 0000 0001
  ShasChanged = true;

  //if (type == NEX_EVENT_POP){
    if (isDaySet(schedule.DayOfWeek, dayTest)) {
      schedule.DayOfWeek ^= 1 << (dayTest-1);         //Simply flip the bit
      sdd1.setBackgroundColour(0,true);
    } else {
      schedule.DayOfWeek ^= 1 << (dayTest-1);
      sdd1.setBackgroundColour(1024,true);
    }
  //}

}
void sdd2callback(NextionEventType type, INextionTouchable *widget)
{

  uint8_t dayTest = 2; //Day 2 0000 0010
  ShasChanged = true;

  if (type == NEX_EVENT_POP){
    if (isDaySet(schedule.DayOfWeek, dayTest)) {
      schedule.DayOfWeek ^= 1 << (dayTest-1);
      sdd2.setBackgroundColour(0,true);
    } else {
      schedule.DayOfWeek ^= 1 << (dayTest-1);
      sdd2.setBackgroundColour(1024,true);
    }
  }
}

void sdd3callback(NextionEventType type, INextionTouchable *widget)
{
  uint8_t dayTest = 3; //Day 3 0000 0100
  ShasChanged = true;

  if (type == NEX_EVENT_POP){
    if (isDaySet(schedule.DayOfWeek, dayTest)) {
      schedule.DayOfWeek ^= 1 << (dayTest-1);
      sdd3.setBackgroundColour(0,true);
    } else {
      schedule.DayOfWeek ^= 1 << (dayTest-1);
      sdd3.setBackgroundColour(1024,true);
    }
  }
}

void sdd4callback(NextionEventType type, INextionTouchable *widget)
{

  uint8_t dayTest = 4; //Day 4 0000 1000
  ShasChanged = true;

  if (type == NEX_EVENT_POP){
    if (isDaySet(schedule.DayOfWeek, dayTest)) {
      schedule.DayOfWeek ^= 1 << (dayTest-1);
      sdd4.setBackgroundColour(0,true);
    } else {
      schedule.DayOfWeek ^= 1 << (dayTest-1);
      sdd4.setBackgroundColour(1024,true);
    }
  }
}

void sdd5callback(NextionEventType type, INextionTouchable *widget)
{
  uint8_t dayTest = 5; //Day 5 0001 0000
  ShasChanged = true;

  if (type == NEX_EVENT_POP){
    if (isDaySet(schedule.DayOfWeek, dayTest)) {
      schedule.DayOfWeek ^= 1 << (dayTest-1);
      sdd5.setBackgroundColour(0,true);
    } else {
      schedule.DayOfWeek ^= 1 << (dayTest-1);
      sdd5.setBackgroundColour(1024,true);
    }
  }
}

void sdd6callback(NextionEventType type, INextionTouchable *widget)
{
  uint8_t dayTest = 6; //Day 6 0010 0000
  ShasChanged = true;

  if (type == NEX_EVENT_POP){
    if (isDaySet(schedule.DayOfWeek, dayTest)) {
      schedule.DayOfWeek ^= 1 << (dayTest-1);
      sdd6.setBackgroundColour(0,true);
    } else {
      schedule.DayOfWeek ^= 1 << (dayTest-1);
      sdd6.setBackgroundColour(1024,true);
    }
  }
}

void sdd7callback(NextionEventType type, INextionTouchable *widget)
{
  uint8_t dayTest = 7; //Day 7 0100 0000
  ShasChanged = true;

  if (type == NEX_EVENT_POP){
    if (isDaySet(schedule.DayOfWeek, dayTest)) {
      schedule.DayOfWeek ^= 1 << (dayTest-1);
      sdd7.setBackgroundColour(0,true);
    } else {
      schedule.DayOfWeek ^= 1 << (dayTest-1);
      sdd7.setBackgroundColour(1024,true);
    }
  }
}

/*
* Function to Send a raw text command to the LCD
*/
void sendCmD(const char *command, uint8_t size){
  //Serial.println(command);
  //Serial.println(size);

  char commandBuffer[size];
  snprintf(commandBuffer, size, "%s", command);

  //Serial.println(commandBuffer);
  nex.sendCommand(commandBuffer);

}



void setup() {
  loadConfig();     //Load saved schedule from EEPROM

  //Uncomment to Reset config to default
  //schedule.DayOfWeek = 0x00;

  Serial.begin(115200);
  // Initialize Nextion LCD
  nextionSerial.begin(115200);

  // Perform PIN configuration
  pinMode(10, OUTPUT);
  pinMode(11, OUTPUT);
  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);
  zoneTrigger(0, LOW);    //reset all pins/zones to low

  /*
   * Configure Pin A2=GND and A3=VCC to power RTC Module
   * RTC using A0-A3 for power and I2C comms
   */
  pinMode(17, OUTPUT);  // A3
  pinMode(16, OUTPUT);  // A2
  digitalWrite(17, HIGH);
  digitalWrite(16, LOW);

  // Initialize RTC
  if (! rtc.begin()) {
    Serial.println(F("ERR:RTC unavailable"));
    while (1);
  }

  //Reset RTC to default value on first start
  if (! rtc.isrunning()) {
    Serial.println(F("WARN:RTC reset"));
    // following line sets the RTC to the date & time this sketch was compiled
    //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    rtc.adjust(DateTime(2016, 1, 1, 0, 0, 0));
  }

  nex.init();

  Serial.print(F("Initializing Display: "));
  // Callbacks for Home page
  Serial.print(hSet.attachCallback(&hSetcallback));
  Serial.print(hShed.attachCallback(&hShedcallback));
  Serial.print(b1.attachCallback(&ZoneOn1));
  Serial.print(b2.attachCallback(&ZoneOn2));
  Serial.print(b3.attachCallback(&ZoneOn3));
  Serial.print(b4.attachCallback(&ZoneOn4));

  // Callbacks for Settings page
  Serial.print(setUp.attachCallback(&setUpcallback));
  Serial.print(setDown.attachCallback(&setDowncallback));
  Serial.print(setOk.attachCallback(&setOkcallback));

  // Callbacks for Sheduler page
  Serial.print(sdUp.attachCallback(&sdUpcallback));
  Serial.print(sdDown.attachCallback(&sdDowncallback));
  Serial.print(sdOk.attachCallback(&sdOkcallback));

  Serial.print(sdd1.attachCallback(&sdd1callback));
  Serial.print(sdd2.attachCallback(&sdd2callback));
  Serial.print(sdd3.attachCallback(&sdd3callback));
  Serial.print(sdd4.attachCallback(&sdd4callback));
  Serial.print(sdd5.attachCallback(&sdd5callback));
  Serial.print(sdd6.attachCallback(&sdd6callback));
  Serial.println(sdd7.attachCallback(&sdd7callback));

  //Configure the Display to sleep after X seconds, maybe move this onto the display...
  //sendCmD("thsp=30", 8);
  //sendCmD("thup=1", 8);

  Serial.println(F("Done."));

  //Load current time into buffers
  refreshTime();
}

void loop() {
  // put your main code here, to run repeatedly:

/*
 * Refresh time every second unless Paused
 * Not using delay means more reactive experience
 */
 if (!Pause)
 {
   if ((millis() % 2000)==0)
   {
     refreshTime();

   }
 }


  nex.poll();
}
