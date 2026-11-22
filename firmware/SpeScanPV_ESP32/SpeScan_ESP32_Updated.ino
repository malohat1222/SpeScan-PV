/*
  SpeScan-PV | ESP32 non-blocking spectral-scanner firmware | research prototype

  Hardware represented by THIS code:
    ESP32 + INA219 I2C sensor + ONE DS3218 servo rotating a reflective grating.
    Optional Nextion display on Serial2.

  NOT implemented by this hardware/firmware:
    * two independently adjustable spectral-band slit edges
    * sample temperature
    * full I-V sweep / maximum power point (requires controlled load)
    * absolute spectral responsivity (requires calibrated incident optical power)
    * Si / Ge / GaAs classification or automatic filter design

  IMPORTANT SCIENTIFIC SAFEGUARD:
    OPTICAL_CALIBRATION_VALID is false until you measure and replace ALL five
    placeholder angle-to-wavelength pairs below.  Wavelength commands/scan
    remain disabled while it is false. This prevents uncalibrated data being
    presented as a measured physical spectrum.

  Electrical safety:
    - Use a separate servo power supply rated for its peak current, NOT ESP32 3V3.
    - Common ground between ESP32 and the servo controller power supply.
    - First test with grating disconnected from servo; verify direction/limits.
    - Never expose INA219 beyond your breakout's bus/shunt ratings.
    - Use suitable lamp enclosure, heatsinking and eye protection.

  Libraries: Adafruit INA219, ESP32Servo, ESP32 Arduino core.
*/

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <ESP32Servo.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// -------- Hardware configuration (edit only after checking wiring) -------
static constexpr uint8_t PIN_SDA = 21;
static constexpr uint8_t PIN_SCL = 22;
static constexpr uint8_t PIN_GRATING_SERVO = 25;
static constexpr uint8_t PIN_NEXTION_RX = 32;  // ESP32 RX <- display TX
static constexpr uint8_t PIN_NEXTION_TX = 33;  // ESP32 TX -> display RX
static constexpr uint8_t INA219_ADDRESS = 0x40;

// Set true ONLY if a real Nextion is connected and the named fields exist.
static constexpr bool USE_NEXTION = false;

// -------- Optical configuration (TARGET range, not validated coverage) ----
static constexpr uint16_t MIN_WAVELENGTH_NM = 400;
static constexpr uint16_t MAX_WAVELENGTH_NM = 1100;
static constexpr uint16_t SCAN_STEP_NM = 10;
static constexpr uint32_t SERVO_SETTLE_MS = 500;
static constexpr uint32_t SAMPLE_INTERVAL_MS = 20;
static constexpr uint8_t SAMPLES_PER_POINT = 10;

// Conservative signal width for first motion tests: adapt to YOUR servo.
// Mechanical travel may vary even among servos sold as "270 degree".
static constexpr int SERVO_MIN_PULSE_US = 1000;
static constexpr int SERVO_MAX_PULSE_US = 2000;
static constexpr float SERVO_NOMINAL_TRAVEL_DEG = 270.0f;

// For cautious motion testing ONLY (no grating attached): SERVO:90
static constexpr float SERVO_TEST_MIN_DEG = 80.0f;
static constexpr float SERVO_TEST_MAX_DEG = 100.0f;

// KEEP FALSE until the reference spectral calibration is complete!
static constexpr bool OPTICAL_CALIBRATION_VALID = false;

struct CalibrationPoint {
  float wavelengthNm;
  float servoDeg;
};

// EXAMPLE VALUES ONLY. NOT experimentally measured.
static const CalibrationPoint calibration[] = {
  { 400.0f,  55.0f },
  { 550.0f,  80.0f },
  { 700.0f, 105.0f },
  { 850.0f, 130.0f },
  {1100.0f, 170.0f }
};
static constexpr size_t CAL_COUNT = sizeof(calibration) / sizeof(calibration[0]);

Adafruit_INA219 sensor(INA219_ADDRESS);
Servo gratingServo;
HardwareSerial nextion(2);
bool inaPresent = false;
bool servoPresent = false;

struct Reading {
  float busV;
  float shuntMv;
  float loadV;
  float currentA;
  float electricPowerW;
};

struct Accumulator {
  double busV = 0;
  double shuntMv = 0;
  double loadV = 0;
  double currentA = 0;
  double powerW = 0;
  uint8_t count = 0;
};

// Keep one consistent state machine so serial STOP remains responsive.
enum class ScanState : uint8_t { IDLE, WAIT_SERVO, SAMPLING };
ScanState state = ScanState::IDLE;
bool repeatingScan = false;
uint16_t activeWavelengthNm = 550;
float activeServoAngle = 0.0f;
uint32_t stateStartedMs = 0;
uint32_t lastSampleMs = 0;
Accumulator sums;

