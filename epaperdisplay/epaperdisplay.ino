/*
    Reading data from Home Assistant REST Api  and displaying on
    DollaTek T5 V1.3 ESP32 2.9 inch EPaper Plus electronic ink screen development board
    from https://www.amazon.co.uk/gp/product/B07RXRX7V5/
*/
#define LILYGO_T5_V22

#include <boards.h>
#include <GxEPD.h>

#include <GxDEPG0290B/GxDEPG0290B.h>      // 2.9" b/w  

#include <U8g2_for_Adafruit_GFX.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
//#include <Ticker.h>
#include <driver/adc.h>
#include "esp_adc_cal.h"
#include <HARestAPI.h>
#include <ArduinoJson.h>

// enable or disable debug mode to serial console
#define DEBUG

#ifdef DEBUG
#define TRACE(x) Serial.println(x);
#else
#define TRACE(x)
#endif

WiFiClient sclient;
HARestAPI ha(sclient);

// Network Settings
const char* ssid = "YOURWIFISSID";
const char* password = "YOURWIFIPASSWORD";
/* Put StaticIP Address details */
IPAddress local_ip(192,168,1,1); // Set static IP
IPAddress gateway(192,168,1,1); // Set network Gateway IP
IPAddress subnet(255, 255, 255, 0); // Set static IP
IPAddress primaryDNS(8, 8, 8, 8);   //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional

#define RETRY_ATTEMPTS 5 // Number of retrys to connect to WiFi

// Home Assistant IP and token key
const char* ha_ip = "192.168.1.10";
uint16_t ha_port = 8123; // Could be 443 is using SSL
const char* ha_pwd = "YOURTOKEN";  //long-lived password. On HA, Profile > Long-Lived Access Tokens > Create Token

// deep sleep configurations
long SleepDuration   = 1; // Sleep time in minutes
long SleepTimer      = 0;

// Battery monitoring
int vref = 1100; // default battery vref
const uint8_t vbatPin = 34;
float VBAT = 0; // battery voltage from ESP32 ADC read

// Display setup
GxIO_Class io(SPI,  EPD_CS, EPD_DC,  EPD_RSET);
GxEPD_Class display(io, EPD_RSET, EPD_BUSY);
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

// icons
const unsigned char solarpanel [] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xc0, 0x3f, 0xff, 0xc0, 0x61, 0x08, 0x60, 0x63,
  0x0c, 0x60, 0x63, 0x0c, 0x60, 0x7f, 0xff, 0xe0, 0x7f, 0xff, 0xe0, 0x43, 0x0c, 0x20, 0xc3, 0x0c,
  0x30, 0xc2, 0x04, 0x30, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0x01, 0x98, 0x00, 0x01, 0x98, 0x00,
  0x03, 0xfc, 0x00, 0x03, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char iconwater [] PROGMEM = {

  0x0f, 0x87, 0x00, 0x1f, 0x8f, 0xc0, 0x18, 0xcc, 0xc0, 0x30, 0x6c, 0xc0, 0x12, 0x6f, 0xc0, 0x36,
  0x67, 0x80, 0x33, 0x60, 0x00, 0x32, 0x40, 0x00, 0x32, 0x60, 0x00, 0x32, 0x60, 0x00, 0x36, 0x60,
  0x00, 0x32, 0x60, 0x00, 0x66, 0x30, 0x00, 0x67, 0x30, 0x00, 0x67, 0xb0, 0x00, 0x67, 0x30, 0x00,
  0x70, 0x60, 0x00, 0x38, 0xe0, 0x00, 0x1f, 0xc0, 0x00, 0x07, 0x80, 0x00
};

char timevar[50];
StaticJsonDocument<512> doc;
static char outstr[15];
bool connectwifi()
{
  // Connect to WiFi
  TRACE("\nConnecting to Wifi\n");
  WiFi.mode(WIFI_STA);

  if (!WiFi.config(local_ip, gateway, subnet, primaryDNS, secondaryDNS))
  {
    TRACE("Wifi Failed to configure\n");
    //updateErrorDisplay("Wifi Failed to configure");
    return false;
  }
  WiFi.begin(ssid, password);

  // Wait for connection
  uint8_t count = 0;

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
    TRACE(".");
    if (count > RETRY_ATTEMPTS)
    {
      return false;
    }
    count++;
  }
  TRACE("\n");
  TRACE("Connected to ");
  TRACE(ssid);
  TRACE("\nIP address: ");
  TRACE(WiFi.localIP());
  TRACE("\nMAC address: ");
  TRACE(WiFi.macAddress());
  TRACE("\n");
  return true;
}

