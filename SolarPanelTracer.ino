/*
  Smart Solar Panel Tracking and Energy Monitoring System
  ESP32 DevKit V1 + Arduino Cloud
  
  Sensors  : LDR x2 (GPIO34, GPIO35), BH1750 (I2C), INA226 (I2C)
  Actuators: SG90 Servo (GPIO18), Relay x2 (GPIO26, GPIO27)
  Display  : OLED SSD1306 128x64 (I2C)
  I2C Bus  : SDA=GPIO21, SCL=GPIO22
*/

#include "thingProperties.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BH1750.h>
#include <INA226_WE.h>
#include <ESP32Servo.h>
#include <WiFi.h>

#define LDR_LEFT_PIN   34
#define LDR_RIGHT_PIN  35
#define SERVO_PIN      18
#define RELAY1_PIN     26
#define RELAY2_PIN     27

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define OLED_ADDR    0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define PANEL_MAX_WATT  0.9f

BH1750    lightMeter;
INA226_WE ina226 = INA226_WE(0x40);

Servo panelServo;
int currentAngle = 90;

#define LDR_THRESHOLD  150
#define NIGHT_LUX      8.0
#define SERVO_STEP     5
#define SERVO_MIN      10
#define SERVO_MAX      170
#define PARK_ANGLE     90
#define LOOP_DELAY     500

#define SHUNT_RESISTOR_OHMS  0.1f

void setup() {
  Serial.begin(9600);
  delay(1500);
  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED bulunamadi!");
  }
  display.setTextColor(SSD1306_WHITE);
  showSplash();

  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();

  if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("BH1750 bulunamadi!");
  }

  if (!ina226.init()) {
    Serial.println("INA226 bulunamadi!");
  }

  ina226.setResistorRange(SHUNT_RESISTOR_OHMS, 3.0);
  ina226.setCorrectionFactor(1.0);

  panelServo.attach(SERVO_PIN);
  panelServo.write(PARK_ANGLE);
  currentAngle = PARK_ANGLE;

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, HIGH);
  digitalWrite(RELAY2_PIN, HIGH);

  deviceMode = "AUTO";
  relayState = false;
  panelAngle = PARK_ANGLE;
}

void loop() {
  ArduinoCloud.update();
  readSensors();

  if (lux < NIGHT_LUX) {
    nightMode();
  } else if (deviceMode == "MANUAL" || deviceMode == "MANUEL") {
    manualMode();
  } else {
    autoMode();
  }

  calculateEfficiency();
  updateOLED();
  delay(LOOP_DELAY);
}

void readSensors() {
  float luxVal = lightMeter.readLightLevel();
  lux = (luxVal < 0) ? 0 : luxVal;

  float busV     = ina226.getBusVoltage_V();
  float shuntmV  = ina226.getShuntVoltage_mV();
  float calcCurrent = (shuntmV / 1000.0) / SHUNT_RESISTOR_OHMS;

  if (calcCurrent < 0) calcCurrent = 0;
  if (busV < 0) busV = 0;

  voltage = busV;
  current = calcCurrent;
  power   = voltage * current;

  // ← DEĞİŞİKLİK 3: Relay durumunu otomatik güncelle
  relayState = (digitalRead(RELAY1_PIN) == LOW);

  Serial.print("Lux: ");        Serial.print(lux, 1);
  Serial.print(" | V: ");       Serial.print(voltage, 4);
  Serial.print("V");
  Serial.print(" | ShuntmV: "); Serial.print(shuntmV, 4);
  Serial.print("mV");
  Serial.print(" | I: ");       Serial.print(current * 1000, 2);
  Serial.print("mA");
  Serial.print(" | P: ");       Serial.print(power * 1000, 2);
  Serial.print("mW");
  Serial.print(" | Mod: ");     Serial.println(deviceMode);
}

