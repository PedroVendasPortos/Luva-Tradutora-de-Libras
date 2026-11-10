#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <math.h>
#include "SensorWindow.h"
#include "ModelRunner.h"

// ============================================================
// HARDWARE CONFIGURATION
// ============================================================

constexpr uint8_t FLEX_COUNT = 5;

const uint8_t FLEX_PINS[FLEX_COUNT] = {
  34,  // Thumb
  35,  // Index
  32,  // Middle
  33,  // Ring
  25   // Pinky
};

const char* FLEX_NAMES[FLEX_COUNT] = {
  "polegar",
  "indicador",
  "medio",
  "anelar",
  "minimo"
};

constexpr uint8_t MPU_SDA = 21;
constexpr uint8_t MPU_SCL = 22;

// O MPU-6050 responde em 0x68 (AD0 em GND) ou 0x69 (AD0 em 3V3).
// O firmware testa os dois automaticamente.
constexpr uint8_t MPU_ADDRESS_LOW = 0x68;
constexpr uint8_t MPU_ADDRESS_HIGH = 0x69;

constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t DEFAULT_SAMPLE_MS = 50; // 20 Hz
constexpr uint16_t ADC_MAX_VALUE = 4095;
constexpr uint16_t CALIBRATION_SAMPLES = 32;
constexpr uint16_t STUCK_SAMPLE_LIMIT = 100; // 5 seconds at 20 Hz

// Quantas leituras seguidas do MPU podem falhar antes de
// declarar o sensor indisponivel e tentar reinicializar.
constexpr uint8_t MPU_FAIL_LIMIT = 3;
constexpr uint32_t MPU_RETRY_INTERVAL_MS = 1000;

// ============================================================
// STATE
// ============================================================

struct FlexCalibration {
  int32_t openValue;
  int32_t closedValue;
  bool valid;
};

struct FlexReading {
  uint16_t raw;
  bool valid;
  bool stuck;
  uint16_t unchangedSamples;
  uint16_t previousRaw;
};

FlexCalibration calibration[FLEX_COUNT];
FlexReading flexReading[FLEX_COUNT];

Preferences preferences;
SensorWindow sensorWindow;

bool collecting = false;
bool mpuAvailable = false;
bool diagnosticMode = false;

uint8_t mpuAddress = MPU_ADDRESS_LOW;
uint8_t mpuWhoAmI = 0;
uint8_t mpuConsecutiveFailures = 0;
uint32_t lastMpuRetry = 0;

uint32_t sampleIntervalMs = DEFAULT_SAMPLE_MS;
uint32_t nextSampleMs = 0;
uint32_t sampleId = 0;
uint32_t sessionId = 0;

uint32_t totalSamples = 0;
uint32_t lateSamples = 0;
uint32_t invalidFlexReads = 0;
uint32_t mpuFailures = 0;

uint32_t measurementWindowStart = 0;
uint32_t measurementWindowSamples = 0;
float measuredHz = 0.0f;

String commandBuffer;

// Preserve the latest completed window for diagnostics after the live buffer resets.
float lastCompletedWindow[MODEL_WINDOW_SAMPLES][MODEL_FEATURE_COUNT];
bool lastWindowAvailable = false;
bool lastWindowValid = false;

// ============================================================
// MPU-6050 LOW-LEVEL I2C
// ============================================================

bool mpuWriteRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(mpuAddress);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool mpuReadRegisters(uint8_t reg, uint8_t* buffer,
                      size_t length) {
  Wire.beginTransmission(mpuAddress);
  Wire.write(reg);

  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  uint8_t received =
    Wire.requestFrom(
        (uint8_t)mpuAddress,
        (uint8_t)length,
        (uint8_t)true
    );

  if (received != length) {
    while (Wire.available()) {
      Wire.read();
    }
    return false;
  }

  for (size_t i = 0; i < length; ++i) {
    buffer[i] = Wire.read();
  }

  return true;
}