static char serialBuffer[64] = {0};
static size_t serialBufferLength = 0;

bool isSensorPresent() {
  Wire.beginTransmission(INA219_ADDRESS);
  return Wire.endTransmission() == 0;
}

bool calibrationTableIsSane() {
  if (CAL_COUNT < 2) return false;
  if (calibration[0].wavelengthNm > MIN_WAVELENGTH_NM ||
      calibration[CAL_COUNT - 1].wavelengthNm < MAX_WAVELENGTH_NM) return false;
  const bool ascendingAngles = calibration[1].servoDeg > calibration[0].servoDeg;
  if (calibration[1].servoDeg == calibration[0].servoDeg) return false;
  for (size_t i = 0; i < CAL_COUNT; ++i) {
    if (!isfinite(calibration[i].wavelengthNm) ||
        !isfinite(calibration[i].servoDeg) ||
        calibration[i].servoDeg < 0.0f ||
        calibration[i].servoDeg > SERVO_NOMINAL_TRAVEL_DEG) return false;
    if (i > 0) {
      if (calibration[i].wavelengthNm <= calibration[i-1].wavelengthNm) return false;
      const float da = calibration[i].servoDeg - calibration[i-1].servoDeg;
      if (ascendingAngles ? (da <= 0.0f) : (da >= 0.0f)) return false;
    }
  }
  return true;
}

bool interpolateAngle(float wavelengthNm, float &outDeg) {
  if (!isfinite(wavelengthNm) ||
      wavelengthNm < MIN_WAVELENGTH_NM ||
      wavelengthNm > MAX_WAVELENGTH_NM ||
      !calibrationTableIsSane()) return false;
  for (size_t i = 0; i + 1 < CAL_COUNT; ++i) {
    const CalibrationPoint &a = calibration[i];
    const CalibrationPoint &b = calibration[i + 1];
    if (wavelengthNm >= a.wavelengthNm && wavelengthNm <= b.wavelengthNm) {
      float t = (wavelengthNm - a.wavelengthNm) /
                (b.wavelengthNm - a.wavelengthNm);
      outDeg = a.servoDeg + t * (b.servoDeg - a.servoDeg);
      return true;
    }
  }
  return false;
}

int servoAngleToPulse(float degree) {
  degree = constrain(degree, 0.0f, SERVO_NOMINAL_TRAVEL_DEG);
  return (int)roundf(SERVO_MIN_PULSE_US +
             (degree / SERVO_NOMINAL_TRAVEL_DEG) *
             (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US));
}

void nextionSendEnd() {
  for (uint8_t i = 0; i < 3; ++i) nextion.write((uint8_t)0xFF);
}

void nextionText(const char *component, const String &value) {
  if (!USE_NEXTION) return;
  nextion.print(component);
  nextion.print(".txt=\"");
  nextion.print(value);
  nextion.print("\"");
  nextionSendEnd();
}

void showReading(uint16_t wavelengthNm, const Reading &m) {
  if (!USE_NEXTION) return;
  nextionText("tWave", String(wavelengthNm) + " nm");
  nextionText("tVolt", String(m.loadV, 3) + " V");
  nextionText("tCurr", String(m.currentA, 4) + " A");
  nextionText("tPower", String(m.electricPowerW, 4) + " W");
}

bool readOneSample(Reading &r) {
  // INA219 reads are meaningful only when the device is responding.
  if (!inaPresent || !isSensorPresent()) return false;
  r.shuntMv = sensor.getShuntVoltage_mV();
  r.busV = sensor.getBusVoltage_V();
  r.currentA = sensor.getCurrent_mA() / 1000.0f;
  r.loadV = r.busV + r.shuntMv / 1000.0f;
  r.electricPowerW = r.loadV * r.currentA; // Derived electrical power at sensed point
  return isfinite(r.shuntMv) && isfinite(r.busV) &&
         isfinite(r.currentA) && isfinite(r.loadV) &&
         isfinite(r.electricPowerW);
}

void printCsvHeader() {
  Serial.println("wavelength_nm,servo_angle_deg,load_voltage_V,current_A,electric_power_W,shunt_mV,measurement_status");
}

void printCsvRow(uint16_t wavelengthNm, float servoDeg, const Reading &r) {
  Serial.print(wavelengthNm); Serial.print(',');
  Serial.print(servoDeg, 2); Serial.print(',');
  Serial.print(r.loadV, 6); Serial.print(',');
  Serial.print(r.currentA, 7); Serial.print(',');
  Serial.print(r.electricPowerW, 7); Serial.print(',');
  Serial.print(r.shuntMv, 4);
  Serial.println(",UNVALIDATED_OPTICAL_SETUP");
}

