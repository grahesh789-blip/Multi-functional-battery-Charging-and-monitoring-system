#define BLYNK_TEMPLATE_ID "TMPL3z9_gAnPz"
#define BLYNK_TEMPLATE_NAME "MFBCMS"
#define BLYNK_AUTH_TOKEN "dEvI35NY4Q8KkRo-fgnCdqPuS1xZ8QtI"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ---- WiFi ----
char ssid[] = "GALAXY MAKER";
char pass[] = "gopesh2009";

// ---- Pins ----
const int RELAY_PIN   = 16;   // Relay control pin
const int LED_PIN     = 2;    // On-board LED indicator
const int VOLTAGE_PIN = 34;   // Analog input for voltage divider sensor
const int ACS712_PIN  = 35;   // Analog input for ACS712 current sensor
const int ONEWIRE_PIN = 4;    // DS18B20 data pin

// ---- Sensors ----
OneWire oneWire(ONEWIRE_PIN);
DallasTemperature sensors(&oneWire);

// ---- ACS712 parameters ----
const float ACS712_SENSITIVITY = 0.100; // 100 mV/A for ACS712-20A
const int   ADC_RESOLUTION     = 4095;  // ESP32 ADC resolution
const float ADC_REF_VOLTAGE    = 3.3;   // ESP32 ADC reference
float currentOffset = 0.0;              // to store zero-current offset

// ---- Voltage divider parameters ----
const float R1 = 30000.0; // Top resistor (ohms)
const float R2 = 7500.0;  // Bottom resistor (ohms)

// ---- State ----
bool charging = false;
unsigned long lastMillis = 0;

// ---- Blynk Virtual Pins ----
// V0 = Voltage
// V1 = Current
// V2 = Temperature (F)
// V3 = Power
// V4 = Charging Status (LED)
// V5 = Relay Control (Button)

BLYNK_WRITE(V5) {
  int value = param.asInt();
  charging = (value == 1);
  digitalWrite(RELAY_PIN, charging ? HIGH : LOW);
  digitalWrite(LED_PIN, charging ? HIGH : LOW);
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  sensors.begin();

  // ---- Calibrate ACS712 zero-current offset ----
  long sum = 0;
  for (int i = 0; i < 500; i++) {
    sum += analogRead(ACS712_PIN);
    delay(2);
  }
  currentOffset = sum / 500.0;
  Serial.printf("ACS712 offset calibrated at: %.2f\n", currentOffset);

  // Connect to WiFi and Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
}

// ---- Read voltage from divider ----
float readVoltage() {
  int adcValue = analogRead(VOLTAGE_PIN);
  float vOut = (adcValue * ADC_REF_VOLTAGE) / ADC_RESOLUTION;
  float vIn = vOut / (R2 / (R1 + R2));
  return vIn;
}

// ---- Read current from ACS712 ----
float readCurrent() {
  const int samples = 50;
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(ACS712_PIN);
    delayMicroseconds(200);
  }
  float adcAvg = sum / (float)samples;
  // subtract offset
  float voltageOffset = (adcAvg - currentOffset) * ADC_REF_VOLTAGE / ADC_RESOLUTION;
  float current = voltageOffset / ACS712_SENSITIVITY;
  return current;
}

// ---- Read temperature from DS18B20 ----
float readTemperatureF() {
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  if (tempC == DEVICE_DISCONNECTED_C) {
    Serial.println("Error: DS18B20 not found!");
    return -999; // error flag
  }
  return (tempC * 9.0 / 5.0) + 32.0;
}

void loop() {
  Blynk.run();

  if (millis() - lastMillis > 1000) {  // Update every 1s
    lastMillis = millis();

    // ---- Read sensors ----
    float voltage = readVoltage();
    float current = readCurrent();
    float tempF   = readTemperatureF();
    float power   = voltage * current;

    // ---- Debugging ----
    Serial.printf("V=%.2f V | I=%.2f A | T=%.2f F | P=%.2f W | Charging=%d\n",
                  voltage, current, tempF, power, charging);

    // ---- Push live data to Blynk ----
    Blynk.virtualWrite(V0, voltage);
    Blynk.virtualWrite(V1, current);
    Blynk.virtualWrite(V2, tempF);
    Blynk.virtualWrite(V3, power);
    Blynk.virtualWrite(V4, charging ? 1 : 0);
  }
}
