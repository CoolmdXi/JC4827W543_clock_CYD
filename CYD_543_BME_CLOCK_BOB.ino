//
// ESP32 CYD (Cheap-Yellow-Display) Weather & Time Display
// written by Larry Bank
// Copyright (c) 2024 BitBank Software, Inc.
##########################################################################################
// all thanks to Larry above who spurred me on to try different things.July 24 coolMD
// Originally based on Larry Banks bb_spi_lcd and CYD_Projects repo
##########################################################################################
// Rewrittain to use BME/BMP280 sensor connected to board.
//Board is JC4827W543 from Guiton on Ali express.

//https://github.com/bitbank2/CYD_Projects
//https://github.com/bitbank2/bb_spi_lcd

#define LCD DISPLAY_CYD_543
// Define your time zone offset in seconds relative to GMT. e.g. Eastern USA = -(3600 * 5)
// The program will try to get it automatically, but will fall back on this value if that fails
#define TZ_OFFSET (3600)
int iTimeOffset;  // offset in seconds


#include <NTPClient.h>  //https://github.com/taranais/NTPClient
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Time.h>
#include <Adafruit_BME280.h>  // include Adafruit BME280 sensor library
#include <BitBang_I2C.h>
ESP32Time rtc(0);
HTTPClient http;
#include <Adafruit_Sensor.h>
#include <bb_spi_lcd.h>

#include "Roboto_Black_16.h"
#include "Roboto_25.h"
#include "Roboto_Thin66pt7b.h"
#include "Orbitron_Bold_88.h"
#include "DSEG7_Modern_Bold_88.h"
#include "Orbitron_Bold_66.h"

#define FONT DSEG7_Modern_Bold_88
#define FONT_GLYPHS DSEG7_Modern_Bold_88Glyphs
//#define FONT Roboto_Thin66pt7b
//#define FONT_GLYPHS Roboto_Thin66pt7bGlyphs

#define SDA_PIN 17
#define SCL_PIN 18

#define WIFI_SSID "your ssd"
#define WIFI_PWD "PASSWORD"

#define BITBANG false

bool firstBMEUpdateDone = false;
bool firstInternetTimeUpdateDone = false;
bool dateUpdatedToday = false;/////////////////////////////////////////////////////////////////////////////////////

unsigned long previousMillisBME = 0;
unsigned long previousMillisInternetTime = 0;

const long intervalBME = 2 * 60 * 1000;                 // 2 minutes sensor update
const long intervalInternetTime =  7 * 60 * 60 * 1000;  // 7 hours internet time update

BB_SPI_LCD lcd;
BBI2C bbi2c;
Adafruit_BME280 bme;

struct tm myTime;  // Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

char szOldTime[16];
int iDigitPos[6];  // clock digit positions
int iCharWidth, iColonWidth;
int iStartX, iStartY;

uint16_t usColor = TFT_CYAN;  // time digit color

// Month names array
const char *monthNames[] = {
  "JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE",
  "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"
};


void GetInternetTime() {
  char szIP[32];

  iTimeOffset = TZ_OFFSET;  // start with fixed offset you can set in the program
  
  timeClient.setTimeOffset(iTimeOffset);  //My timezone
  timeClient.update();
  Serial.println(timeClient.getFormattedTime());
  unsigned long epochTime = timeClient.getEpochTime();
  
  struct tm *ptm = gmtime((time_t *)&epochTime);//Get a time structure

  int currentYear = ptm->tm_year + 1900;  // Format date
  int currentMonth = ptm->tm_mon ;     
  int currentDay = ptm->tm_mday; 

  char dateBuffer[32];   ///Date Buffer                 
  snprintf(dateBuffer, sizeof(dateBuffer), "%02d %s %04d", currentDay, monthNames[currentMonth], currentYear);//Print DATE
  lcd.setCursor(115, 18);                 
  lcd.setTextColor(TFT_YELLOW, TFT_BLUE);  
  lcd.setFont(FONT_16x32);                
  lcd.print(dateBuffer);                  // Print Date

  memcpy(&myTime, ptm, sizeof(myTime));  // get the current time struct into a local copy
  rtc.setTime(epochTime);                // set the ESP32's internal RTC to the correct time
  Serial.printf("Current time: %02d:%02d:%02d\n", myTime.tm_hour, myTime.tm_min, myTime.tm_sec);
  //timeClient.end();  // don't need it any more
  Serial.print("time and date Updated   ");
} /* GetInternetTime() */

void setup() {
  Serial.begin(115200); 
  lcd.begin(LCD);
  lcd.fillScreen(TFT_BLACK);
  lcd.setFont(FONT_12x16);
  lcd.setTextColor(TFT_GREEN, TFT_BLACK);
  bbi2c.bWire = !BITBANG;  // use bit bang, not wire library
  bbi2c.iSDA = SDA_PIN;
  bbi2c.iSCL = SCL_PIN;
  I2CInit(&bbi2c, 100000L);

  // Draw the outer rounded rectangle  within screen edges
  lcd.drawRoundRect(1, 1, 478, 271, 10, TFT_WHITE);
  lcd.drawRoundRect(4, 4, 472, 265, 10, TFT_WHITE);
  lcd.fillRoundRect(5, 5, 469, 50, 10, TFT_BLUE);  //////////////////draw top Border


  WiFi.begin(WIFI_SSID, WIFI_PWD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting Wifi");
  }
  Serial.println("WiFi Connected!");
  delay(1000);

  timeClient.begin();// Initialize a NTPClient to get time

  previousMillisBME = millis();           // Initialize the start time BME function
  previousMillisInternetTime = millis();  // Initialize the start time Internet time function

  lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
  lcd.setCursor(25, 190);/////was 10
  lcd.print("TEMPERATURE");
  lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
  lcd.setCursor(195, 190); //was 180
  lcd.print("HUMIDITY");
  lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
  lcd.setCursor(340, 190);
  lcd.print("PRESSURE");
  lcd.setFont(FONT_12x16);

  // Prepare positions of clock digits
  iCharWidth = FONT_GLYPHS['0' - ' '].xAdvance;
  iColonWidth = FONT_GLYPHS[':' - ' '].xAdvance;

  iStartX = 60;
  iStartY = lcd.height() - 105;
  iDigitPos[0] = iStartX;
  iDigitPos[1] = iStartX + iCharWidth;
  iDigitPos[2] = iStartX + iCharWidth * 2;
  iDigitPos[3] = iStartX + iColonWidth + iCharWidth * 2;
  iDigitPos[4] = iDigitPos[3] + iCharWidth;
  iDigitPos[5] = lcd.width();

} /* setup() */
bool status;