void cancelMeasurement(const char *reason) {
  state = ScanState::IDLE;
  repeatingScan = false;
  sums = Accumulator{};
  Serial.print("# "); Serial.println(reason);
  nextionText("tStatus", reason);
}

bool beginPoint(uint16_t wl) {
  if (!servoPresent) {
    Serial.println("# ERROR: servo not attached");
    return false;
  }
  if (!inaPresent || !isSensorPresent()) {
    Serial.println("# ERROR: INA219 not found; no measurement recorded");
    return false;
  }
  if (!OPTICAL_CALIBRATION_VALID) {
    Serial.println("# ERROR: optical calibration NOT VALIDATED; no wavelength positioning");
    return false;
  }
  float deg = 0;
  if (!interpolateAngle((float)wl, deg)) {
    Serial.println("# ERROR: wavelength outside measured calibration");
    return false;
  }
  activeWavelengthNm = wl;
  activeServoAngle = deg;
  gratingServo.writeMicroseconds(servoAngleToPulse(deg));
  stateStartedMs = millis();
  state = ScanState::WAIT_SERVO;
  sums = Accumulator{};
  return true;
}

void startSingle(uint16_t wl) {
  if (state != ScanState::IDLE) {
    Serial.println("# BUSY: issue STOP before next command");
    return;
  }
  repeatingScan = false;
  if (beginPoint(wl)) {
    Serial.println("# SINGLE_MEASUREMENT_START");
    printCsvHeader();
  }
}

void startScan() {
  if (state != ScanState::IDLE) {
    Serial.println("# BUSY: issue STOP before next command");
    return;
  }
  repeatingScan = true;
  if (beginPoint(MIN_WAVELENGTH_NM)) {
    Serial.println("# SCAN_START");
    printCsvHeader();
  } else {
    repeatingScan = false;
  }
}

void serviceScan() {
  if (state == ScanState::IDLE) return;
  const uint32_t now = millis();
  if (state == ScanState::WAIT_SERVO) {
    if (now - stateStartedMs >= SERVO_SETTLE_MS) {
      lastSampleMs = now - SAMPLE_INTERVAL_MS;
      state = ScanState::SAMPLING;
    }
    return;
  }
  if (state == ScanState::SAMPLING && now - lastSampleMs >= SAMPLE_INTERVAL_MS) {
    lastSampleMs = now;
    Reading r;
    if (!readOneSample(r)) {
      inaPresent = false;
      cancelMeasurement("ERROR: INA219 disconnected or invalid reading");
      return;
    }
    sums.busV += r.busV;
    sums.shuntMv += r.shuntMv;
    sums.loadV += r.loadV;
    sums.currentA += r.currentA;
    sums.powerW += r.electricPowerW;
    sums.count++;

    if (sums.count < SAMPLES_PER_POINT) return;
    const float n = (float)sums.count;
    Reading avg = {
      (float)(sums.busV / n),
      (float)(sums.shuntMv / n),
      (float)(sums.loadV / n),
      (float)(sums.currentA / n),
      (float)(sums.powerW / n)
    };
    printCsvRow(activeWavelengthNm, activeServoAngle, avg);
    showReading(activeWavelengthNm, avg);

    if (repeatingScan &&
        (uint32_t)activeWavelengthNm + SCAN_STEP_NM <= MAX_WAVELENGTH_NM) {
      if (!beginPoint(activeWavelengthNm + SCAN_STEP_NM)) {
        cancelMeasurement("ERROR: next optical point failed");
      }
    } else {
      cancelMeasurement(repeatingScan ? "SCAN_COMPLETE" : "MEASURE_COMPLETE");
    }
  }
}

void status() {
  Serial.print("# MODE=");
  Serial.println(state == ScanState::IDLE ? "IDLE" : "ACTIVE");
  Serial.print("# INA219=");
  Serial.println(isSensorPresent() ? "RESPONDING" : "MISSING");
  Serial.print("# SERVO=");
  Serial.println(gratingServo.attached() ? "ATTACHED" : "NOT_ATTACHED");
  Serial.print("# OPTICAL_CALIBRATION=");
  Serial.println(OPTICAL_CALIBRATION_VALID ? "USER_MARKED_VALID" : "NOT_VALIDATED");
  Serial.println("# TEMP_SENSOR=NOT_INSTALLED");
  Serial.println("# IV_SWEEP=NOT_IMPLEMENTED");
  Serial.println("# DUAL_EDGE_SLIT=NOT_IMPLEMENTED");
  Serial.println("# POWER=U_load * I (derived electrical watts)");
}

void help() {
  Serial.println("# COMMANDS: HELP | STATUS | START | STOP | MEASURE | GOTO:550 | SENSOR | SERVO:90");
  Serial.println("# START/GOTO/MEASURE are BLOCKED until optical calibration is verified.");
  Serial.println("# SERVO:80..100 is for cautious detached-hardware testing only.");
}