void DoNightSleep() {
  pinMode(16, OUTPUT);
  digitalWrite(16, HIGH);
  gpio_deep_sleep_hold_en();
  SleepTimer = (540 * 60);
  esp_sleep_enable_timer_wakeup(SleepTimer * 1000000LL); // in Secs, 1000000LL converts to Secs as unit = 1uSec
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  Serial.println("Starting deep-sleep period...");
  esp_deep_sleep_start();  // Sleep
}

void BeginSleep() {
  pinMode(16, OUTPUT);
  digitalWrite(16, HIGH);
  gpio_deep_sleep_hold_en();
  SleepTimer = (SleepDuration * 60);
  esp_sleep_enable_timer_wakeup(SleepTimer * 1000000LL); // in Secs, 1000000LL converts to Secs as unit = 1uSec
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  Serial.println("Starting deep-sleep period...");
  esp_deep_sleep_start();  // Sleep
}
void ParseTimeJsonData(String strInput) {
  TRACE(strInput);
  DeserializationError error = deserializeJson(doc, strInput);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
  } else {
    const char* timestamp = doc["state"];
        for (int i = 0; i < 16; i++){
          if (i == 10) {
            timevar[i] = ' ';
          } else {
          timevar[i] = timestamp[i];
          }
        }
        timevar[4] = '/';
        timevar[7] = '/';
        TRACE(timevar[11]);
        TRACE(timevar[12]);
        if (timevar[11] == '2' && timevar[12] == '2') {
          TRACE("night sleep");
          //DoNightSleep();
        }
  }
}
float ParseSensorJsonData(String strInput) {
  TRACE(strInput);
  int strLen = strInput.length() + 1;
  char charInput[strLen];
  strInput.toCharArray(charInput, strLen);
  DeserializationError error = deserializeJson(doc, charInput);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
  } else {
    const char* state = doc["state"];
    return atof(state);
  }
  return 0;
}

