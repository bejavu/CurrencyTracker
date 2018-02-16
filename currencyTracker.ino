/*
Copyright 2017 Google LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. 
*/
#include <FS.h>

#include <ESP8266WiFi.h> 
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include "Wire.h"
#include <LiquidCrystal_I2C.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <Servo.h>
#include <Time.h> 

#define SERVO_PIN 14

#define CENTER_POSITION 90

#define ACTION_LED_PIN LED_BUILTIN
#define CONNECTION_LED_PIN 2
#define BUTTON_PIN 12
#define RED_PIN 13
#define GREEN_PIN 15
#define BOUNCE_DURATION 200
#define RESET_DURATION 3000

// template< typename T, size_t N > size_t ArraySize(T (&) [N]){ return N; }

//flag for saving data
bool shouldSaveConfig = false;

char *AP_SSID = "CryptoTracker_AP";
String currencies_str = "Bitcoin,Ethereum,ZCash,Dragonchain";

// String currencies[] = {"Bitcoin", "Ethereum", "ZCash", "Dragonchain"};
String *currencies;
int currencies_length = 0;


int currencyIndex = 0;
int currentCurrency = 0;

volatile unsigned long buttonTimer = 0;
volatile unsigned long bounceTime = 0;
volatile bool buttonActive = false;
volatile bool interrupted = false;
volatile bool resetInterrupted = false;

WiFiClientSecure client;
const int httpsPort = 443;

double priceChange = 0;
double priceUsd = 0;
double priceBtc = 0;
String symbol = "";

Servo myservo;
int currentServoPosition = 0;
String mostRecentDateString = "";

LiquidCrystal_I2C lcd(0x27, 16, 2);

WiFiManager wifiManager;

void split_currencies(String currencies_str) {
  int index = 0;
  currencies_length = 1;
  while((index = currencies_str.indexOf(',', index + 1)) != -1) {
    currencies_length++;
  }

  currencies = new String[currencies_length];

  int start = 0;
  for (int i = 0; i < currencies_length; i++) {
    index = currencies_str.indexOf(',', start);
    if (start != -1) {
      currencies[i] = currencies_str.substring(start, index);
    }
    else {
      currencies[i] = currencies_str.substring(start);
    }
    start = index + 1;
  }
}

void wifiConnect () {
  delay(200);
  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if(!SPIFFS.begin()) {
    Serial.println("failed to mount FS");
    return;
  }
  Serial.println("mounted file system");
  if (SPIFFS.exists("/config.json")) {
    //file exists, reading and loading
    Serial.println("reading config file");
    File configFile = SPIFFS.open("/config.json", "r");
    if (configFile) {
      Serial.println("opened config file");
      size_t size = configFile.size();
      // Allocate a buffer to store contents of the file.
      std::unique_ptr<char[]> buf(new char[size]);

      configFile.readBytes(buf.get(), size);
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.parseObject(buf.get());
      json.printTo(Serial);
      if (json.success()) {
        currencies_str = json["currencies_str"].asString();
      } else {
        Serial.println("failed to load json config");
      }
    }
  }

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_currencies("Custom Currencies", "Custom Currencies", currencies_str.c_str(), 200);

  wifiManager.setDebugOutput(false);

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  wifiManager.addParameter(&custom_currencies);

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  Serial.println();
  Serial.print("Opening AP with name: ");
  Serial.println(AP_SSID);
  lcd.clear();
  lcd.print("Connect to:");
  lcd.setCursor(0,1);
  lcd.print(AP_SSID);
  if (!wifiManager.autoConnect(AP_SSID)) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  lcd.clear();
  lcd.print("WiFi connected");
  lcd.setCursor(0,1);
  lcd.print(WiFi.localIP());

  //read updated parameters
  currencies_str = custom_currencies.getValue();

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["currencies_str"] = currencies_str;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  split_currencies(currencies_str);
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setupIndicatorLEDs()
{
    pinMode(ACTION_LED_PIN, OUTPUT);
    pinMode(CONNECTION_LED_PIN, OUTPUT);
}

void SetActionLEDOn()
{
    digitalWrite(ACTION_LED_PIN, false);
}

void SetActionLEDOff()
{
   digitalWrite(ACTION_LED_PIN, true); 
}

void SetConnectionLEDOn()
{
   digitalWrite(CONNECTION_LED_PIN, false); 
}

void SetConnectionLEDOff()
{
    digitalWrite(CONNECTION_LED_PIN, true); 
}

void handleInterrupt() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!buttonActive) {
      buttonActive = true;
      buttonTimer = millis();
    }
  } else {
    if (buttonActive) {
      buttonActive = false;
      if (millis() - buttonTimer > RESET_DURATION) {
        // Serial.println("LONG PRESS");
        interrupted = true;
        resetInterrupted = true;
      } else {
        if(millis() > bounceTime) {
          bounceTime = millis() + BOUNCE_DURATION;
          // Serial.println("SORT PRESS");
          interrupted = true;
          currencyIndex++;
          currencyIndex = currencyIndex % (currencies_length + 1);
          StartUp();
        }
      }
    }
  }
}