void DisplayTime(void) {

  char szTemp[2], szTime[32];
  int i, iHour, iMin, iSec;
  struct tm *ptm;
  unsigned long epochTime;

  iHour = rtc.getHour(true);  ///remove true for 12 hour
  iMin = rtc.getMinute();
  iSec = rtc.getSecond();

  sprintf(szTime, "%02d:%02d", iHour, iMin);
  if (iSec & 0x1) {  // flash the colon
    szTime[2] = ' ';
  }
  if (strcmp(szTime, szOldTime)) {  // digit(s) changed, redraw them to minimize flicker
    szTemp[1] = 0;
    lcd.setFreeFont(&FONT);
    for (i = 0; i < 5; i++) {
      if (szTime[i] != szOldTime[i]) {
        szTemp[0] = szOldTime[i];
        lcd.setTextColor(TFT_BLACK, TFT_BLACK + 1);
        lcd.drawString(szTemp, iDigitPos[i], iStartY);  // erase old character
        // draw new character
        if (i == 0 && szTime[0] == '0') continue;  // skip leading 0 for hours
        lcd.setTextColor(usColor, TFT_BLACK);
        szTemp[0] = szTime[i];
        lcd.drawString(szTemp, iDigitPos[i], iStartY);
      }  // if needs redraw
    }    // for i
    strcpy(szOldTime, szTime);
  }
} /* DisplayTime() */

void BMEupDate() {  /// Initialise BME"*)
  lcd.setFont(FONT_12x16);

  status = bme.begin(0x76);
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1)
      ;
  }
  char _buffer[8];  // 
  // read temperature, humidity and pressure from the BME280 sensor
  float temp = bme.readTemperature();  // get temperature in °C
  float humi = bme.readHumidity();     // get humidity in rH%
  float pres = bme.readPressure()+1200.00;     // get pressure in Pa /////////// correct for error

  Serial.println(pres),"   ";

  // print temperature (in C)
  sprintf(_buffer, " %.1f", temp);
  // sprintf(_buffer, " %02u.%02u", (int)temp, (int)(temp * 100) % 100);
  lcd.setFont(FONT_16x32);
  lcd.setTextColor(TFT_YELLOW, TFT_BLACK);  // Temp Color
  lcd.setCursor(30, 220);
  lcd.print(_buffer);
  lcd.println(" C");

  // 2: print humidity
  sprintf(_buffer, " %.1f %%", humi);
  
  lcd.setTextColor(TFT_YELLOW, TFT_BLACK);//Humidity color
  lcd.setCursor(180, 220);
  lcd.println(_buffer);

  // 3: print pressure 
  sprintf(_buffer, "%04u", (int)(pres / 100));// display PA converted to mb
//sprintf( _buffer, "%04u.%02u", (int)(pres/100), (int)((uint32_t)pres % 100) );//2 dec places
  lcd.setFont(FONT_16x32);
  lcd.setTextColor(TFT_YELLOW, TFT_BLACK);  // set text color for Pressure
  lcd.setCursor(338, 220);                 //
  lcd.print(_buffer);
  lcd.println(" mb");

  // BMEupDate()
}

void loop() {
  unsigned long currentMillis = millis();

  // Update BME sensor at the beginning of the loop if it's the first loop or if the interval has passed
  if (!firstBMEUpdateDone || (currentMillis - previousMillisBME >= intervalBME)) {
    BMEupDate();
    previousMillisBME = currentMillis;
    firstBMEUpdateDone = true;  // Set the flag to true after the first update
  }

  // Update internet time at the beginning of the loop if it's the first loop or if the interval has passed
  if (!firstInternetTimeUpdateDone || (currentMillis - previousMillisInternetTime >= intervalInternetTime)) {
    GetInternetTime();              // Update the internal RTC with accurate time
    strcpy(szOldTime, "        ");  // Force complete repaint of time after WiFi update
    previousMillisInternetTime = currentMillis;
    firstInternetTimeUpdateDone = true;  // Set the flag to true after the first update
  }
    // Check if it's 0:01 and the date hasn't been updated today
  if (rtc.getHour(true) == 0 && rtc.getMinute() == 1 && !dateUpdatedToday) {
    GetInternetTime();
    
    dateUpdatedToday = true;
  }


  // Reset the flag at the start of a new day
  if (rtc.getHour(true) == 0 && rtc.getMinute() == 0) {
    dateUpdatedToday = false;
  }

  // Update display every second
  DisplayTime();
  delay(1000);  // Maintain a 1-second update interval for the display
}