void processCommand(char *cmd) {
  // ASCII command normalization, no line-read blocking.
  while (*cmd == ' ' || *cmd == '\t') cmd++;
  size_t n = strlen(cmd);
  while (n && (cmd[n-1] == ' ' || cmd[n-1] == '\t' || cmd[n-1] == '\r')) cmd[--n] = '\0';
  for (size_t i = 0; i < n; ++i) if (cmd[i] >= 'a' && cmd[i] <= 'z') cmd[i] -= ('a' - 'A');

  if (strcmp(cmd, "HELP") == 0) help();
  else if (strcmp(cmd, "STATUS") == 0) status();
  else if (strcmp(cmd, "STOP") == 0) cancelMeasurement("STOP_REQUESTED");
  else if (strcmp(cmd, "START") == 0) startScan();
  else if (strcmp(cmd, "MEASURE") == 0) startSingle(550);
  else if (strcmp(cmd, "SENSOR") == 0) {
    Reading r;
    if (readOneSample(r)) {
      Serial.print("# U_load_V="); Serial.println(r.loadV, 6);
      Serial.print("# I_A="); Serial.println(r.currentA, 7);
      Serial.print("# P_el_W="); Serial.println(r.electricPowerW, 7);
      Serial.println("# WARNING: electrical-only read, not spectral responsivity");
    } else {
      Serial.println("# ERROR: sensor not responding");
    }
  }
  else if (strncmp(cmd, "GOTO:", 5) == 0) {
    char *end = nullptr;
    float val = strtof(cmd + 5, &end);
    if (end == cmd + 5 || *end != '\0' || !isfinite(val) ||
        val < MIN_WAVELENGTH_NM || val > MAX_WAVELENGTH_NM) {
      Serial.println("# ERROR: GOTO needs valid wavelength in configured range");
    } else {
      startSingle((uint16_t)lroundf(val));
    }
  }
  else if (strncmp(cmd, "SERVO:", 6) == 0) {
    if (state != ScanState::IDLE) {
      Serial.println("# BUSY: STOP before manual servo test"); return;
    }
    char *end = nullptr;
    float val = strtof(cmd + 6, &end);
    if (end == cmd + 6 || *end != '\0' || !isfinite(val) ||
        val < SERVO_TEST_MIN_DEG || val > SERVO_TEST_MAX_DEG) {
      Serial.println("# ERROR: servo bench-test range is 80..100 degrees");
    } else if (!servoPresent) {
      Serial.println("# ERROR: servo not attached");
    } else {
      gratingServo.writeMicroseconds(servoAngleToPulse(val));
      Serial.println("# SERVO_BENCH_TEST_MOVED; wavelength is NOT calibrated");
    }
  }
  else if (cmd[0] != '\0') Serial.println("# ERROR: unknown command; type HELP");
}

void serviceSerial() {
  while (Serial.available()) {
    const char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialBufferLength > 0) {
        serialBuffer[serialBufferLength] = '\0';
        processCommand(serialBuffer);
        serialBufferLength = 0;
      }
    } else if (serialBufferLength + 1 < sizeof(serialBuffer)) {
      serialBuffer[serialBufferLength++] = c;
    } else {
      serialBufferLength = 0;
      Serial.println("# ERROR: command too long");
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println("# SpeScan-PV prototype firmware v0.2");
  Serial.println("# DO NOT present example servo map as physical spectral calibration");

  if (!Wire.begin(PIN_SDA, PIN_SCL, 100000)) {
    Serial.println("# ERROR: I2C initialization failed");
  }
  inaPresent = sensor.begin(&Wire);
  if (inaPresent) {
    sensor.setCalibration_32V_2A();
    Serial.println("# INA219 initialized with 32V/2A calibration");
  } else {
    Serial.println("# INA219 missing: measurement commands disabled");
  }

  gratingServo.setPeriodHertz(50);
  gratingServo.attach(PIN_GRATING_SERVO, SERVO_MIN_PULSE_US, SERVO_MAX_PULSE_US);
  // attach() may return channel index 0 on success; check attached() instead.
  servoPresent = gratingServo.attached();
  Serial.println(servoPresent ? "# SERVO_ATTACHED" : "# SERVO_ATTACH_FAILED");
  // Intentionally no automatic grating motion at startup.

  if (USE_NEXTION) {
    nextion.begin(9600, SERIAL_8N1, PIN_NEXTION_RX, PIN_NEXTION_TX);
    nextionText("tStatus", "SpeScan-PV prototype ready");
  }
  status();
  help();
}

void loop() {
  serviceSerial();  // called repeatedly, including while scanning
  serviceScan();    // no blocking 500ms delay
  delay(1);         // cooperative yield for ESP32 background services
}