// Tenta inicializar o MPU em um endereco especifico.
// Retorna true se o dispositivo respondeu e foi configurado.
bool initializeMPUAt(uint8_t address, bool verbose) {
  mpuAddress = address;

  uint8_t identity = 0;

  if (!mpuReadRegisters(0x75, &identity, 1)) {
    if (verbose) {
      Serial.printf("# MPU 0x%02X: sem resposta I2C\n", address);
    }
    return false;
  }

  mpuWhoAmI = identity;

  if (verbose) {
    Serial.printf("# MPU 0x%02X: WHO_AM_I=0x%02X\n",
                  address, identity);
  }

  // O MPU-6050 original retorna 0x68, mas muitos modulos
  // compativeis/clones retornam outros valores (0x70, 0x72,
  // 0x98, ...) e funcionam normalmente. Rejeitamos apenas
  // 0x00 e 0xFF, que indicam barramento flutuando/sem chip.
  if (identity == 0x00 || identity == 0xFF) {
    if (verbose) {
      Serial.println("# MPU: WHO_AM_I invalido (barramento sem chip?)");
    }
    return false;
  }

  if (identity != 0x68 && verbose) {
    Serial.println("# MPU: WHO_AM_I diferente de 0x68; "
                   "assumindo modulo compativel");
  }

  // Wake the device (sai do modo sleep).
  if (!mpuWriteRegister(0x6B, 0x00)) {
    if (verbose) Serial.println("# MPU: falha ao acordar (0x6B)");
    return false;
  }

  delay(10);

  // Accelerometer range: +/- 2 g.
  if (!mpuWriteRegister(0x1C, 0x00)) {
    if (verbose) Serial.println("# MPU: falha ao configurar acelerometro");
    return false;
  }

  // Gyroscope range: +/- 250 degrees/second.
  if (!mpuWriteRegister(0x1B, 0x00)) {
    if (verbose) Serial.println("# MPU: falha ao configurar giroscopio");
    return false;
  }

  // Digital low-pass filter configuration.
  if (!mpuWriteRegister(0x1A, 0x03)) {
    if (verbose) Serial.println("# MPU: falha ao configurar filtro");
    return false;
  }

  if (verbose) {
    Serial.printf("# MPU: inicializado em 0x%02X\n", address);
  }

  return true;
}

// Testa 0x68 e depois 0x69.
bool initializeMPU(bool verbose = false) {
  if (initializeMPUAt(MPU_ADDRESS_LOW, verbose)) {
    return true;
  }

  if (initializeMPUAt(MPU_ADDRESS_HIGH, verbose)) {
    return true;
  }

  // Volta ao endereco padrao para a proxima tentativa.
  mpuAddress = MPU_ADDRESS_LOW;
  return false;
}

// Varre o barramento I2C e lista os dispositivos encontrados.
void scanI2C() {
  Serial.println("# I2C scan:");
  uint8_t found = 0;

  for (uint8_t addr = 1; addr < 127; ++addr) {
    Wire.beginTransmission(addr);

    if (Wire.endTransmission() == 0) {
      Serial.printf("# dispositivo em 0x%02X\n", addr);
      ++found;
    }
  }

  if (found == 0) {
    Serial.println("# nenhum dispositivo encontrado "
                   "(verifique SDA/SCL, VCC, GND e solda)");
  }
}

bool readMotion(float& ax, float& ay, float& az,
                float& gx, float& gy, float& gz) {
  // Read registers 0x3B through 0x48: 14 bytes total.
  uint8_t data[14];

  if (!mpuReadRegisters(0x3B, data, sizeof(data))) {
    return false;
  }

  int16_t rawAx = (int16_t)((data[0] << 8) | data[1]);
  int16_t rawAy = (int16_t)((data[2] << 8) | data[3]);
  int16_t rawAz = (int16_t)((data[4] << 8) | data[5]);

  // Bytes 6 and 7 are temperature; not used in this project.

  int16_t rawGx = (int16_t)((data[8] << 8) | data[9]);
  int16_t rawGy = (int16_t)((data[10] << 8) | data[11]);
  int16_t rawGz = (int16_t)((data[12] << 8) | data[13]);

  // Accelerometer: +/- 2 g = 16384 LSB/g.
  ax = rawAx / 16384.0f;
  ay = rawAy / 16384.0f;
  az = rawAz / 16384.0f;

  // Gyroscope: +/- 250 degrees/second = 131 LSB/(degree/second).
  gx = rawGx / 131.0f;
  gy = rawGy / 131.0f;
  gz = rawGz / 131.0f;

  return std::isfinite(ax) &&
         std::isfinite(ay) &&
         std::isfinite(az) &&
         std::isfinite(gx) &&
         std::isfinite(gy) &&
         std::isfinite(gz);
}

// Reinicializa o MPU se ele estiver indisponivel, no maximo
// uma tentativa por MPU_RETRY_INTERVAL_MS. Funciona tambem
// fora da coleta.
void retryMpuIfNeeded(uint32_t now) {
  if (mpuAvailable) {
    return;
  }

  if (now - lastMpuRetry < MPU_RETRY_INTERVAL_MS) {
    return;
  }

  lastMpuRetry = now;

  // Silencioso para nao poluir o stream CSV durante a coleta.
  mpuAvailable = initializeMPU(false);

  if (mpuAvailable) {
    mpuConsecutiveFailures = 0;
  }
}

// ============================================================
// CALIBRATION STORAGE
// ============================================================

void loadCalibration() {
  preferences.begin("libras", false);

  for (uint8_t i = 0; i < FLEX_COUNT; ++i) {
    char key[20];

    snprintf(key, sizeof(key), "open%u", i);
    calibration[i].openValue =
        preferences.getInt(key, -1);

    snprintf(key, sizeof(key), "closed%u", i);
    calibration[i].closedValue =
        preferences.getInt(key, -1);

    int32_t openValue = calibration[i].openValue;
    int32_t closedValue = calibration[i].closedValue;

    calibration[i].valid =
        openValue >= 0 &&
        openValue <= ADC_MAX_VALUE &&
        closedValue >= 0 &&
        closedValue <= ADC_MAX_VALUE &&
        abs(closedValue - openValue) >= 20;
  }
}

