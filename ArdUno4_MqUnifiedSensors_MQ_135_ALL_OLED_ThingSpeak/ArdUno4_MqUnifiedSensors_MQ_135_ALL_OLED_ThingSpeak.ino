/*
  MQUnifiedsensor Library - reading an MQ135

  Demonstrates the use a MQ135 sensor.
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

//Include the library
#include <MQUnifiedsensor.h>  // https://github.com/miguel5612/MQSensorsLib
// docs: https://github.com/miguel5612/MQSensorsLib_Docs/tree/master
// https://jayconsystems.com/blog/understanding-a-gas-sensor 
// https://quartzcomponents.com/products/mq-135-air-quality-gas-sensor-module#:~:text=The%20MQ%2D135%20Gas%20sensor,the%20digital%20pin%20goes%20high.

//Definitions
#define Board "Arduino UNO"
#define Voltage_Resolution 5
#define pin A0           //Analog input 0 of your arduino
#define MQType "MQ-135"  //MQ135
//#define ADC_Bit_Resolution 10 // For arduino UNO/MEGA/NANO
#define ADC_Bit_Resolution 14   // For arduino UNO and changing the resolution with 'analogReadResolution(14); //change to 14-bit resolution', see https://docs.arduino.cc/tutorials/uno-r4-wifi/adc-resolution/
#define RatioMQ135CleanAir 3.6  //RS / R0 = 3.6 ppm
//#define calibration_button 13 //Pin to calibrate your sensor

//Declare Sensor
MQUnifiedsensor MQ135(Board, Voltage_Resolution, ADC_Bit_Resolution, pin, MQType);

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
// displaying data in blocks as the display is limited in size
// one block is the WiFi block with the IP address and the connection state
// the second block is the sensor data (temperature, humidity and air pressure)
unsigned int displayBlockCounter = 0;

float co = -1;
float alcohol = -1;
float co2 = -1;
float toluen = -1;
float nh4 = -1;
float acetone = -1;

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
  MQ135.setRegressionMethod(1);  //_PPM =  a*ratio^b

  /*****************************  MQ Init ********************************************/
  //Remarks: Configure the pin of arduino as input.
  /************************************************************************************/
  MQ135.init();

/*
  On a 'Flying Fish' module there are 3 resistors opposite of connection pins
  middle SMD resistor is '102' -> see https://www.digikey.de/de/resources/conversion-calculators/conversion-calculator-smd-resistor-code
  '102' = 1000 Ohm, this is the RL resistor value: setRL(1) // in Kilo Ohm
*/

  //If the RL value is different from 10K please assign your RL value with the following method:
  MQ135.setRL(1);

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
    MQ135.update();  // Update data, the arduino will read the voltage from the analog pin
    calcR0 += MQ135.calibrate(RatioMQ135CleanAir);
    Serial.print(".");
  }
  MQ135.setR0(calcR0 / 10);
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
  Serial.println("** Values from MQ-135 ****");
  Serial.println(F("|    CO    |  Alcohol  |    CO2    |  Toluen  |   NH4    |  Acetone |"));

  displayBlockCounter = 0;

  // connect to your local Wi-Fi network
  ConnectWiFi();
}

