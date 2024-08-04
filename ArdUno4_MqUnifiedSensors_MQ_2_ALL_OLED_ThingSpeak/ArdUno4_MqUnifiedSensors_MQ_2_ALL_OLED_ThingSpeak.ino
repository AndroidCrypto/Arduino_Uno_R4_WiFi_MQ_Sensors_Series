/*
  MQUnifiedsensor Library - reading an MQ2

  Demonstrates the use a MQ2 sensor.
  Library originally added 01 may 2019
  by Miguel A Califa, Yersson Carrillo, Ghiordy Contreras, Mario Rodriguez
 
  Added example
  modified 23 May 2019
  by Miguel Califa 

  Updated library usage
  modified 26 March 2020
  by Miguel Califa 

  Wiring:
  https://github.com/miguel5612/MQSensorsLib_Docs/blob/master/static/img/MQ_Arduino.PNG
  Please make sure arduino A0 pin represents the analog input configured on #define pin

  This example code is in the public domain.

*/

// Include the library
#include <MQUnifiedsensor.h>  // https://github.com/miguel5612/MQSensorsLib
// docs: https://github.com/miguel5612/MQSensorsLib_Docs/
// https://jayconsystems.com/blog/understanding-a-gas-sensor 

// Definitions
#define Board "Arduino UNO"
#define Voltage_Resolution 5
#define pin A0           //Analog input 0 of your arduino
#define MQType "MQ-2"  //MQ2
//#define ADC_Bit_Resolution 10 // For arduino UNO/MEGA/NANO
#define ADC_Bit_Resolution 14   // For arduino UNO and changing the resolution with 'analogReadResolution(14); // change to 14-bit resolution', see https://docs.arduino.cc/tutorials/uno-r4-wifi/adc-resolution/
#define RatioMQ2CleanAir (9.83) //RS / R0 = 60 ppm
//#define calibration_button 13 //Pin to calibrate your sensor

//Declare Sensor
MQUnifiedsensor MQ2(Board, Voltage_Resolution, ADC_Bit_Resolution, pin, MQType);

float h2 = -1;
float lpg = -1;
float co = -1;
float alcohol = -1;
float propane = -1;

  /*
    Exponential regression:
    Gas    | a      | b
    H2     | 987.99 | -2.162
    LPG    | 574.25 | -2.222
    CO     | 36974  | -3.109
    Alcohol| 3616.1 | -2.675
    Propane| 658.71 | -2.168
  */

// ------------------------------------------------------------------
// WiFi
#include <WiFiS3.h>
WiFiClient client;
const char ssid[] = "YOUR_WIFI_SSID";  // change your network SSID
const char pass[] = "YOUR_WIFI_PASSWORD";   // change your network password

int status = WL_IDLE_STATUS;

// ------------------------------------------------------------------
// ThingSpeak Channel
const String writeAPIKey = "YOUR_THINGSPEAK_API_KEY";

// ------------------------------------------------------------------
// OLED
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library.
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET -1        // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C  ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

String display1 = "";
String display2 = "";
String display3 = "";
String display4 = "";
String display5 = "";
String display6 = "";
String display7 = "";

// ------------------------------------------------------------------
// vars for "delay" without blocking the loop
const unsigned long period1s = 1000;    //the value is a number of milliseconds, ie 1 second
const unsigned long period3s = 3000;    //the value is a number of milliseconds, ie 3 seconds
const unsigned long period10s = 10000;  //the value is a number of milliseconds, ie 10 seconds
const unsigned long period20s = 20000;  //the value is a number of milliseconds, ie 20 seconds
const unsigned long period30s = 30000;  //the value is a number of milliseconds, ie 30 seconds
unsigned long currentMillis;
unsigned long previousMillisDisplay = 0;
unsigned long previousMillisThingSpeak = 0;
// ------------------------------------------------------------------

int displayCounter = 0;