void saveCalibration(uint8_t index) {
  char key[20];

  snprintf(key, sizeof(key), "open%u", index);
  preferences.putInt(key, calibration[index].openValue);

  snprintf(key, sizeof(key), "closed%u", index);
  preferences.putInt(key, calibration[index].closedValue);
}

bool calibratedValue(uint8_t index, int32_t& result) {
  if (!calibration[index].valid ||
      !flexReading[index].valid) {
    return false;
  }

  int32_t openValue = calibration[index].openValue;
  int32_t closedValue = calibration[index].closedValue;
  int32_t span = closedValue - openValue;

  if (span == 0) {
    return false;
  }

  // Normalize the open reference to 0 and the closed
  // reference to 1000. Clamp readings to that range.
  float value =
      1000.0f * (flexReading[index].raw - openValue) / span;

  value = constrain(value, 0.0f, 1000.0f);
  result = (int32_t)lroundf(value);

  return true;
}

// ============================================================
// FLEX SENSOR ACQUISITION
// ============================================================

void readFlexSensors() {
  for (uint8_t i = 0; i < FLEX_COUNT; ++i) {
    uint16_t raw = analogRead(FLEX_PINS[i]);

    flexReading[i].raw = raw;
    flexReading[i].valid = raw > 0 && raw < ADC_MAX_VALUE;

    if (raw == flexReading[i].previousRaw) {
      if (flexReading[i].unchangedSamples < UINT16_MAX) {
        ++flexReading[i].unchangedSamples;
      }
    } else {
      flexReading[i].unchangedSamples = 0;
    }

    flexReading[i].previousRaw = raw;

    flexReading[i].stuck =
        flexReading[i].unchangedSamples >= STUCK_SAMPLE_LIMIT;

    if (!flexReading[i].valid) {
      ++invalidFlexReads;
    }
  }
}

// Capture an average reference for one finger.
bool captureCalibration(uint8_t index, bool isOpen) {
  if (index >= FLEX_COUNT) {
    return false;
  }

  uint32_t sum = 0;

  for (uint16_t n = 0; n < CALIBRATION_SAMPLES; ++n) {
    uint16_t value = analogRead(FLEX_PINS[index]);

    if (value == 0 || value >= ADC_MAX_VALUE) {
      return false;
    }

    sum += value;
    yield();
  }

  int32_t average = sum / CALIBRATION_SAMPLES;

  if (isOpen) {
    calibration[index].openValue = average;
  } else {
    calibration[index].closedValue = average;
  }

  calibration[index].valid =
      calibration[index].openValue >= 0 &&
      calibration[index].closedValue >= 0 &&
      abs(calibration[index].closedValue -
          calibration[index].openValue) >= 20;

  saveCalibration(index);

  return true;
}
// ============================================================
// FINGER GAP SENSORS (4x analog Hall-effect / magnetic)
// ============================================================
//
// One sensor per space between adjacent fingers. A small magnet
// on one finger moves relative to the sensor on the neighbor, so
// the analog output changes with the distance between them.
//
// Calibrated scale: 0    = fingers TOGETHER (touching)
//                   1000 = fingers APART (maximum spread)
// Note: Hall response is not linear with distance.

constexpr uint8_t GAP_COUNT = 4;
constexpr uint8_t GAP_OVERSAMPLE = 4;

const uint8_t GAP_PINS[GAP_COUNT] = {
  36,  // gap between thumb and index
  39,  // gap between index and middle
  27,  // gap between middle and ring
  26   // gap between ring and pinky
};

const char* GAP_NAMES[GAP_COUNT] = {
  "polegar_indicador",
  "indicador_medio",
  "medio_anelar",
  "anelar_minimo"
};

struct GapCalibration {
  int32_t togetherValue;
  int32_t apartValue;
  bool valid;
};

struct GapReading {
  uint16_t raw;
  bool valid;
};

GapCalibration gapCalibration[GAP_COUNT];
GapReading gapReading[GAP_COUNT];

void setupGapSensors() {
  for (uint8_t i = 0; i < GAP_COUNT; ++i) {
    pinMode(GAP_PINS[i], INPUT);
    analogSetPinAttenuation(GAP_PINS[i], ADC_11db);

    gapReading[i].raw = 0;
    gapReading[i].valid = false;
    gapCalibration[i] = {-1, -1, false};
  }
}