String makeLessPrettyJSON(String JSONData)
{
  String notPretty = "";
  for(int i = 0; i < JSONData.length(); i++)
  {
    if(JSONData.charAt(i) != '\n' && JSONData.charAt(i) != '\r' &&
      JSONData.charAt(i) != ' ' && JSONData.charAt(i) != '  ' &&
      JSONData.charAt(i) != '[' && JSONData.charAt(i) != ']')
    {
      notPretty += JSONData.charAt(i);
    }
  }

  return notPretty;
}

void StartUp()
{
  digitalWrite(RED_PIN, LOW);
  digitalWrite(GREEN_PIN, LOW);
  MoveServoToPosition(CENTER_POSITION, 10); // Initialize
  lcd.clear();
  lcd.print("Tracking");
  lcd.setCursor(0,1);
  Serial.print("currencies_length: ");
  Serial.println(currencies_length);
  if (currencyIndex < currencies_length) {
    currentCurrency = currencyIndex;
    lcd.print(currencies[currentCurrency]);
  } else {
    currentCurrency = 0;
    lcd.print("All Currencies");
  }
}

void MoveServoToPosition(int position, int speed)
{
  if(position < currentServoPosition)
  {
    for(int i = currentServoPosition; i > position && !interrupted; i--)
    {
      myservo.write(i);
      delay(speed);
    }
  }
  else if(position > currentServoPosition)
  {
    for(int i = currentServoPosition; i < position && !interrupted; i++)
    {
      myservo.write(i);
      delay(speed);
    }
  }

  currentServoPosition = position;
}

void GetCurrency()
{
    char* host = "api.coinmarketcap.com";
    String url = "/v1/ticker/" + currencies[currentCurrency] + "/";
    String payload = getJson(host, url);

    String unPretty = makeLessPrettyJSON(payload);

    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(unPretty);

    // Test if parsing succeeds.
    if (!root.success()) {
      Serial.println("parseObject() failed");
    }

    priceChange = (double)root[String("percent_change_24h")];
    priceUsd = (double)root[String("price_usd")];
    priceBtc = (double)root[String("price_btc")];
    symbol = root[String("symbol")].asString();
}