void setup() {
  //Init the serial port communication - to debug the library
  Serial.begin(115200);  //Init serial port

  analogReadResolution(14);  //change to 14-bit resolution

  // ------------------------------------------------------------------
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000);  // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();
  // ------------------------------------------------------------------

  //Set math model to calculate the PPM concentration and the value of constants
  MQ2.setRegressionMethod(1);  //_PPM =  a*ratio^b

  /*****************************  MQ Init ********************************************/
  //Remarks: Configure the pin of arduino as input.
  /************************************************************************************/
  MQ2.init();

/*
  On a 'Flying Fish' module there are 3 resistors opposite of connection pins
  middle SMD resistor is '102' -> see https://www.digikey.de/de/resources/conversion-calculators/conversion-calculator-smd-resistor-code
  '102' = 1000 Ohm, this is the RL resistor value: setRL(1) // in Kilo Ohm
*/

  //If the RL value is different from 10K please assign your RL value with the following method:
  MQ2.setRL(1);

  /*****************************  MQ CAlibration ********************************************/
  // Explanation:
  // In this routine the sensor will measure the resistance of the sensor supposedly before being pre-heated
  // and on clean air (Calibration conditions), setting up R0 value.
  // We recomend executing this routine only on setup in laboratory conditions.
  // This routine does not need to be executed on each restart, you can load your R0 value from eeprom.
  // Acknowledgements: https://jayconsystems.com/blog/understanding-a-gas-sensor
  Serial.print("Calibrating please wait.");
  float calcR0 = 0;
  for (int i = 1; i <= 10; i++) {
    MQ2.update();  // Update data, the arduino will read the voltage from the analog pin
    calcR0 += MQ2.calibrate(RatioMQ2CleanAir);
    Serial.print(".");
  }
  MQ2.setR0(calcR0 / 10);
  Serial.println("  done!.");

  if (isinf(calcR0)) {
    Serial.println("Warning: Conection issue, R0 is infinite (Open circuit detected) please check your wiring and supply");
    while (1)
      ;
  }
  if (calcR0 == 0) {
    Serial.println("Warning: Conection issue found, R0 is zero (Analog pin shorts to ground) please check your wiring and supply");
    while (1)
      ;
  }
  /*****************************  MQ CAlibration ********************************************/
  Serial.println("** Values from  MQ-2  ****");
  Serial.println(F("|    H2    |    LPG    |    CO    |  Alcohol |  Benzene |  Propane |"));

  // connect to your local Wi-Fi network
  ConnectWiFi();
}

void loop() {
  displayCounter++;
  MQ2.update();  // Update data, the arduino will read the voltage from the analog pin

  MQ2.setA(987.99);
  MQ2.setB(-2.162); 
  h2 = MQ2.readSensor();

  MQ2.setA(574.25);
  MQ2.setB(-2.222); 
  lpg = MQ2.readSensor();  // Sensor will read PPM concentration using the model, a and b values set previously or from the setup

  MQ2.setA(36974);
  MQ2.setB(-3.109);
  co = MQ2.readSensor();

  MQ2.setA(3616.1);
  MQ2.setB(-2.675);
  alcohol = MQ2.readSensor();

  MQ2.setA(7585.3);
  MQ2.setB(-2.168);
  propane = MQ2.readSensor();

  Serial.print("|   ");
  Serial.print(h2);
  Serial.print("   |   ");
  Serial.print(lpg);
  Serial.print("   |   ");
  Serial.print(co);
  Serial.print("   |   ");
  Serial.print(alcohol);
  Serial.print("   |   ");
  Serial.print(propane);
  Serial.println("   |");

  if (displayCounter > 10) {
    Serial.println(F("|    H2    |    LPG    |    CO    |  Alcohol |  Propane |"));
    displayCounter = 0;
  }

  currentMillis = millis();
  if (currentMillis - previousMillisDisplay > period3s) {
    printSensorValues();
    displayValues();
    previousMillisDisplay = currentMillis;
  }

  if (currentMillis - previousMillisThingSpeak > period30s) {
    updateThingSpeakChannel();
    previousMillisThingSpeak = currentMillis;
  }

  /*
    Exponential regression:
    Gas    | a      | b
    H2     | 987.99 | -2.162
    LPG    | 574.25 | -2.222
    CO     | 36974  | -3.109
    Alcohol| 3616.1 | -2.675
    Propane| 658.71 | -2.168
  */

  delay(1000);  //Sampling frequency
}