void loadGapCalibration() {
  for (uint8_t i = 0; i < GAP_COUNT; ++i) {
    char key[20];

    snprintf(key, sizeof(key), "gtog%u", i);
    gapCalibration[i].togetherValue = preferences.getInt(key, -1);

    snprintf(key, sizeof(key), "gapart%u", i);
    gapCalibration[i].apartValue = preferences.getInt(key, -1);

    int32_t t = gapCalibration[i].togetherValue;
    int32_t a = gapCalibration[i].apartValue;

    gapCalibration[i].valid =
        t >= 0 && t <= ADC_MAX_VALUE &&
        a >= 0 && a <= ADC_MAX_VALUE &&
        abs(a - t) >= 20;
  }
}

void saveGapCalibration(uint8_t index) {
  char key[20];

  snprintf(key, sizeof(key), "gtog%u", index);
  preferences.putInt(key, gapCalibration[index].togetherValue);

  snprintf(key, sizeof(key), "gapart%u", index);
  preferences.putInt(key, gapCalibration[index].apartValue);
}

void readGapSensors() {
  for (uint8_t i = 0; i < GAP_COUNT; ++i) {
    uint32_t sum = 0;

    for (uint8_t n = 0; n < GAP_OVERSAMPLE; ++n) {
      sum += analogRead(GAP_PINS[i]);
    }

    uint16_t raw = sum / GAP_OVERSAMPLE;

    gapReading[i].raw = raw;
    gapReading[i].valid = raw > 0 && raw < ADC_MAX_VALUE;
  }
}

// 0 = together, 1000 = apart.
bool gapCalibratedValue(uint8_t index, int32_t& result) {
  if (!gapCalibration[index].valid || !gapReading[index].valid) {
    return false;
  }

  int32_t t = gapCalibration[index].togetherValue;
  int32_t a = gapCalibration[index].apartValue;
  int32_t span = a - t;

  if (span == 0) {
    return false;
  }

  float value = 1000.0f * (gapReading[index].raw - t) / span;
  value = constrain(value, 0.0f, 1000.0f);
  result = (int32_t)lroundf(value);

  return true;
}

bool captureGapCalibration(uint8_t index, bool isTogether) {
  if (index >= GAP_COUNT) {
    return false;
  }

  uint32_t sum = 0;

  for (uint16_t n = 0; n < CALIBRATION_SAMPLES; ++n) {
    uint16_t value = analogRead(GAP_PINS[index]);

    if (value == 0 || value >= ADC_MAX_VALUE) {
      return false;
    }

    sum += value;
    yield();
  }

  int32_t average = sum / CALIBRATION_SAMPLES;

  if (isTogether) {
    gapCalibration[index].togetherValue = average;
  } else {
    gapCalibration[index].apartValue = average;
  }

  gapCalibration[index].valid =
      gapCalibration[index].togetherValue >= 0 &&
      gapCalibration[index].apartValue >= 0 &&
      abs(gapCalibration[index].apartValue -
          gapCalibration[index].togetherValue) >= 20;

  saveGapCalibration(index);

  return true;
}

int gapIndexFromName(String name) {
  name.toLowerCase();

  for (uint8_t i = 0; i < GAP_COUNT; ++i) {
    if (name == GAP_NAMES[i]) {
      return i;
    }
  }

  return -1;
}

// Appends the gap columns to the CSV header and ends the line.
void printGapCsvHeader() {
  for (uint8_t i = 0; i < GAP_COUNT; ++i) {
    Serial.printf(",gap_%s_raw", GAP_NAMES[i]);
    Serial.printf(",gap_%s_calibrated", GAP_NAMES[i]);
    Serial.printf(",gap_%s_valid", GAP_NAMES[i]);
  }

  Serial.println();
}

// Appends the gap columns to a CSV record and ends the line.
void printGapCsvRecord() {
  for (uint8_t i = 0; i < GAP_COUNT; ++i) {
    Serial.printf(",%u", gapReading[i].raw);

    int32_t calibrated = 0;
    Serial.print(",");

    if (gapCalibratedValue(i, calibrated)) {
      Serial.print(calibrated);
    }

    Serial.printf(",%u", gapReading[i].valid ? 1 : 0);
  }

  Serial.println();
}

void printGapStatus() {
  Serial.println("GAPS (0=together, 1000=apart)");

  for (uint8_t i = 0; i < GAP_COUNT; ++i) {
    int32_t calibrated = 0;
    bool ok = gapCalibratedValue(i, calibrated);

    Serial.printf(
        "gap_%s: gpio=%u raw=%u valid=%u calibrated=%u value=%ld\n",
        GAP_NAMES[i], GAP_PINS[i], gapReading[i].raw,
        gapReading[i].valid ? 1 : 0,
        gapCalibration[i].valid ? 1 : 0,
        ok ? (long)calibrated : -1L);
  }

  Serial.println("Use: CALIBRATE_GAP <gap> TOGETHER|APART");
  Serial.println("Gaps: polegar_indicador, indicador_medio, "
                 "medio_anelar, anelar_minimo");
}

