# CurrencyTracker
### Arduino project for currency tacker

This Currency tracker is connected automatically to a WiFI network or create a new AP for quering the user for its SSID and Password along with a list of currencies to track.
After Its connected It will show on the screen the current price (in usd and btc) and 24 hour change of the selected currency.
The Servo engine will rotate the arrow to indicate the change (-20%-20% change maps to 0 - 180 degrees).
The green\red LEDs will lit if the change is grater than\lower that zero.
Using the button you can switch between currencies to track and a rotating between all currencies mode.
Long press the button will reset the device and its settings (wifi network and currencies list).

## Parts needed:
* Assembled Adafruit Feather HUZZAH with ESP8266 WiFi With Headers, [can be found here][feather]
* Micro servo.
* Simple button.
* 4 LEDs, 2 red LEDs, two green LEDs.
* Optional: Lithium Ion Polymer Battery - 3.7v

## Connect it all together:
![Currency Tracker](https://github.com/bejavu/CurrencyTracker/raw/master/CurrenctTracker_bb.png)

## Deploy the code:
you will need to install these libraries:
* [ArduinoJson][ArduinoJson]
* [WiFiManager][WiFiManager]
* [Servo][Servo]
* [LiquidCrystal_I2C][LiquidCrystal_I2C]

Than Compile and upload the code from: `currencyTracker.ino`.

## Put in in a box:
You can print a 3D box and arrow for this and make a real desktop Currency Tracker.
Solid sketch will be uploaded soon.




   [feather]: <https://www.adafruit.com/product/3046>
   [WiFiManager]: <https://github.com/tzapu/WiFiManager>
   [ArduinoJson]: <https://github.com/bblanchon/ArduinoJson>
   [Servo]: <https://github.com/arduino-libraries/Servo>
   [LiquidCrystal_I2C]: <https://github.com/fdebrabander/Arduino-LiquidCrystal-I2C-library>