void updateHardware()
{
  // Serial.print("priceChange ");
  // Serial.println(priceChange);

  // Serial.print("priceUsd ");
  // Serial.println(priceUsd);

  // Serial.print("priceBtc ");
  // Serial.println(priceBtc);

  if (priceChange >= 0) {
    digitalWrite(GREEN_PIN, HIGH);
    digitalWrite(RED_PIN, LOW);
  } else {
    digitalWrite(GREEN_PIN, LOW);
    digitalWrite(RED_PIN, HIGH);
  }


  lcd.clear();
  lcd.print(symbol);
  lcd.setCursor(0,1);
  lcd.print(priceChange);
  lcd.print("%");

  lcd.setCursor(7,0);
  int unit = 0;
  if (priceUsd < 0.1) {
    unit++;
    priceUsd = priceUsd * 1000;
  }
  if (priceUsd < 0.1) {
    unit++;
    priceUsd = priceUsd * 1000;
  }
  lcd.print(priceUsd);
  lcd.setCursor(11,0);
  lcd.print(" ");
  if (unit == 1) {
    lcd.print("u");  
  }
  if (unit == 2) {
    lcd.print("n");  
  }
  lcd.print("USD");
  lcd.setCursor(7,1);
  unit = 0;
  if (priceBtc < 0.1) {
    unit++;
    priceBtc = priceBtc * 1000;
  }
  if (priceBtc < 0.1) {
    unit++;
    priceBtc = priceBtc * 1000;
  }
  lcd.print(priceBtc);
  lcd.setCursor(11,1);
  lcd.print(" ");
  if (unit == 1) {
    lcd.print("u");  
  }
  if (unit == 2) {
    lcd.print("n");  
  }
  lcd.print("BTC");

  double change = priceChange;

  if (change > 20) {
    change = 20;
  } else if (change < -20) {
    change = -20;
  }

  int position = map(change, -20, 20, 0, 180);

  // Serial.print("change ");
  // Serial.println(change);

  // Serial.print("position ");
  // Serial.println(position);

  MoveServoToPosition(position, 10);
}

String getJson(String host, String url){

    Serial.print("connecting to ");
    Serial.println(host);

    if (!client.connect(host.c_str(), httpsPort)) {
      Serial.println("connection failed");
      return "bad";
    }

    Serial.print("requesting URL ");
    Serial.println(url);

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "User-Agent: BuildFailureDetectorESP8266\r\n" +
                 "Connection: close\r\n\r\n");

    Serial.println("request sent");

    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if(line.indexOf("Date") != -1)
      {
        mostRecentDateString = line;
      }
      // Serial.println(line);
      if (line == "\r") {
        Serial.println("headers received");
        break;
      }
    }

    String payload = client.readStringUntil(']');
    Serial.println("payload received");

    #ifdef PRINT_PAYLOAD
        Serial.println("reply was:");
        Serial.println("==========");
        Serial.println(payload);
        Serial.println("==========");
    #endif
    return payload;
}

void RunCurrencyTracker()
{
  Serial.println("Tracking " + currencies[currentCurrency]);
  GetCurrency();
  if (interrupted) {
    return;
  }
  Serial.println("updateHardware");
  updateHardware();
  if (currencyIndex == currencies_length) {
    currentCurrency++;
    currentCurrency = (currentCurrency % currencies_length); 
  }
}


void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  myservo.attach(SERVO_PIN);

  // client.setTimeout(200);

  lcd.begin(); // initialize screen
  lcd.backlight(); // turn backlight on


  setupIndicatorLEDs();
  SetActionLEDOn();    
  SetConnectionLEDOff();

  wifiConnect();
  
  SetConnectionLEDOn();

  StartUp();

  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleInterrupt, CHANGE);
}

void loop() {
  if (resetInterrupted) {
    // reset settings
    wifiManager.resetSettings();
    delay(2500);
    // reset device.
    Serial.println("reset device.");
    ESP.reset();
  }

  interrupted = false;

  SetActionLEDOn();

  // wait for WiFi connection
  if(WiFi.status() == WL_CONNECTED) {
    SetConnectionLEDOn();
    RunCurrencyTracker();
  }
  else{
    Serial.println("not connected");
    SetConnectionLEDOff();
    wifiConnect();
  }

  SetActionLEDOff();

  if (!interrupted) {
    delay(5000);
  }

}