// ============================================================
// SENSOR WINDOW / FUTURE MODEL INPUT
// ============================================================

// Feature order is fixed. Keep the same order and scaling when
// preparing the model's training dataset:
// 0..4: calibrated flex sensors, normalized to 0.0..1.0
// 5..7: accelerometer X/Y/Z, normalized by +/-2 g
// 8..10: gyroscope X/Y/Z, normalized by +/-250 degrees/second
// 11..14: four inter-finger magnetic gap sensors, normalized to 0.0..1.0.
//
// A row is valid only when all five flex sensors are calibrated,
// their readings are valid and not flagged stuck, and the MPU read
// succeeded. Invalid values are stored as zero, but the whole window
// must be valid before it can be passed to the model.

void addCurrentSampleToWindow(float ax, float ay, float az,
                              float gx, float gy, float gz,
                              bool motionValid) {
  float features[MODEL_FEATURE_COUNT] = {0.0f};
  bool frameValid = motionValid;

  for (uint8_t i = 0; i < FLEX_COUNT; ++i) {
    int32_t value = 0;
    if (calibratedValue(i, value) &&
        flexReading[i].valid &&
        !flexReading[i].stuck) {
      features[i] = value / 1000.0f;
    } else {
      frameValid = false;
    }
  }

  if (motionValid) {
    features[5] = ax / 2.0f;
    features[6] = ay / 2.0f;
    features[7] = az / 2.0f;
    features[8] = gx / 250.0f;
    features[9] = gy / 250.0f;
    features[10] = gz / 250.0f;
  } else {
    frameValid = false;
  }

  // Finger gap sensors -> features 11..14.
  constexpr uint8_t GAP_FEATURE_OFFSET = 11;
  static_assert(GAP_FEATURE_OFFSET + GAP_COUNT == MODEL_FEATURE_COUNT,
                "MODEL_FEATURE_COUNT must be 11 + GAP_COUNT (15)");

  for (uint8_t i = 0; i < GAP_COUNT; ++i) {
    int32_t gapValue = 0;

    if (gapCalibratedValue(i, gapValue) && gapReading[i].valid) {
      features[GAP_FEATURE_OFFSET + i] = gapValue / 1000.0f;
    } else {
      frameValid = false;
    }
  }

  sensorWindow.push(features, frameValid);
}

// Process one complete 40 x 15 window, then clear it so the next
// 40 samples form a new, non-overlapping gesture window.
void processCompletedWindow() {
  static float modelInput[MODEL_WINDOW_SAMPLES][MODEL_FEATURE_COUNT];

  lastWindowValid = sensorWindow.copyTo(lastCompletedWindow);
  lastWindowAvailable = true;
  Serial.printf("# WINDOW_COMPLETE samples=%u valid=%u\n",
                sensorWindow.size(), lastWindowValid ? 1 : 0);

  if (!lastWindowValid) {
    Serial.println("# WINDOW_REJECTED: one or more sensor samples are invalid or uncalibrated");
    sensorWindow.clear();
    return;
  }

  memcpy(modelInput, lastCompletedWindow, sizeof(modelInput));

  ModelPrediction prediction{};
  if (runModelInference(modelInput, prediction)) {
    Serial.printf("# MODEL_RESULT class=%d confidence=%.5f\\n",
                  prediction.classIndex, prediction.confidence);
  } else {
    Serial.println("# MODEL_NOT_READY: window captured; inference adapter has no trained model yet");
  }

  sensorWindow.clear();
}

void printSensorWindow() {
  if (!sensorWindow.isFull()) {
    Serial.printf("WINDOW_NOT_READY: %u/%u samples\n",
                  sensorWindow.size(), MODEL_WINDOW_SAMPLES);
    return;
  }

  Serial.printf("WINDOW_READY: samples=%u valid=%u features=%u\n",
                MODEL_WINDOW_SAMPLES,
                sensorWindow.isValid() ? 1 : 0,
                MODEL_FEATURE_COUNT);
   Serial.println("row,flex_thumb,flex_index,flex_middle,flex_ring,flex_pinky,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z,gap_thumb_index,gap_index_middle,gap_middle_ring,gap_ring_pinky,row_valid");

  float row[MODEL_FEATURE_COUNT];
  bool rowValid = false;

  for (uint16_t i = 0; i < MODEL_WINDOW_SAMPLES; ++i) {
    for (uint8_t j = 0; j < MODEL_FEATURE_COUNT; ++j) row[j] = source[i][j];
    rowValid = sourceValid;
    Serial.print(i);

    for (uint8_t j = 0; j < MODEL_FEATURE_COUNT; ++j) {
      Serial.print(",");
      Serial.print(row[j], 6);
    }

    Serial.printf(",%u\n", rowValid ? 1 : 0);
  }

  Serial.println("# End of sensor window");
}

// ============================================================
// CSV OUTPUT
// ============================================================

