/*
  SpeScan-PV - ESP32 firmware
  Photovoltaic spectral response scanner prototype

  Hardware:
  - ESP32 / ESP32-PICO-DevKitM-2
  - INA219 current/voltage sensor (I2C)
  - DS3218 servo motor controlling diffraction grating
  - Optional Nextion display (UART)

  IMPORTANT:
  The wavelength-to-servo calibration table below contains STARTING VALUES ONLY.
  It MUST be replaced by experimentally measured calibration points for the real
  optical geometry of the prototype.
*/

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <ESP32Servo.h>

// ---------------- Pin configuration ----------------
static const int I2C_SDA = 21;
static const int I2C_SCL = 22;
static const int SERVO_PIN = 25;
static const int NEXTION_RX = 32;  // ESP32 RX <- Nextion TX
static const int NEXTION_TX = 33;  // ESP32 TX -> Nextion RX

// ---------------- Measurement configuration ----------------
static const uint16_t LAMBDA_MIN_NM = 400;
static const uint16_t LAMBDA_MAX_NM = 1100;
static const uint16_t LAMBDA_STEP_NM = 10;
static const uint16_t SETTLE_TIME_MS = 500;
static const uint8_t SAMPLE_COUNT = 10;
static const uint16_t SAMPLE_INTERVAL_MS = 20;

// DS3218 270-degree servo: typical pulse range. Verify on your exact servo.
static const int SERVO_MIN_US = 500;
static const int SERVO_MAX_US = 2500;
static const float SERVO_MAX_DEG = 270.0f;

// ---------------- Optical calibration ----------------
// Replace these example anchors after calibration with a reference spectrometer
// or known spectral lines. Angles must be monotonic with wavelength.
struct CalibrationPoint {
  float wavelengthNm;
  float servoDeg;
};

CalibrationPoint calibration[] = {
  {400.0f,  55.0f},
  {550.0f,  80.0f},
  {700.0f, 105.0f},
  {850.0f, 130.0f},
  {1100.0f, 170.0f}
};
static const size_t CAL_COUNT = sizeof(calibration) / sizeof(calibration[0]);

Adafruit_INA219 ina219;
Servo gratingServo;
HardwareSerial nextion(2);

volatile bool stopRequested = false;
bool inaOk = false;

struct Measurement {
  float busVoltageV;
  float shuntVoltageMv;
  float loadVoltageV;
  float currentMa;
  float powerMw;
};

float interpolateServoAngle(float wavelengthNm) {
  if (wavelengthNm <= calibration[0].wavelengthNm) return calibration[0].servoDeg;
  if (wavelengthNm >= calibration[CAL_COUNT - 1].wavelengthNm) return calibration[CAL_COUNT - 1].servoDeg;

  for (size_t i = 0; i < CAL_COUNT - 1; ++i) {
    const CalibrationPoint &a = calibration[i];
    const CalibrationPoint &b = calibration[i + 1];
    if (wavelengthNm >= a.wavelengthNm && wavelengthNm <= b.wavelengthNm) {
      float t = (wavelengthNm - a.wavelengthNm) / (b.wavelengthNm - a.wavelengthNm);
      return a.servoDeg + t * (b.servoDeg - a.servoDeg);
    }
  }
  return calibration[0].servoDeg;
}

int angleToPulseUs(float angleDeg) {
  angleDeg = constrain(angleDeg, 0.0f, SERVO_MAX_DEG);
  return (int)(SERVO_MIN_US + (angleDeg / SERVO_MAX_DEG) * (SERVO_MAX_US - SERVO_MIN_US));
}

void setWavelength(float wavelengthNm) {
  float angleDeg = interpolateServoAngle(wavelengthNm);
  int pulseUs = angleToPulseUs(angleDeg);
  gratingServo.writeMicroseconds(pulseUs);
  delay(SETTLE_TIME_MS);
}

Measurement readAveragedMeasurement() {
  Measurement m = {0, 0, 0, 0, 0};
  if (!inaOk) return m;

  for (uint8_t i = 0; i < SAMPLE_COUNT; ++i) {
    float shuntMv = ina219.getShuntVoltage_mV();
    float busV = ina219.getBusVoltage_V();
    float currentMa = ina219.getCurrent_mA();
    float powerMw = ina219.getPower_mW();

    m.shuntVoltageMv += shuntMv;
    m.busVoltageV += busV;
    m.loadVoltageV += busV + shuntMv / 1000.0f;
    m.currentMa += currentMa;
    m.powerMw += powerMw;
    delay(SAMPLE_INTERVAL_MS);
  }

  const float n = (float)SAMPLE_COUNT;
  m.shuntVoltageMv /= n;
  m.busVoltageV /= n;
  m.loadVoltageV /= n;
  m.currentMa /= n;
  m.powerMw /= n;
  return m;
}