void autoMode() {
  deviceMode = "AUTO";  // ← DEĞİŞİKLİK 1

  int ldrLeft  = analogRead(LDR_LEFT_PIN);
  int ldrRight = analogRead(LDR_RIGHT_PIN);
  int diff     = ldrLeft - ldrRight;

  Serial.print(" LDR Sol: "); Serial.print(ldrLeft);
  Serial.print(" Sag: ");     Serial.println(ldrRight);

  bool leftSaturated  = (ldrLeft  >= 4090);
  bool rightSaturated = (ldrRight >= 4090);

  if (leftSaturated && rightSaturated) {
    Serial.println("LDR: Her iki sensor doymus, hareketsiz");
  } else if (abs(diff) > LDR_THRESHOLD) {
    if (diff > 0) {
      currentAngle = constrain(currentAngle + SERVO_STEP, SERVO_MIN, SERVO_MAX);
    } else {
      currentAngle = constrain(currentAngle - SERVO_STEP, SERVO_MIN, SERVO_MAX);
    }
    panelServo.write(currentAngle);
    Serial.print("Servo aci: "); Serial.println(currentAngle);
  }

  panelAngle = currentAngle;
  digitalWrite(RELAY1_PIN, LOW);
}

void manualMode() {
  deviceMode = "MANUAL";  // ← DEĞİŞİKLİK 2

  currentAngle = constrain(manuelAngle, SERVO_MIN, SERVO_MAX);
  panelServo.write(currentAngle);
  panelAngle = currentAngle;
  digitalWrite(RELAY1_PIN, LOW);
  Serial.print("MANUEL mod aci: "); Serial.println(currentAngle);
}

void nightMode() {
  deviceMode = "NIGHT";
  panelServo.write(PARK_ANGLE);
  currentAngle = PARK_ANGLE;
  panelAngle   = PARK_ANGLE;
  digitalWrite(RELAY1_PIN, HIGH);
  Serial.println("NIGHT modu aktif");
}

void calculateEfficiency() {
  float expectedPower = (lux / 1000.0) * PANEL_MAX_WATT;

  if (expectedPower > 0.01 && power > 0) {
    efficiency = (power / expectedPower) * 100.0;
    efficiency = constrain(efficiency, 0, 100);
  } else {
    efficiency = 0;
  }
}

void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print("SolarTracker");
  display.setCursor(78, 0);
  display.print("[");
  display.print(deviceMode);
  display.print("]");
  display.drawLine(0, 9, 128, 9, SSD1306_WHITE);

  // Satır 1: Lux
  display.setCursor(0, 12);
  display.print("Lux: ");
  display.print((int)lux);
  display.print(" lx");

  // Satır 2: Voltaj + Akım
  display.setCursor(0, 22);
  display.print("V:");
  display.print(voltage, 2);
  display.print("V");
  display.setCursor(64, 22);
  display.print("I:");
  display.print(current * 1000, 1);
  display.print("mA");

  // Satır 3: Güç + Verim  ← EKLENDİ
  display.setCursor(0, 32);
  display.print("P:");
  display.print(power * 1000, 1);
  display.print("mW");
  display.setCursor(64, 32);
  display.print("Eff:");
  display.print((int)efficiency);
  display.print("%");

  // Satır 4: Açı + Relay
  display.setCursor(0, 42);
  display.print("Aci:");
  display.print(panelAngle);
  display.print(" Rly:");
  display.print(relayState ? "ON" : "OFF");

  // Alt durum çizgisi
  display.drawLine(0, 53, 128, 53, SSD1306_WHITE);
  display.setCursor(0, 56);
  display.print("WiFi:");
  display.print(WiFi.isConnected() ? "OK" : "--");
  display.print(" Cloud:");
  display.print(ArduinoCloud.connected() ? "OK" : "--");

  display.display();
}

void showSplash() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(15, 10);
  display.print("Solar Tracker");
  display.setCursor(20, 25);
  display.print("ESP32 + Cloud");
  display.setCursor(10, 40);
  display.print("Baslatiliyor...");
  display.display();
  delay(2000);
}

// ===== CLOUD CALLBACK FONKSİYONLARI =====

void onDeviceModeChange() {
  Serial.print("Mod degisti: ");
  Serial.println(deviceMode);
  if (deviceMode == "NIGHT") deviceMode = "AUTO";
}

void onManuelAngleChange() {
  Serial.print("Manuel aci geldi: ");
  Serial.println(manuelAngle);
  if (deviceMode != "MANUAL" && deviceMode != "MANUEL") {
    deviceMode = "MANUAL";
  }
}

// ← DEĞİŞİKLİK 4: onRelayStateChange kaldırıldı
// Relay artık sadece mod (AUTO/MANUAL/NIGHT) tarafından kontrol ediliyor