void printCsvHeader() {
  Serial.print(
      "session_id,sample_id,timestamp_ms");

  for (uint8_t i = 0; i < FLEX_COUNT; ++i) {
    Serial.printf(",flex_%s_raw", FLEX_NAMES[i]);
    Serial.printf(",flex_%s_calibrated", FLEX_NAMES[i]);
    Serial.printf(",flex_%s_valid", FLEX_NAMES[i]);
    Serial.printf(",flex_%s_stuck", FLEX_NAMES[i]);
  }

  Serial.print(
      ",accel_x_g,accel_y_g,accel_z_g,gyro_x_dps,gyro_y_dps,gyro_z_dps,motion_valid");
  printGapCsvHeader();
}

void printCsvRecord(float ax, float ay, float az,
                    float gx, float gy, float gz,
                    bool motionValid) {
  Serial.printf("%lu,%lu,%lu",
      (unsigned long)sessionId,
      (unsigned long)sampleId,
      (unsigned long)millis());

  for (uint8_t i = 0; i < FLEX_COUNT; ++i) {
    Serial.printf(",%u", flexReading[i].raw);

    int32_t calibrated = 0;

    Serial.print(",");

    if (calibratedValue(i, calibrated)) {
      Serial.print(calibrated);
    }

    Serial.printf(",%u", flexReading[i].valid ? 1 : 0);
    Serial.printf(",%u", flexReading[i].stuck ? 1 : 0);
  }

  Serial.print(",");
  if (motionValid) Serial.print(ax, 4);

  Serial.print(",");
  if (motionValid) Serial.print(ay, 4);

  Serial.print(",");
  if (motionValid) Serial.print(az, 4);

  Serial.print(",");
  if (motionValid) Serial.print(gx, 4);

  Serial.print(",");
  if (motionValid) Serial.print(gy, 4);

  Serial.print(",");
  if (motionValid) Serial.print(gz, 4);

  Serial.printf(",%u", motionValid ? 1 : 0);
  printGapCsvRecord();
}

// ============================================================
// STATUS AND COMMANDS
// ============================================================

void printStatus() {
  Serial.println("STATUS");
  Serial.printf("collecting=%u\n", collecting ? 1 : 0);
  Serial.printf("mpu_available=%u\n", mpuAvailable ? 1 : 0);
  Serial.printf("mpu_address=0x%02X\n", mpuAddress);
  Serial.printf("mpu_who_am_i=0x%02X\n", mpuWhoAmI);
  Serial.printf("sample_interval_ms=%lu\n",
                (unsigned long)sampleIntervalMs);
  Serial.printf("measured_hz=%.2f\n", measuredHz);
  Serial.printf("total_samples=%lu\n",
                (unsigned long)totalSamples);
  Serial.printf("late_samples=%lu\n",
                (unsigned long)lateSamples);
  Serial.printf("invalid_flex_reads=%lu\n",
                (unsigned long)invalidFlexReads);
  Serial.printf("mpu_failures=%lu\n",
                (unsigned long)mpuFailures);
  Serial.printf("window_samples=%u/%u\n",
                sensorWindow.size(), MODEL_WINDOW_SAMPLES);
  Serial.printf("window_full=%u\n", sensorWindow.isFull() ? 1 : 0);
  Serial.printf("window_valid=%u\n", sensorWindow.isValid() ? 1 : 0);

  for (uint8_t i = 0; i < FLEX_COUNT; ++i) {
    Serial.printf(
        "%s: gpio=%u raw=%u valid=%u calibrated=%u stuck=%u\n",
        FLEX_NAMES[i],
        FLEX_PINS[i],
        flexReading[i].raw,
        flexReading[i].valid ? 1 : 0,
        calibration[i].valid ? 1 : 0,
        flexReading[i].stuck ? 1 : 0);
  }
}

void printHelp() {
  Serial.println("Commands:");
  Serial.println("START");
  Serial.println("PAUSE");
  Serial.println("STOP");
  Serial.println("STATUS");
  Serial.println("CONFIG");
  Serial.println("CONFIG SAMPLE_MS <1..1000>");
  Serial.println("CALIBRATE <finger> OPEN");
  Serial.println("CALIBRATE <finger> CLOSED");
  Serial.println("DIAG");
  Serial.println("I2C (scan the I2C bus)");
  Serial.println("MPU (re-initialize the MPU-6050 with verbose log)");
  Serial.println("WINDOW (dump the latest 40-sample matrix; pause collection first)");
  Serial.println("GAPS (show the 4 finger-gap sensors)");              // <-- add
  Serial.println("CALIBRATE_GAP <gap> TOGETHER|APART");                 // <-- add
  Serial.println("HELP");
  Serial.println("Finger: polegar, indicador, medio, anelar, minimo");
}

int fingerIndexFromName(String name) {
  name.toLowerCase();

  for (uint8_t i = 0; i < FLEX_COUNT; ++i) {
    if (name == FLEX_NAMES[i]) {
      return i;
    }
  }

  return -1;
}