void loop() {
  displayCounter++;

  MQ135.update();  // Update data, the arduino will read the voltage from the analog pin

  MQ135.setA(605.18);
  MQ135.setB(-3.937);  // Configure the equation to calculate CO concentration value
  //float CO = MQ135.readSensor(); // Sensor will read PPM concentration using the model, a and b values set previously or from the setup
  co = MQ135.readSensor();  // Sensor will read PPM concentration using the model, a and b values set previously or from the setup

  MQ135.setA(77.255);
  MQ135.setB(-3.18);  //Configure the equation to calculate Alcohol concentration value
  //float Alcohol = MQ135.readSensor(); // SSensor will read PPM concentration using the model, a and b values set previously or from the setup
  alcohol = MQ135.readSensor();  // SSensor will read PPM concentration using the model, a and b values set previously or from the setup

  MQ135.setA(110.47);
  MQ135.setB(-2.862);  // Configure the equation to calculate CO2 concentration value
  //float CO2 = MQ135.readSensor(); // Sensor will read PPM concentration using the model, a and b values set previously or from the setup
  co2 = MQ135.readSensor();  // Sensor will read PPM concentration using the model, a and b values set previously or from the setup

  MQ135.setA(44.947);
  MQ135.setB(-3.445);  // Configure the equation to calculate Toluen concentration value
  //float Toluen = MQ135.readSensor(); // Sensor will read PPM concentration using the model, a and b values set previously or from the setup
  toluen = MQ135.readSensor();  // Sensor will read PPM concentration using the model, a and b values set previously or from the setup

  MQ135.setA(102.2);
  MQ135.setB(-2.473);  // Configure the equation to calculate NH4 concentration value
  //float NH4 = MQ135.readSensor(); // Sensor will read PPM concentration using the model, a and b values set previously or from the setup
  nh4 = MQ135.readSensor();  // Sensor will read PPM concentration using the model, a and b values set previously or from the setup

  MQ135.setA(34.668);
  MQ135.setB(-3.369);  // Configure the equation to calculate Acetone concentration value
  //float Aceton = MQ135.readSensor(); // Sensor will read PPM concentration using the model, a and b values set previously or from the setup
  acetone = MQ135.readSensor();  // Sensor will read PPM concentration using the model, a and b values set previously or from the setup
  Serial.print("|   ");
  Serial.print(co);
  Serial.print("   |   ");
  Serial.print(alcohol);
  // Note: 400 Offset for CO2 source: https://github.com/miguel5612/MQSensorsLib/issues/29
  /*
  Motivation:
  We have added 400 PPM because when the library is calibrated it assumes the current state of the
  air as 0 PPM, and it is considered today that the CO2 present in the atmosphere is around 400 PPM.
  https://www.lavanguardia.com/natural/20190514/462242832581/concentracion-dioxido-cabono-co2-atmosfera-bate-record-historia-humanidad.html
  */
  Serial.print("   |   ");
  Serial.print(co2 + 400);
  Serial.print("   |   ");
  Serial.print(toluen);
  Serial.print("   |   ");
  Serial.print(nh4);
  Serial.print("   |   ");
  Serial.print(acetone);
  Serial.println("   |");

  if (displayCounter > 10) {
    Serial.println(F("|    CO    |  Alcohol  |    CO2    |  Toluen  |   NH4    |  Acetone |"));
    displayCounter = 0;
  }

  currentMillis = millis();
  if (currentMillis - previousMillisDisplay > period3s) {
    if (displayBlockCounter == 0) {
      printSensorValues();
      displayValues();
      displayBlockCounter = 1;
    } else if (displayBlockCounter == 1) {
      printSensorValues();
      displayValues();
      displayBlockCounter = 0;
    }
    previousMillisDisplay = currentMillis;
  }

  if (currentMillis - previousMillisThingSpeak > period30s) {
    updateThingSpeakChannel();
    previousMillisThingSpeak = currentMillis;
  }


  /*
    Exponential regression:
  GAS      | a      | b
  CO       | 605.18 | -3.937  
  Alcohol  | 77.255 | -3.18 
  CO2      | 110.47 | -2.862
  Toluen   | 44.947 | -3.445
  NH4      | 102.2  | -2.473
  Acetone  | 34.668 | -3.369
  */

  delay(500);  //Sampling frequency
}

// ------------------------------------------------------------------
void printSensorValues() {
  display1 = "  MQ-135  Sensor";
  display2 = "CO:       " + String(co);
  display3 = "Alcohol:  " + String(alcohol);
  display4 = "CO2:      " + String(co2 + 400);
  display5 = "Toluen:   " + String(toluen);
  display6 = "NH4:      " + String(nh4);
  display7 = "Acetone:  " + String(acetone);
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
    String getData = "api_key=" + writeAPIKey + "&field1=" + String(co) + "&field2=" + String(co2+400) + "&field3=" + String(alcohol) + "&field4=" + String(toluen) + "&field5=" + String(nh4) + "&field6=" + String(acetone);

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
|    CO    |  Alcohol  |    CO2    |  Toluen  |   NH4    |  Acetone |
|   1.98   |   0.76   |   401.72   |   0.30   |   2.81   |   0.26   |
|   1.97   |   0.76   |   401.72   |   0.30   |   2.80   |   0.26   |
|   1.97   |   0.76   |   401.72   |   0.30   |   2.80   |   0.26   |
|   1.96   |   0.75   |   401.71   |   0.30   |   2.79   |   0.26   |
|   1.94   |   0.75   |   401.70   |   0.30   |   2.77   |   0.25   |
|   1.94   |   0.75   |   401.70   |   0.30   |   2.77   |   0.25   |
|   1.94   |   0.75   |   401.70   |   0.30   |   2.77   |   0.25   |
|   1.93   |   0.74   |   401.69   |   0.29   |   2.76   |   0.25   |
|   1.92   |   0.74   |   401.69   |   0.29   |   2.76   |   0.25   |
|   1.92   |   0.74   |   401.69   |   0.29   |   2.75   |   0.25   |
|   1.92   |   0.74   |   401.69   |   0.29   |   2.75   |   0.25   |

*/