/*
  Burning-in an MQ-X sensor
  
  For new sensor it is essential to 'burn-in' the sensor for at least 48 hours by providing
  the heater with 5 volts and make some measures on the resistor = sensor.

  When running this sketch you will see the decreasing values in the ThingSpeak dashboard.

  This sketch isn't sensor type depending as it does not give out values for specific gases,
  it just reads the input voltage on AnalogInputPin A0.

  The Arduino R4 Wi-Fi can use an Analog-Digital Converter (ADC) with expanded resolution:
  As per default it has a 10 Bit resolution (0..4095), but I'm using a 14 Bit resolution with
  a range of (0..16383).

  The data shown on the display is updated every 3 seconds, but the ThingSpeak channel is updated 
  every 30 seconds, so after 48 hours we get 48 * 60 * 2 = 5760 measurements.

  Wiring:
  https://github.com/miguel5612/MQSensorsLib_Docs/blob/master/static/img/MQ_Arduino.PNG
  
  Please make sure arduino A0 pin represents the analog input configured on #define Analog_Input_Pin

  This example code is in the public domain.

*/

// We do not need a library as we are consuming the raw values. The voltage is calculated on the
// 'Voltage_Resolution' of 5 V

#define Voltage_Resolution 5   // 0..5 V
#define Analog_Input_Pin A0    // Analog input pin 0 of your arduino
#define ADC_Bit_Resolution 14  // For arduino UNO R4 Wi-Fi only: changing the resolution with 'analogReadResolution(14); //change to 14-bit resolution', see https://docs.arduino.cc/tutorials/uno-r4-wifi/adc-resolution/

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

unsigned int displayCounter = -1;
float analogInputReading = -1;
float voltageCalculated = -1;
unsigned int thingSpeakUpdates = 0;
// ------------------------------------------------------------------
// vars for "delay" without blocking the loop
const unsigned long period1s = 1000;    //the value is a number of milliseconds, ie 1 second
const unsigned long period3s = 3000;    //the value is a number of milliseconds, ie 3 seconds
const unsigned long period10s = 10000;  //the value is a number of milliseconds, ie 10 seconds
const unsigned long period20s = 20000;  //the value is a number of milliseconds, ie 20 seconds
const unsigned long period30s = 30000;  //the value is a number of milliseconds, ie 30 seconds
const unsigned long period60s = 60000;  //the value is a number of milliseconds, ie 60 seconds
unsigned long currentMillis;
unsigned long previousMillisDisplay = 0;
unsigned long previousMillisThingSpeak = 0;
// ------------------------------------------------------------------

void setup() {
  //Init the serial port communication - to debug the library
  Serial.begin(115200);  //Init serial port

  analogReadResolution(ADC_Bit_Resolution);  //change to 14-bit resolution
  // analogReadResolution(10);  //change to 10-bit resolution = default

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
  Serial.println(F("** Values from the MQ-Sensor ****"));
  Serial.println(F("|   Analog   |  Voltage  |"));

  // connect to your local Wi-Fi network
  ConnectWiFi();
}

void loop() {
  displayCounter++;

  analogInputReading = analogRead(Analog_Input_Pin);  // maximum is 16.383 if 14 bit resolution is in use or 1024 in 10 bit mode
  voltageCalculated = getVoltageFromAdc(analogInputReading, Voltage_Resolution, ADC_Bit_Resolution);


  Serial.print(F("|   "));
  Serial.print(analogInputReading);
  Serial.print(F("   |   "));
  Serial.print(voltageCalculated);
  Serial.println(F("   |"));

  if (displayCounter > 10) {
    Serial.println(F("|   Analog   |  Voltage  |"));
    displayCounter = 0;
  }

  currentMillis = millis();
  if (currentMillis - previousMillisDisplay > period3s) {
    printSensorValues();
    displayValues();
    previousMillisDisplay = currentMillis;
  }

  if (currentMillis - previousMillisThingSpeak > period30s) {
    thingSpeakUpdates++;
    updateThingSpeakChannel();
    previousMillisThingSpeak = currentMillis;
  }

  delay(1000);  //Sampling frequency
}

// ------------------------------------------------------------------
void printSensorValues() {
  display1 = "  MQ-Sensor Burn-In";
  display2 = "Analog Read: " + String(analogInputReading);
  display3 = "Voltage Cal:  " + String(voltageCalculated);
  display4 = "ThingSp Upd: " + String(thingSpeakUpdates);
  display5 = "";
  display6 = "";
  display7 = "";
}

void displayValues() {
  display.clearDisplay();
  display.setTextSize(1);               // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);  // Draw white text
  display.setCursor(0, 0);              // Start at top-left corner
  display.println(display1);
  display.setCursor(0, 10);
  display.println(display2);
  display.setCursor(0, 20);
  display.println(display3);
  display.setCursor(0, 30);
  display.println(display4);
  /*
  display.setCursor(0, 32);
  display.println(display5);
  display.setCursor(0, 40);
  display.println(display6);
  display.setCursor(0, 48);
  display.println(display7);
  */
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
    String getData = "api_key=" + writeAPIKey + "&field1=" + String(analogInputReading) + "&field2=" + String(voltageCalculated) + "&field3=" + String(thingSpeakUpdates);

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

/*
Usage
int vRead = analogRead(A0); // maximum is 16.383 if 14 bit resolution is in use or 1024 in 10 bit mode
voltage = getVoltageFromAdc(vRead, 5.0, 14); // 5.0 = Arduino sensor driven with 5 volt, 14 bit resolution
*/
float getVoltageFromAdc(int value, float voltage, int adc_Bit_Resolution) {
  return (value)*voltage / ((pow(2, adc_Bit_Resolution)) - 1);
}

// ------------------------------------------------------------------