void processCommand(String command) {
  command.trim();

  if (command.isEmpty()) {
    return;
  }

  // Keep the CSV stream free of diagnostic messages.
  // Only START begins a CSV session; PAUSE/STOP end it.
  if (collecting) {
    if (command.equalsIgnoreCase("PAUSE") ||
        command.equalsIgnoreCase("STOP")) {
      collecting = false;
      Serial.println("# collection_stopped");
    }
    return;
  }

  if (command.equalsIgnoreCase("START")) {
    ++sessionId;
    sampleId = 0;
    sensorWindow.clear();
    collecting = true;
    nextSampleMs = millis();
    measurementWindowStart = millis();
    measurementWindowSamples = 0;

    printCsvHeader();
    return;
  }

  if (command.equalsIgnoreCase("PAUSE") ||
      command.equalsIgnoreCase("STOP")) {
    collecting = false;
    Serial.println("OK: collection stopped");
    return;
  }

  if (command.equalsIgnoreCase("WINDOW")) {
    printSensorWindow();
    return;
  }

  if (command.equalsIgnoreCase("I2C")) {
    scanI2C();
    return;
  }

  if (command.equalsIgnoreCase("MPU")) {
    mpuConsecutiveFailures = 0;
    mpuAvailable = initializeMPU(true);
    Serial.printf("mpu_available=%u\n", mpuAvailable ? 1 : 0);
    return;
  }

    if (command.equalsIgnoreCase("STATUS") ||
      command.equalsIgnoreCase("DIAG")) {
    readFlexSensors();
    readGapSensors();
    printStatus();
    printGapStatus();
    return;
  }

  if (command.equalsIgnoreCase("HELP")) {
    printHelp();
    return;
  }

  if (command.equalsIgnoreCase("CONFIG")) {
    Serial.printf("sample_interval_ms=%lu\n",
                  (unsigned long)sampleIntervalMs);
    Serial.println("accelerometer_range=+/-2g");
    Serial.println("gyroscope_range=+/-250 dps");
    Serial.println("calibration_output_range=0..1000");
    Serial.printf("stuck_detection_samples=%u (%.1f seconds at current rate)\n",
                  STUCK_SAMPLE_LIMIT,
                  STUCK_SAMPLE_LIMIT * sampleIntervalMs / 1000.0f);
    return;
  }

  if (command.startsWith("CONFIG SAMPLE_MS ")) {
    long value = command.substring(17).toInt();

    if (value >= 1 && value <= 1000) {
      sampleIntervalMs = (uint32_t)value;
      Serial.println("OK: sampling interval updated");
    } else {
      Serial.println("ERROR: interval must be 1..1000 ms");
    }
    return;
  }

  if (command.startsWith("CALIBRATE ")) {
    int firstSpace = command.indexOf(' ');
    int secondSpace = command.indexOf(' ', firstSpace + 1);

    if (secondSpace < 0) {
      Serial.println("ERROR: use CALIBRATE <finger> OPEN|CLOSED");
      return;
    }

    String fingerName =
        command.substring(firstSpace + 1, secondSpace);
    String position =
        command.substring(secondSpace + 1);
    position.trim();
    position.toUpperCase();

    int index = fingerIndexFromName(fingerName);

    if (index < 0 ||
        (position != "OPEN" && position != "CLOSED")) {
      Serial.println("ERROR: invalid finger or position");
      return;
    }

    if (captureCalibration(index, position == "OPEN")) {
      Serial.printf("OK: %s %s reference=%ld calibrated=%u\n",
          FLEX_NAMES[index],
          position.c_str(),
          (long)(position == "OPEN"
              ? calibration[index].openValue
              : calibration[index].closedValue),
          calibration[index].valid ? 1 : 0);
    } else {
      Serial.println("ERROR: calibration capture invalid");
    }
    return;
  } 
    if (command.equalsIgnoreCase("GAPS")) {
    readGapSensors();
    printGapStatus();
    return;
  }

  if (command.startsWith("CALIBRATE_GAP ")) {
    int firstSpace = command.indexOf(' ');
    int secondSpace = command.indexOf(' ', firstSpace + 1);

    if (secondSpace < 0) {
      Serial.println("ERROR: use CALIBRATE_GAP <gap> TOGETHER|APART");
      return;
    }

    String gapName = command.substring(firstSpace + 1, secondSpace);
    String position = command.substring(secondSpace + 1);
    position.trim();
    position.toUpperCase();

    int index = gapIndexFromName(gapName);

    if (index < 0 ||
        (position != "TOGETHER" && position != "APART")) {
      Serial.println("ERROR: invalid gap or position");
      return;
    }

    if (captureGapCalibration(index, position == "TOGETHER")) {
      Serial.printf("OK: gap_%s %s reference=%ld calibrated=%u\n",
          GAP_NAMES[index],
          position.c_str(),
          (long)(position == "TOGETHER"
              ? gapCalibration[index].togetherValue
              : gapCalibration[index].apartValue),
          gapCalibration[index].valid ? 1 : 0);
    } else {
      Serial.println("ERROR: calibration capture invalid");
    }
    return;                                   // <-- added
  }                                           // <-- added (closes CALIBRATE_GAP if)

  Serial.println("ERROR: unknown command; send HELP");   // <-- added
}                                             // <-- added (closes processCommand)