void nextionEnd() {
  nextion.write(0xFF); nextion.write(0xFF); nextion.write(0xFF);
}

void nextionSetText(const char* component, const String& value) {
  nextion.print(component);
  nextion.print(".txt=\"");
  nextion.print(value);
  nextion.print("\"");
  nextionEnd();
}

void updateDisplay(float wavelengthNm, const Measurement &m) {
  nextionSetText("tWave", String(wavelengthNm, 0) + " nm");
  nextionSetText("tVolt", String(m.loadVoltageV, 3) + " V");
  nextionSetText("tCurr", String(m.currentMa / 1000.0f, 3) + " A");
  nextionSetText("tPower", String(m.powerMw / 1000.0f, 3) + " W");
}

void printCsvHeader() {
  Serial.println("wavelength_nm,servo_deg,load_voltage_V,current_A,power_W,shunt_mV");
}

void printCsvRow(float wavelengthNm, const Measurement &m) {
  float servoDeg = interpolateServoAngle(wavelengthNm);
  Serial.print(wavelengthNm, 0); Serial.print(',');
  Serial.print(servoDeg, 2); Serial.print(',');
  Serial.print(m.loadVoltageV, 5); Serial.print(',');
  Serial.print(m.currentMa / 1000.0f, 6); Serial.print(',');
  Serial.print(m.powerMw / 1000.0f, 6); Serial.print(',');
  Serial.println(m.shuntVoltageMv, 4);
}

void scanSpectrum() {
  stopRequested = false;
  Serial.println("# SCAN_START");
  printCsvHeader();

  for (uint16_t wl = LAMBDA_MIN_NM; wl <= LAMBDA_MAX_NM; wl += LAMBDA_STEP_NM) {
    if (stopRequested) break;
    setWavelength((float)wl);
    Measurement m = readAveragedMeasurement();
    printCsvRow((float)wl, m);
    updateDisplay((float)wl, m);
  }

  Serial.println(stopRequested ? "# SCAN_ABORTED" : "# SCAN_COMPLETE");
}

void measureAt(float wavelengthNm) {
  wavelengthNm = constrain(wavelengthNm, (float)LAMBDA_MIN_NM, (float)LAMBDA_MAX_NM);
  setWavelength(wavelengthNm);
  Measurement m = readAveragedMeasurement();
  printCsvHeader();
  printCsvRow(wavelengthNm, m);
  updateDisplay(wavelengthNm, m);
}

void processCommand(String cmd) {
  cmd.trim();
  cmd.toUpperCase();

  if (cmd == "START") {
    scanSpectrum();
  } else if (cmd == "STOP") {
    stopRequested = true;
    Serial.println("# STOP_REQUESTED");
  } else if (cmd == "MEASURE") {
    measureAt(550.0f);
  } else if (cmd.startsWith("GOTO:")) {
    float wl = cmd.substring(5).toFloat();
    measureAt(wl);
  } else if (cmd == "STATUS") {
    Serial.print("# INA219="); Serial.println(inaOk ? "OK" : "ERROR");
    Serial.println("# Commands: START | STOP | MEASURE | GOTO:550 | STATUS");
  } else if (cmd.length() > 0) {
    Serial.println("# Unknown command");
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("# SpeScan-PV firmware starting");

  Wire.setPins(I2C_SDA, I2C_SCL);
  Wire.begin();

  inaOk = ina219.begin(&Wire);
  if (inaOk) {
    // Default library initialization uses 32 V / 2 A calibration.
    ina219.setCalibration_32V_2A();
    Serial.println("# INA219=OK");
  } else {
    Serial.println("# INA219=NOT_FOUND");
  }

  gratingServo.setPeriodHertz(50);
  if (!gratingServo.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US)) {
    Serial.println("# SERVO_ATTACH=ERROR");
  } else {
    Serial.println("# SERVO_ATTACH=OK");
    setWavelength(550.0f);
  }

  // Explicit pins are used to avoid board-specific UART defaults.
  nextion.begin(9600, SERIAL_8N1, NEXTION_RX, NEXTION_TX);
  nextionSetText("tStatus", "SpeScan-PV ready");

  Serial.println("# Commands: START | STOP | MEASURE | GOTO:550 | STATUS");
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    processCommand(cmd);
  }
  delay(10);
}