void setup() {
  Serial.begin(115200);
  TRACE("Start");
  SPI.begin(EPD_SCLK, EPD_MISO, EPD_MOSI);
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize((adc_unit_t)ADC_UNIT_1, (adc_atten_t)ADC_ATTEN_DB_2_5, (adc_bits_width_t)ADC_WIDTH_BIT_12, 1100, &adc_chars);
  
  pinMode(14, OUTPUT);
  if (!connectwifi())
  {
    BeginSleep(); // wifi failed so sleep and reset
  }
  display.init(); // enable diagnostic output on Serial
  u8g2Fonts.begin(display);
  // Start Home Assistant rest api data
  ha.setHAServer(ha_ip, ha_port);
  ha.setHAPassword(ha_pwd);
  ha.setDebugMode(false);
  
  float fltHotWater = ParseSensorJsonData( ha.sendGetHA("/api/states/sensor.hot_water_top"));
  //String val = ha.sendGetHA("/api/states/sensor.hot_water_top");
  TRACE("Hot Water Top: ");
  TRACE(fltHotWater);

  float fltSolarPVWatts = ParseSensorJsonData( ha.sendGetHA("/api/states/sensor.solarpv_total_meter_gridpower"));
  TRACE("Solar PV Watts: ");
  TRACE(fltSolarPVWatts);

  float fltSolarPVGenerated = ParseSensorJsonData( ha.sendGetHA("/api/states/sensor.solarpv_todaygenerated"));
  TRACE("Solar PV Generated: ");
  TRACE(fltSolarPVGenerated);

  ParseTimeJsonData( ha.sendGetHA("/api/states/sensor.date_time_iso"));
    TRACE(timevar);
  // End Home Assistant rest api data
 
  TRACE("Display start");
  display.setRotation(1);
  u8g2Fonts.setFontMode(1);                           // use u8g2 transparent mode (this is default)
  u8g2Fonts.setFontDirection(0);                      // left to right (this is default)
  u8g2Fonts.setForegroundColor(GxEPD_BLACK);          // apply Adafruit GFX color
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE);          // apply Adafruit GFX color
  u8g2Fonts.setFont(u8g2_font_helvR14_tf);
 
  display.fillScreen(GxEPD_WHITE);
  display.drawLine(display.width() / 2, display.height() - 40, display.width() / 2, 0, GxEPD_BLACK);

  // Solar header
  u8g2Fonts.setCursor(30, 15);                          // start writing at this position
  u8g2Fonts.print("Solar Panels");
  display.drawBitmap(0, 0, solarpanel, 20, 20, GxEPD_BLACK);
  
  // Water header
  u8g2Fonts.setCursor((display.width() / 2) + 40 , 15);
  u8g2Fonts.print("Water Temp");
  display.drawBitmap((display.width() / 2) + 10, 0, iconwater, 20, 20, GxEPD_BLACK);
  
  // Solar today generated header
  u8g2Fonts.setCursor(50, 125);  
  u8g2Fonts.print("kWh today");

  // Print last updated
  u8g2Fonts.setFont(u8g2_font_helvR10_tf);
  u8g2Fonts.setCursor((display.width() / 2) + 10 , 100);
  //for (int i = 0; i < data_lastupdate_len; i++) {
  //  u8g2Fonts.print((char)data_lastupdate[i]);
  //}
  u8g2Fonts.print(timevar);
    
  // Print water temp
  u8g2Fonts.setFont(u8g2_font_logisoso32_tf  );    // select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
  u8g2Fonts.setCursor((display.width() / 2) + 20 , 80);

  //fltHotWater
  dtostrf(fltHotWater,3, 1, outstr);
  u8g2Fonts.print(outstr);
  u8g2Fonts.print("°C");

  // Print solar watts
  u8g2Fonts.setCursor(15 , 80);
  
  dtostrf(fltSolarPVWatts,4, 1, outstr);
  u8g2Fonts.print(outstr);
  u8g2Fonts.print("W");
  
  // Print solar Kw today
  u8g2Fonts.setFont(u8g2_font_logisoso20_tf);
  u8g2Fonts.setCursor(5, 125); 
  dtostrf(fltSolarPVGenerated,2, 1, outstr);
  u8g2Fonts.print(outstr);
  
  TRACE("Display Draw Battery");
  DrawBattery((display.width() -50), 125);
  TRACE("Display Update");
  display.update();
  TRACE("Display End");
  BeginSleep();
}

void DrawBattery(int x, int y) {
  // Modified from https://github.com/CybDis/Lilygo-T5-47-HomeAssistant-Dashboard/tree/master/src
  uint8_t percentage = 100;
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
    //Serial.printf("eFuse Vref:%u mV", adc_chars.vref);
    vref = adc_chars.vref;
  }
  float voltage = analogRead(35) / 4096.0 * 6.566 * (vref / 1000.0);
  //float voltage = 4.1;
  if (voltage >= 0 ) { // Only display if there is a valid reading
    //Serial.println("\nVoltage = " + String(voltage));
    percentage = 2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3) + 255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303;
    if (voltage >= 4.20) percentage = 100;
    if (voltage <= 3.20) percentage = 0;  // orig 3.5
    display.drawRect(x, y - 15 , 40, 15, GxEPD_BLACK);
    display.fillRect(x + 40, y - 9, 4, 6, GxEPD_BLACK);
    display.fillRect(x + 2, y - 13, 36 * percentage / 100.0, 11, GxEPD_BLACK);
    
    u8g2Fonts.setFont(u8g2_font_helvR14_tf);
    u8g2Fonts.setCursor(x-40, y);
    u8g2Fonts.print(String(percentage) + "%");
    u8g2Fonts.setCursor(x-95, y);
    u8g2Fonts.print(String(voltage, 2) + "V");
  }
}

void loop() {
  // not used
}