// ------------------------------------------------------------------
void printSensorValues() {
  display1 = "  MQ-2    Sensor";
  display2 = "H2:       " + String(h2);
  display3 = "LPG:      " + String(lpg);
  display4 = "CO:       " + String(co);
  display5 = "Alcohol:  " + String(alcohol);
  display6 = "Propane:  " + String(propane);
}

void displayValues() {
  display.clearDisplay();
  display.setTextSize(1);               // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);  // Draw white text
  display.setCursor(0, 0);              // Start at top-left corner
  display.println(display1);
  display.setCursor(0, 8);
  display.println(display2);
  display.setCursor(0, 16);
  display.println(display3);
  display.setCursor(0, 24);
  display.println(display4);
  display.setCursor(0, 32);
  display.println(display5);
  display.setCursor(0, 40);
  display.println(display6);
  display.setCursor(0, 48);
  display.println(display7);
  display.display();
}

// ------------------------------------------------------------------
// A WiFi connection is necessary for ThingSpeak integration

void ConnectWiFi() {
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println(F("Communication with WiFi module failed!"));
    while (true)
      ;
  }
  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println(F("Please upgrade the firmware"));
  }
  // Attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    Serial.print(F("Attempting to connect to WPA SSID: "));
    Serial.println(ssid);
    // Connect to WPA/WPA2 network:
    status = WiFi.begin(ssid, pass);
    // wait 10 seconds for connection:
    delay(10000);
  }
  // You're connected now, so print out the data:
  Serial.println(F("You're connected to Wifi"));
  PrintNetwork();
}

void PrintNetwork() {
  Serial.print(F("WiFi Status: "));
  Serial.println(WiFi.status());
  Serial.print(F("SSID: "));
  Serial.println(WiFi.SSID());
  IPAddress ip = WiFi.localIP();
  Serial.print(F("IP Address: "));
  Serial.println(ip);
}

// ------------------------------------------------------------------

void updateThingSpeakChannel() {
  // Write a Channel Feed: GET https://api.thingspeak.com/update?api_key=VM8FY781OJVQG4HE&field1=0
  const char server[] = "api.thingspeak.com";
  
  if (client.connect(server, 80)) {
    String getData = "api_key=" + writeAPIKey + "&field1=" + String(h2) + "&field2=" + String(lpg) + "&field3=" + String(co) + "&field4=" + String(alcohol) + "&field5=" + String(propane);

    client.println("GET /update HTTP/1.1");
    client.println("Host: api.thingspeak.com");
    client.println("Connection: close");
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.println("Content-Length: " + String(getData.length()));
    client.println();
    client.println(getData);
  } else {
    Serial.println("Connection Failed");
  }
}

// ------------------------------------------------------------------

/*
|    H2    |    LPG    |    CO    |  Alcohol |  Propane |
|   5.50   |   2.77   |   21.16   |   5.87   |   41.59   |
|   5.48   |   2.76   |   21.06   |   5.84   |   41.44   |
|   5.48   |   2.76   |   21.10   |   5.85   |   41.51   |
|   5.45   |   2.74   |   20.93   |   5.81   |   41.27   |
|   5.43   |   2.73   |   20.82   |   5.79   |   41.13   |
|   5.42   |   2.72   |   20.72   |   5.76   |   40.98   |
|   5.42   |   2.72   |   20.72   |   5.76   |   40.98   |
|   5.40   |   2.71   |   20.61   |   5.74   |   40.83   |
|   5.40   |   2.72   |   20.62   |   5.74   |   40.85   |
|   5.38   |   2.70   |   20.50   |   5.71   |   40.68   |
|   5.37   |   2.70   |   20.47   |   5.70   |   40.64   |
*/