void pollSerialCommands() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();

    if (c == '\n' || c == '\r') {
      if (!commandBuffer.isEmpty()) {
        processCommand(commandBuffer);
        commandBuffer = "";
      }
    } else if (commandBuffer.length() < 100) {
      commandBuffer += c;
    } else {
      commandBuffer = "";
    }
  }
}

// ============================================================
// SETUP AND MAIN LOOP
// ============================================================

void setup() {
  Serial.begin(SERIAL_BAUD);

  for (uint8_t i = 0; i < FLEX_COUNT; ++i) {
    pinMode(FLEX_PINS[i], INPUT);

    flexReading[i].raw = 0;
    flexReading[i].previousRaw = UINT16_MAX;
    flexReading[i].valid = false;
    flexReading[i].stuck = false;
    flexReading[i].unchangedSamples = 0;

    calibration[i] = {-1, -1, false};
  }

  // Set ADC resolution for the classic ESP32 Arduino core.
  analogReadResolution(12);

  // Limit ADC input range to the ESP32's supported range.
  // Actual voltage limits depend on the exact ESP32 variant.
  analogSetPinAttenuation(34, ADC_11db);
  analogSetPinAttenuation(35, ADC_11db);
  analogSetPinAttenuation(32, ADC_11db);
  analogSetPinAttenuation(33, ADC_11db);
  analogSetPinAttenuation(25, ADC_11db);
  setupGapSensors();

  Wire.begin(MPU_SDA, MPU_SCL);
  Wire.setTimeOut(20);

  loadCalibration();
  loadGapCalibration();

  Serial.println("# Libras sensor acquisition firmware");

  // Aguarda o modulo estabilizar apos energizar e tenta
  // algumas vezes antes de desistir.
  delay(100);

  for (uint8_t attempt = 1; attempt <= 3 && !mpuAvailable; ++attempt) {
    Serial.printf("# MPU init, tentativa %u\n", attempt);
    mpuAvailable = initializeMPU(true);

    if (!mpuAvailable) {
      delay(100);
    }
  }

  if (!mpuAvailable) {
    Serial.println("# MPU nao inicializou. Escaneando I2C...");
    scanI2C();
  }

  Serial.printf("# MPU6050 available=%u\n",
                mpuAvailable ? 1 : 0);
  Serial.println("# Sampling default: 20 Hz (50 ms)");
  Serial.println("# A sensor is flagged stuck after 5 s with exactly unchanged ADC readings.");
  Serial.println("# Send HELP for commands");
}

void loop() {
  pollSerialCommands();

  uint32_t now = millis();

  if (!collecting) {
    // Mesmo parado, tenta recuperar o MPU em segundo plano.
    retryMpuIfNeeded(now);
    return;
  }

  if ((int32_t)(now - nextSampleMs) < 0) {
    return;
  }

  // Keep the sampling schedule anchored to the original deadlines.
  // Count only the slots actually skipped; never create catch-up samples.
  uint32_t missedSlots = (now - nextSampleMs) / sampleIntervalMs;
  lateSamples += missedSlots;
  nextSampleMs += (missedSlots + 1) * sampleIntervalMs;

  readFlexSensors();
  readGapSensors();

  float ax = NAN;
  float ay = NAN;
  float az = NAN;
  float gx = NAN;
  float gy = NAN;
  float gz = NAN;

  bool motionValid = false;

  if (mpuAvailable) {
    motionValid = readMotion(ax, ay, az, gx, gy, gz);

    if (motionValid) {
      mpuConsecutiveFailures = 0;
    } else {
      ++mpuFailures;

      // Tolera falhas isoladas de I2C; so desativa o MPU
      // apos varias falhas seguidas.
      if (++mpuConsecutiveFailures >= MPU_FAIL_LIMIT) {
        mpuAvailable = false;
      }
    }
  }

  retryMpuIfNeeded(now);

  addCurrentSampleToWindow(ax, ay, az, gx, gy, gz, motionValid);

  if (sensorWindow.isFull()) {
    processCompletedWindow();
  }

  printCsvRecord(ax, ay, az, gx, gy, gz, motionValid);

  ++sampleId;
  ++totalSamples;
  ++measurementWindowSamples;

  if (now - measurementWindowStart >= 1000) {
    measuredHz =
        measurementWindowSamples * 1000.0f /
        (now - measurementWindowStart);

    measurementWindowStart = now;
    measurementWindowSamples = 0;
  }
}
