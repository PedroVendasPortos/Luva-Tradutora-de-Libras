#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <math.h>
#include "SensorWindow.h"

// ============================================================
// HARDWARE CONFIGURATION
// ============================================================

constexpr uint8_t FLEX_COUNT = 5;
constexpr uint8_t GAP_COUNT = 3;

const uint8_t FLEX_PINS[FLEX_COUNT] = {
  33,  // Polegar (ADC1)
  34,  // Indicador (ADC1)
  14,  // Medio (ADC2)
  12,  // Anelar (ADC2; pino de strapping)
  25   // Minimo (ADC2)
};

// NOTA: GPIOs 12, 14, 25 e 13 pertencem ao ADC2 no ESP32 classico
// e nao podem ser usados para leitura analogica enquanto o Wi-Fi estiver
// ativo. Este firmware nao utiliza Wi-Fi. GPIO12 tambem e pino de strapping:
// o circuito conectado a ele nao pode forcar nivel alto durante a inicializacao,
// ou o ESP32 pode falhar ao iniciar.

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
constexpr uint16_t STUCK_SAMPLE_LIMIT = 100; // 5 seconds at 20 Hz

// ---- Aquisicao ADC estavel ----
// Cada leitura: 1 conversao descartada (troca de canal do
// multiplexador) + ADC_SAMPLES conversoes que sao promediadas.
constexpr uint8_t ADC_SAMPLES = 8;
constexpr uint16_t ADC_SETTLE_US = 200;   // apos descartar a 1a leitura
constexpr uint16_t ADC_INTERVAL_US = 100; // entre as amostras

// ---- Deteccao de canal morto / pino sem sensor ----
// Uma leitura no trilho (0 ou 4095) e uma medida legitima: o dedo pode estar
// no fim de curso. O que NAO e legitimo e o canal ficar PRESO no trilho ou
// oscilar como um pino solto. As condicoes abaixo so invalidam o canal quando
// persistem, para que um unico pico de ruido nao derrube a leitura inteira.
constexpr uint16_t CHANNEL_FAULT_SAMPLES = 20; // 1 s a 20 Hz
constexpr uint16_t FLOAT_SPREAD_COUNTS = 250;  // oscilacao entre as 8 conversoes
                                               // tipica de pino flutuante;
                                               // divisor resistivo fica ~10..60

// ---- Filtro exponencial (EMA) ----
// 0 < alpha <= 1. Menor = mais suave, porem com mais atraso.
constexpr float FLEX_FILTER_ALPHA = 0.20f;
constexpr float GAP_FILTER_ALPHA = 0.20f;

// ---- Calibracao ----
// Mantenha o dedo IMOVEL durante a captura.
constexpr uint16_t CALIBRATION_SAMPLES = 100;
constexpr uint16_t CALIBRATION_TRIM = 10;          // descarta 10 menores e 10 maiores
constexpr uint16_t CALIBRATION_MAX_SPREAD = 60;    // dispersao maxima (contagens ADC) apos o corte
constexpr uint16_t CALIBRATION_MAX_CLIPPED = 5;    // leituras saturadas toleradas

// ---- Calibracao do offset do giroscopio ----
constexpr uint16_t GYRO_CAL_SAMPLES = 200;
constexpr float GYRO_CAL_MAX_SPREAD_DPS = 3.0f;    // se variar mais que isso, o MPU se moveu
constexpr uint8_t GYRO_CAL_MAX_READ_FAILS = 10;

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
  uint16_t raw;              // valor filtrado (media de 8 + EMA)
  uint16_t spread;           // max-min das 8 conversoes (ruido do canal)
  bool valid;
  bool stuck;
  bool saturated;            // alguma conversao encostou no trilho
  uint16_t unchangedSamples;
  uint16_t saturatedSamples; // amostras seguidas no trilho
  uint16_t noisySamples;     // amostras seguidas com spread alto
  uint32_t previousSum;      // soma bruta das 8 conversoes (usada no stuck)
};

// Resultado da captura de uma referencia de calibracao. Separar os casos
// permite dizer ao usuario se o problema e o dedo que se moveu, o divisor
// saturado ou o sensor/canal sem sinal.
//
// Fica declarado aqui, antes da primeira funcao, porque a Arduino IDE
// gera prototipos automaticamente no topo do .ino: um tipo usado na
// assinatura precisa existir antes desse ponto.
enum CaptureStatus {
  CAPTURE_OK,
  CAPTURE_SATURATED,    // todas as amostras no trilho: nada a medir
  CAPTURE_UNSTABLE,     // dedo se moveu, fio solto ou ruido alto
  CAPTURE_UNAVAILABLE   // canal desativado / indice invalido
};

FlexCalibration calibration[FLEX_COUNT];
FlexReading flexReading[FLEX_COUNT];

// Estado do filtro EMA dos flex.
float filteredFlex[FLEX_COUNT] = {0};
bool flexFilterInitialized[FLEX_COUNT] = {false};

// Offset do giroscopio (graus/s), subtraido de todas as leituras.
float gyroOffset[3] = {0.0f, 0.0f, 0.0f};
bool gyroOffsetValid = false;

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
uint32_t lastAcquisitionUs = 0;  // tempo dos 7 canais em uma amostra

uint32_t measurementWindowStart = 0;
uint32_t measurementWindowSamples = 0;
float measuredHz = 0.0f;

String commandBuffer;

// ============================================================
// ADC HELPERS (leitura estavel, filtro e referencia de calibracao)
// ============================================================

// Le um canal analogico de forma estavel:
//  1) descarta a primeira conversao (efeito da troca de canal);
//  2) espera o capacitor de amostragem assentar;
//  3) promedia ADC_SAMPLES conversoes.
// 'sum' devolve a soma bruta (resolucao 8x maior, util para o
// detector de sensor travado). 'clipped' indica se o VALOR MEDIO
// encostou no trilho (0 ou 4095): um pico isolado de uma conversao
// nao pode marcar um canal bom como saturado. 'spread' e a diferenca
// entre a 2a maior e a 2a menor das 8 conversoes - descarta um extremo
// de cada lado, que e o melhor indicador de pino flutuante (sem sensor)
// sem se deixar enganar por um unico pico de ruido.
uint16_t readStableADC(uint8_t pin, bool& clipped, uint32_t& sum,
                       uint16_t& spread) {
  analogRead(pin);  // descarta
  delayMicroseconds(ADC_SETTLE_US);

  sum = 0;

  uint16_t low1 = ADC_MAX_VALUE;
  uint16_t low2 = ADC_MAX_VALUE;
  uint16_t high1 = 0;
  uint16_t high2 = 0;

  for (uint8_t i = 0; i < ADC_SAMPLES; ++i) {
    const uint16_t v = analogRead(pin);

    if (v < low1) {
      low2 = low1;
      low1 = v;
    } else if (v < low2) {
      low2 = v;
    }

    if (v > high1) {
      high2 = high1;
      high1 = v;
    } else if (v > high2) {
      high2 = v;
    }

    sum += v;
    delayMicroseconds(ADC_INTERVAL_US);
  }

  const uint16_t averaged =
      (uint16_t)((sum + ADC_SAMPLES / 2) / ADC_SAMPLES);

  clipped = (averaged == 0) || (averaged >= ADC_MAX_VALUE);
  spread = (uint16_t)(high2 - low2);

  return averaged;
}

// Filtro exponencial generico (um estado por sensor).
uint16_t emaFilter(float& state, bool& initialized,
                   uint16_t sample, float alpha) {
  if (!initialized) {
    state = sample;
    initialized = true;
  } else {
    state = alpha * sample + (1.0f - alpha) * state;
  }

  return (uint16_t)lroundf(state);
}

void sortU16(uint16_t* data, uint16_t n) {
  for (uint16_t i = 1; i < n; ++i) {
    uint16_t key = data[i];
    int32_t j = (int32_t)i - 1;

    while (j >= 0 && data[j] > key) {
      data[j + 1] = data[j];
      --j;
    }

    data[j + 1] = key;
  }
}

// Captura uma referencia de calibracao robusta:
// CALIBRATION_SAMPLES leituras estaveis, ordena, descarta os extremos
// e faz a media do miolo.
//
// Saturação PARCIAL e aceita (o dedo pode estar no fim de curso e o valor
// no trilho e uma referencia valida). So rejeitamos quando TODAS as
// amostras ficam no trilho, o que significa que o canal nao esta medindo
// nada (divisor em 5 V, falta de GND, fio rompido ou GPIO errado).
CaptureStatus captureReference(uint8_t pin, int32_t& result,
                               uint16_t* clippedOut = nullptr) {
  uint16_t samples[CALIBRATION_SAMPLES];
  uint16_t clippedCount = 0;

  for (uint16_t n = 0; n < CALIBRATION_SAMPLES; ++n) {
    bool clipped = false;
    uint32_t sum = 0;
    uint16_t spread = 0;

    samples[n] = readStableADC(pin, clipped, sum, spread);

    if (clipped) {
      ++clippedCount;
    }

    yield();
  }

  if (clippedOut != nullptr) {
    *clippedOut = clippedCount;
  }

  if (clippedCount == CALIBRATION_SAMPLES) {
    return CAPTURE_SATURATED;
  }

  sortU16(samples, CALIBRATION_SAMPLES);

  uint16_t first = CALIBRATION_TRIM;
  uint16_t last = CALIBRATION_SAMPLES - CALIBRATION_TRIM - 1;

  if (samples[last] - samples[first] > CALIBRATION_MAX_SPREAD) {
    return CAPTURE_UNSTABLE;
  }

  uint32_t total = 0;
  uint16_t count = 0;

  for (uint16_t i = first; i <= last; ++i) {
    total += samples[i];
    ++count;
  }

  result = (int32_t)((total + count / 2) / count);
  return CAPTURE_OK;
}

// Mensagem de erro especifica para o usuario final.
const char* captureStatusText(CaptureStatus status) {
  switch (status) {
    case CAPTURE_SATURATED:
      return "canal colado em 0/4095 nas 100 amostras: o sensor nao esta "
             "entregando sinal (divisor em 5 V? falta GND? fio rompido? "
             "GPIO errado?). Rode SELFTEST e SCAN.";
    case CAPTURE_UNSTABLE:
      return "leitura instavel: o dedo se moveu, o fio esta solto ou o "
             "canal tem ruido alto. Prenda o dedo e repita. Se persistir, "
             "rode SELFTEST para ver min/max do canal.";
    case CAPTURE_UNAVAILABLE:
      return "canal desativado ou inexistente.";
    case CAPTURE_OK:
    default:
      return "ok";
  }
}

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

// Leitura sem correcao de offset (usada pela calibracao do giroscopio).
bool readMotionRaw(float& ax, float& ay, float& az,
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

// Leitura com o offset do giroscopio ja removido.
bool readMotion(float& ax, float& ay, float& az,
                float& gx, float& gy, float& gz) {
  if (!readMotionRaw(ax, ay, az, gx, gy, gz)) {
    return false;
  }

  gx -= gyroOffset[0];
  gy -= gyroOffset[1];
  gz -= gyroOffset[2];

  return true;
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
// GYROSCOPE OFFSET CALIBRATION
// ============================================================
// Mantenha a luva COMPLETAMENTE parada (sobre a mesa) durante ~1 s.

void loadGyroCalibration() {
  gyroOffset[0] = preferences.getFloat("gyoffx", NAN);
  gyroOffset[1] = preferences.getFloat("gyoffy", NAN);
  gyroOffset[2] = preferences.getFloat("gyoffz", NAN);

  gyroOffsetValid = std::isfinite(gyroOffset[0]) &&
                    std::isfinite(gyroOffset[1]) &&
                    std::isfinite(gyroOffset[2]);

  if (!gyroOffsetValid) {
    gyroOffset[0] = gyroOffset[1] = gyroOffset[2] = 0.0f;
  }
}

bool captureGyroCalibration() {
  if (!mpuAvailable) {
    return false;
  }

  double sum[3] = {0.0, 0.0, 0.0};
  float minV[3] = { 1e9f,  1e9f,  1e9f};
  float maxV[3] = {-1e9f, -1e9f, -1e9f};

  uint16_t good = 0;
  uint8_t fails = 0;

  while (good < GYRO_CAL_SAMPLES) {
    float ax, ay, az, gx, gy, gz;

    if (!readMotionRaw(ax, ay, az, gx, gy, gz)) {
      if (++fails > GYRO_CAL_MAX_READ_FAILS) {
        return false;
      }
      delay(5);
      continue;
    }

    float g[3] = {gx, gy, gz};

    for (uint8_t i = 0; i < 3; ++i) {
      sum[i] += g[i];
      if (g[i] < minV[i]) minV[i] = g[i];
      if (g[i] > maxV[i]) maxV[i] = g[i];
    }

    ++good;
    delay(5);
    yield();
  }

  for (uint8_t i = 0; i < 3; ++i) {
    if (maxV[i] - minV[i] > GYRO_CAL_MAX_SPREAD_DPS) {
      return false;  // o sensor se moveu durante a captura
    }
  }

  for (uint8_t i = 0; i < 3; ++i) {
    gyroOffset[i] = (float)(sum[i] / GYRO_CAL_SAMPLES);
  }

  preferences.putFloat("gyoffx", gyroOffset[0]);
  preferences.putFloat("gyoffy", gyroOffset[1]);
  preferences.putFloat("gyoffz", gyroOffset[2]);

  gyroOffsetValid = true;
  return true;
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

// Atualiza os contadores de falha de um canal analogico.
// Retorna false apenas quando a falha PERSISTE (canal preso no trilho ou
// oscilando como pino solto). Uma saturacao ou um pico de ruido isolado
// nao invalidam a leitura: o valor no trilho pode ser o fim de curso real.
bool updateChannelHealth(uint16_t spread, bool clipped,
                         uint16_t& saturatedSamples,
                         uint16_t& noisySamples) {
  if (clipped) {
    if (saturatedSamples < UINT16_MAX) {
      ++saturatedSamples;
    }
  } else {
    saturatedSamples = 0;
  }

  if (spread >= FLOAT_SPREAD_COUNTS) {
    if (noisySamples < UINT16_MAX) {
      ++noisySamples;
    }
  } else {
    noisySamples = 0;
  }

  return saturatedSamples < CHANNEL_FAULT_SAMPLES &&
         noisySamples < CHANNEL_FAULT_SAMPLES;
}

void readFlexSensors() {
  for (uint8_t i = 0; i < FLEX_COUNT; ++i) {
    bool clipped = false;
    uint32_t sum = 0;
    uint16_t spread = 0;

    uint16_t averaged = readStableADC(FLEX_PINS[i], clipped, sum, spread);

    flexReading[i].spread = spread;
    flexReading[i].saturated = clipped;

    // Detector de sensor travado usa a soma bruta (antes do filtro),
    // porque o valor filtrado quase nunca muda de forma visivel em
    // repouso e geraria falsos positivos.
    if (sum == flexReading[i].previousSum) {
      if (flexReading[i].unchangedSamples < UINT16_MAX) {
        ++flexReading[i].unchangedSamples;
      }
    } else {
      flexReading[i].unchangedSamples = 0;
    }

    flexReading[i].previousSum = sum;

    flexReading[i].stuck =
        flexReading[i].unchangedSamples >= STUCK_SAMPLE_LIMIT;

    const bool healthy =
        updateChannelHealth(spread, clipped,
                            flexReading[i].saturatedSamples,
                            flexReading[i].noisySamples);

    flexReading[i].valid = healthy && !flexReading[i].stuck;

    // So alimenta o filtro com amostras limpas, para que uma
    // saturacao momentanea nao contamine o estado.
    if (!clipped && spread < FLOAT_SPREAD_COUNTS) {
      flexReading[i].raw = emaFilter(filteredFlex[i],
                                     flexFilterInitialized[i],
                                     averaged, FLEX_FILTER_ALPHA);
    } else if (flexFilterInitialized[i]) {
      // Amostra atipica isolada: preserva o ultimo valor filtrado em vez
      // de injetar um pico do trilho (0/4095) no meio da serie.
      flexReading[i].raw = (uint16_t)lroundf(filteredFlex[i]);
    } else {
      flexReading[i].raw = averaged;
    }

    if (!flexReading[i].valid) {
      ++invalidFlexReads;
    }
  }
}

// Capture an average reference for one finger.
CaptureStatus captureCalibration(uint8_t index, bool isOpen) {
  if (index >= FLEX_COUNT) {
    return CAPTURE_UNAVAILABLE;
  }

  int32_t average = 0;
  uint16_t clipped = 0;

  CaptureStatus status =
      captureReference(FLEX_PINS[index], average, &clipped);

  if (status != CAPTURE_OK) {
    return status;
  }

  if (clipped > CALIBRATION_MAX_CLIPPED) {
    Serial.printf("# aviso: referencia de %s com %u/%u amostras no trilho; "
                  "resolucao comprometida. Verifique o divisor do sensor.\n",
                  FLEX_NAMES[index], clipped, CALIBRATION_SAMPLES);
  }

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

  if (!calibration[index].valid) {
    Serial.printf("# aviso: %s ainda nao tem OPEN e CLOSED com diferenca "
                  "minima de 20 contagens (open=%ld closed=%ld)\n",
                  FLEX_NAMES[index],
                  (long)calibration[index].openValue,
                  (long)calibration[index].closedValue);
  }

  saveCalibration(index);

  return CAPTURE_OK;
}

// ============================================================
// FINGER GAP SENSORS (3x analog Hall-effect / magnetic)
// ============================================================
//
// One sensor per space between adjacent fingers. A small magnet
// on one finger moves relative to the sensor on the neighbor, so
// the analog output changes with the distance between them.
//
// Calibrated scale: 0    = fingers TOGETHER (touching)
//                   1000 = fingers APART (maximum spread)
// Note: Hall response is not linear with distance. Se o modelo for
// treinado com estes valores, mantenha exatamente a mesma escala
// no treinamento e no firmware.

const uint8_t GAP_PINS[GAP_COUNT] = {
  35,  // indicador-medio (ADC1)
  32,  // medio-anelar (ADC1; sensor informado como quebrado)
  13   // anelar-minimo (ADC2)
};

const bool GAP_ENABLED[GAP_COUNT] = {
  true,
  false, // sensor medio-anelar indisponivel no momento
  true
};

const char* GAP_NAMES[GAP_COUNT] = {
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
  uint16_t raw;  // valor filtrado (media de 8 + EMA)
  uint16_t spread;
  bool valid;
  bool saturated;
  uint16_t saturatedSamples;
  uint16_t noisySamples;
};

GapCalibration gapCalibration[GAP_COUNT];
GapReading gapReading[GAP_COUNT];

float filteredGap[GAP_COUNT] = {0};
bool gapFilterInitialized[GAP_COUNT] = {false};

// Zera o estado dos filtros e dos detectores de falha (chamado no START
// para nao carregar valores antigos para dentro da nova sessao).
void resetFilters() {
  for (uint8_t i = 0; i < FLEX_COUNT; ++i) {
    flexFilterInitialized[i] = false;
    filteredFlex[i] = 0.0f;

    flexReading[i].previousSum = UINT32_MAX;
    flexReading[i].unchangedSamples = 0;
    flexReading[i].saturatedSamples = 0;
    flexReading[i].noisySamples = 0;
    flexReading[i].stuck = false;
    flexReading[i].saturated = false;
    flexReading[i].spread = 0;
  }

  for (uint8_t i = 0; i < GAP_COUNT; ++i) {
    gapFilterInitialized[i] = false;
    filteredGap[i] = 0.0f;

    gapReading[i].saturatedSamples = 0;
    gapReading[i].noisySamples = 0;
    gapReading[i].saturated = false;
    gapReading[i].spread = 0;
  }
}

void setupGapSensors() {
  for (uint8_t i = 0; i < GAP_COUNT; ++i) {
    gapReading[i].raw = 0;
    gapReading[i].valid = false;
    gapCalibration[i] = {-1, -1, false};
    if (GAP_ENABLED[i]) {
      pinMode(GAP_PINS[i], INPUT);
      analogSetPinAttenuation(GAP_PINS[i], ADC_11db);
    }
  }
}

void loadGapCalibration() {
  for (uint8_t i = 0; i < GAP_COUNT; ++i) {
    char key[20];

    // Chaves v2 evitam reutilizar calibracoes da antiga ordem de quatro gaps.
    snprintf(key, sizeof(key), "g2t%u", i);
    gapCalibration[i].togetherValue = preferences.getInt(key, -1);

    snprintf(key, sizeof(key), "g2a%u", i);
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

  snprintf(key, sizeof(key), "g2t%u", index);
  preferences.putInt(key, gapCalibration[index].togetherValue);

  snprintf(key, sizeof(key), "g2a%u", index);
  preferences.putInt(key, gapCalibration[index].apartValue);
}

void readGapSensors() {
  for (uint8_t i = 0; i < GAP_COUNT; ++i) {
    if (!GAP_ENABLED[i]) {
      gapReading[i].raw = 0;
      gapReading[i].spread = 0;
      gapReading[i].valid = false;
      gapReading[i].saturated = false;
      gapReading[i].saturatedSamples = 0;
      gapReading[i].noisySamples = 0;
      continue;
    }

    bool clipped = false;
    uint32_t sum = 0;
    uint16_t spread = 0;

    uint16_t averaged = readStableADC(GAP_PINS[i], clipped, sum, spread);

    gapReading[i].spread = spread;
    gapReading[i].saturated = clipped;

    gapReading[i].valid =
        updateChannelHealth(spread, clipped,
                            gapReading[i].saturatedSamples,
                            gapReading[i].noisySamples);

    if (!clipped && spread < FLOAT_SPREAD_COUNTS) {
      gapReading[i].raw = emaFilter(filteredGap[i],
                                    gapFilterInitialized[i],
                                    averaged, GAP_FILTER_ALPHA);
    } else if (gapFilterInitialized[i]) {
      gapReading[i].raw = (uint16_t)lroundf(filteredGap[i]);
    } else {
      gapReading[i].raw = averaged;
    }
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

CaptureStatus captureGapCalibration(uint8_t index, bool isTogether) {
  if (index >= GAP_COUNT || !GAP_ENABLED[index]) {
    return CAPTURE_UNAVAILABLE;
  }

  int32_t average = 0;
  uint16_t clipped = 0;

  CaptureStatus status =
      captureReference(GAP_PINS[index], average, &clipped);

  if (status != CAPTURE_OK) {
    return status;
  }

  if (clipped > CALIBRATION_MAX_CLIPPED) {
    Serial.printf("# aviso: referencia de gap_%s com %u/%u amostras no "
                  "trilho; resolucao comprometida.\n",
                  GAP_NAMES[index], clipped, CALIBRATION_SAMPLES);
  }

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

  return CAPTURE_OK;
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
  Serial.println("GAPS (0=together, 1000=apart; disabled sensor skipped)");

  for (uint8_t i = 0; i < GAP_COUNT; ++i) {
    if (!GAP_ENABLED[i]) {
      Serial.printf("gap_%s: gpio=%u DISABLED (sensor quebrado)\n",
                    GAP_NAMES[i], GAP_PINS[i]);
      continue;
    }

    int32_t calibrated = 0;
    bool ok = gapCalibratedValue(i, calibrated);

    Serial.printf(
        "gap_%s: gpio=%u raw=%u valid=%u calibrated=%u sat=%u ruido=%u "
        "value=%ld\n",
        GAP_NAMES[i], GAP_PINS[i], gapReading[i].raw,
        gapReading[i].valid ? 1 : 0,
        gapCalibration[i].valid ? 1 : 0,
        gapReading[i].saturated ? 1 : 0,
        gapReading[i].spread,
        ok ? (long)calibrated : -1L);
  }

  Serial.println("Use: CALIBRATE_GAP <gap> TOGETHER|APART");
  Serial.println("Gaps: indicador_medio, medio_anelar, anelar_minimo");
  Serial.println("gap_medio_anelar esta desativado enquanto o sensor estiver quebrado.");
}

// ============================================================
// SENSOR WINDOW / FUTURE MODEL INPUT
// ============================================================

// Feature order is fixed. Keep the same order and scaling when
// preparing the model's training dataset:
// 0..4: calibrated flex sensors, normalized to 0.0..1.0
// 5..7: accelerometer X/Y/Z, normalized by +/-2 g (clamped -1..1)
// 8..10: gyroscope X/Y/Z (offset removed), normalized by
//        +/-250 degrees/second (clamped -1..1)
// 11..13: calibrated gaps (indicador-medio, medio-anelar, anelar-minimo), normalized to 0.0..1.0
//
// A row is valid only when all five flex sensors are calibrated,
// their readings are valid and not flagged stuck, and the MPU read
// succeeded. Invalid values are stored as zero, but the whole window
// must be valid before it can be passed to the model.
//
// IMPORTANTE: o dataset de treinamento deve ser gerado com
// exatamente este mesmo pre-processamento (media de 8 amostras,
// filtro EMA alpha=0.20, calibracao 0..1000, offset do giroscopio
// removido e as mesmas escalas acima).

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
    features[5] = constrain(ax / 2.0f, -1.0f, 1.0f);
    features[6] = constrain(ay / 2.0f, -1.0f, 1.0f);
    features[7] = constrain(az / 2.0f, -1.0f, 1.0f);
    features[8] = constrain(gx / 250.0f, -1.0f, 1.0f);
    features[9] = constrain(gy / 250.0f, -1.0f, 1.0f);
    features[10] = constrain(gz / 250.0f, -1.0f, 1.0f);
  } else {
    frameValid = false;
  }

  // Gap sensors -> features 11..13.
  constexpr uint8_t GAP_FEATURE_OFFSET = 11;
  static_assert(GAP_FEATURE_OFFSET + GAP_COUNT == MODEL_FEATURE_COUNT,
                "MODEL_FEATURE_COUNT must be 11 + GAP_COUNT (14)");

  for (uint8_t i = 0; i < GAP_COUNT; ++i) {
    if (!GAP_ENABLED[i]) {
      features[GAP_FEATURE_OFFSET + i] = 0.0f;
      continue; // sensor quebrado nao invalida toda a janela
    }

    int32_t gapValue = 0;
    if (gapCalibratedValue(i, gapValue) && gapReading[i].valid) {
      features[GAP_FEATURE_OFFSET + i] = gapValue / 1000.0f;
    } else {
      frameValid = false;
    }
  }

  sensorWindow.push(features, frameValid);
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
  Serial.println("row,flex_thumb,flex_index,flex_middle,flex_ring,flex_pinky,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z,gap_index_middle,gap_middle_ring,gap_ring_pinky,row_valid");

  float row[MODEL_FEATURE_COUNT];
  bool rowValid = false;

  for (uint16_t i = 0; i < MODEL_WINDOW_SAMPLES; ++i) {
    sensorWindow.getRow(i, row, rowValid);
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
// DIAGNOSTICO DE FIACAO E DE SENSOR (codigo x hardware)
// ============================================================
//
// Estes dois comandos existem para responder a pergunta:
// "o problema esta no firmware ou na luva?"
//
//   SCAN      -> mostra o valor cru de TODOS os GPIOs analogicos
//                candidatos. Dobrando um dedo de cada vez, voce ve
//                em qual GPIO o sensor realmente esta ligado e se
//                ele responde.
//   SELFTEST  -> mede min/max/ruido dos 8 canais configurados e
//                classifica cada um (travado, preso no trilho,
//                oscilando ou estavel).
//
// Nenhum dos dois altera calibracao, NVS ou coleta.

constexpr uint8_t SCAN_PIN_COUNT = 12;

// ADC1 (32..39) + ADC2 (12,13,14,25,26,27). Nunca inclua 6..11 (flash).
const uint8_t SCAN_PINS[SCAN_PIN_COUNT] = {
  32, 33, 34, 35, 36, 39,   // ADC1
  25, 26, 27, 14, 12, 13    // ADC2
};

constexpr uint8_t SCAN_MAX_SWEEPS = 150;   // 150 x 200 ms = 30 s
constexpr uint16_t SCAN_PERIOD_MS = 200;

bool scanActive = false;
uint32_t scanSweeps = 0;
uint32_t scanLastMs = 0;
uint16_t scanMin[SCAN_PIN_COUNT];
uint16_t scanMax[SCAN_PIN_COUNT];

void startScan() {
  for (uint8_t i = 0; i < SCAN_PIN_COUNT; ++i) {
    pinMode(SCAN_PINS[i], INPUT);
    analogSetPinAttenuation(SCAN_PINS[i], ADC_11db);
    scanMin[i] = ADC_MAX_VALUE;
    scanMax[i] = 0;
  }

  scanSweeps = 0;
  scanLastMs = 0;
  scanActive = true;

  Serial.println("# SCAN iniciado (~30 s). Nada e gravado na calibracao.");
  Serial.println("# 1) mantenha os dedos ESTENDIDOS e observe os valores");
  Serial.println("# 2) dobre UM dedo de cada vez e veja qual coluna muda");
  Serial.println("# 3) envie qualquer comando (ex.: STATUS) para encerrar");

  Serial.print("# gpio");
  for (uint8_t i = 0; i < SCAN_PIN_COUNT; ++i) {
    Serial.printf(",%u", SCAN_PINS[i]);
  }
  Serial.println();
}

void stopScan() {
  scanActive = false;

  if (scanSweeps == 0) {
    Serial.println("# SCAN encerrado antes da primeira varredura.");
    return;
  }

  Serial.println("# SCAN encerrado. Resumo min/max por GPIO:");
  Serial.println("# gpio,min,max,delta");

  for (uint8_t i = 0; i < SCAN_PIN_COUNT; ++i) {
    Serial.printf("# %u,%u,%u,%u\n",
                  SCAN_PINS[i], scanMin[i], scanMax[i],
                  (uint16_t)(scanMax[i] - scanMin[i]));
  }

  Serial.println("# delta alto = aquele GPIO tem um sensor que se moveu.");
  Serial.println("# delta ~0 em todos = nenhum sensor ligado nos pinos varridos.");
}

void runScan() {
  if (!scanActive) {
    return;
  }

  const uint32_t now = millis();

  if (scanSweeps > 0 && (now - scanLastMs) < SCAN_PERIOD_MS) {
    return;
  }

  scanLastMs = now;
  ++scanSweeps;

  Serial.printf("%lu", (unsigned long)now);

  for (uint8_t i = 0; i < SCAN_PIN_COUNT; ++i) {
    const uint16_t value = (uint16_t)analogRead(SCAN_PINS[i]);

    if (value < scanMin[i]) scanMin[i] = value;
    if (value > scanMax[i]) scanMax[i] = value;

    Serial.printf(",%u", value);
  }

  Serial.println();

  if (scanSweeps >= SCAN_MAX_SWEEPS) {
    stopScan();
  }
}

// Classifica um canal a partir das estatisticas do SELFTEST.
const char* classifyChannel(uint16_t lowest, uint16_t highest,
                            uint8_t railLow, uint8_t railHigh,
                            uint8_t rounds) {
  if ((int)railHigh * 10 >= (int)rounds * 9) return "PRESO_EM_4095";
  if ((int)railLow * 10 >= (int)rounds * 9) return "PRESO_EM_0";
  if (highest == lowest) return "TRAVADO";
  if ((uint16_t)(highest - lowest) <= 8) return "ESTAVEL";
  if ((uint16_t)(highest - lowest) <= 300) return "OSCILA_POUCO";
  return "OSCILA_MUITO";
}

void printSelfTestHints() {
  Serial.println("# Como ler cada estado:");
  Serial.println("#  ESTAVEL        canal com sinal estavel (divisor ligado)");
  Serial.println("#  OSCILA_*       variou durante o teste (dedo/luva) ou pino sem sensor");
  Serial.println("#  TRAVADO        nenhuma conversao mudou: fio rompido, GPIO errado, canal morto");
  Serial.println("#  PRESO_EM_4095  divisor em 5 V (VIN) ou saida acima de ~3,1 V");
  Serial.println("#  PRESO_EM_0     falta GND no divisor, resistor aberto ou sensor em curto");
  Serial.println("# Procedimento: rode com os dedos ESTENDIDOS e anote min/max;");
  Serial.println("# rode com um dedo DOBRADO e compare. Se min/max nao mudarem,");
  Serial.println("# o sensor nao chega nesse GPIO (fiacao) - nao e o codigo.");
  Serial.println("# Use SCAN para ver todos os GPIOs ao mesmo tempo.");
}

void runSelfTest() {
  constexpr uint8_t TEST_CHANNELS = FLEX_COUNT + GAP_COUNT;
  constexpr uint8_t ROUNDS = 25;
  constexpr uint16_t ROUND_DELAY_MS = 40;

  uint8_t pins[TEST_CHANNELS];
  const char* names[TEST_CHANNELS];

  for (uint8_t i = 0; i < FLEX_COUNT; ++i) {
    pins[i] = FLEX_PINS[i];
    names[i] = FLEX_NAMES[i];
  }

  for (uint8_t i = 0; i < GAP_COUNT; ++i) {
    pins[FLEX_COUNT + i] = GAP_PINS[i];
    names[FLEX_COUNT + i] = GAP_NAMES[i];
  }

  uint16_t lowest[TEST_CHANNELS];
  uint16_t highest[TEST_CHANNELS];
  uint32_t total[TEST_CHANNELS];
  uint16_t maxSpread[TEST_CHANNELS];
  uint8_t railLow[TEST_CHANNELS];
  uint8_t railHigh[TEST_CHANNELS];

  for (uint8_t c = 0; c < TEST_CHANNELS; ++c) {
    pinMode(pins[c], INPUT);
    analogSetPinAttenuation(pins[c], ADC_11db);

    lowest[c] = ADC_MAX_VALUE;
    highest[c] = 0;
    total[c] = 0;
    maxSpread[c] = 0;
    railLow[c] = 0;
    railHigh[c] = 0;
  }

  Serial.println("# SELFTEST: 8 canais, ~1,5 s. NAO mova a luva durante o teste.");

  for (uint8_t r = 0; r < ROUNDS; ++r) {
    for (uint8_t c = 0; c < TEST_CHANNELS; ++c) {
      bool clipped = false;
      uint32_t sum = 0;
      uint16_t spread = 0;

      const uint16_t value = readStableADC(pins[c], clipped, sum, spread);

      if (value < lowest[c]) lowest[c] = value;
      if (value > highest[c]) highest[c] = value;
      if (spread > maxSpread[c]) maxSpread[c] = spread;

      total[c] += value;

      if (value == 0) ++railLow[c];
      if (value >= ADC_MAX_VALUE) ++railHigh[c];
    }

    delay(ROUND_DELAY_MS);
    yield();
  }

  Serial.println("canal,gpio,min,max,delta,media,ruido_max,total0,total4095,estado");

  for (uint8_t c = 0; c < TEST_CHANNELS; ++c) {
    Serial.printf("%s,%u,%u,%u,%u,%lu,%u,%u,%u,%s\n",
                  names[c], pins[c],
                  lowest[c], highest[c],
                  (uint16_t)(highest[c] - lowest[c]),
                  (unsigned long)(total[c] / ROUNDS),
                  maxSpread[c],
                  railLow[c], railHigh[c],
                  classifyChannel(lowest[c], highest[c],
                                  railLow[c], railHigh[c], ROUNDS));
  }

  printSelfTestHints();
}

// ============================================================
// DEMO "PEDRO" — 5 gestos ficticios FIXOS no firmware
// ============================================================
//
// Nao e Libras: cinco poses de mao devolvem as letras de PEDRO.
// Nada e ensinado nem gravado: os alvos ja estao no codigo.
//
// A decisao usa so a direcao da gravidade do MPU (accel normalizada).
// Motivo, medido no log de 25/11 (3219 amostras): os quatro flexiveis
// estao mortos, o anelar tem curso irregular e os magnetos estao
// flutuando; o MPU e o unico canal com sinal pleno (100% valido).
//
// Cada alvo e um eixo do MPU apontando para o centro da Terra:
//
//   P  az = +1   mao de costas para cima (palma para baixo)
//   E  ay = +1   dedos apontando para baixo
//   D  ay = -1   dedos apontando para cima
//   R  ax = +1   mao de lado (um polegar embaixo)
//   O  ax = -1   mao de lado (o outro polegar embaixo)
//
// Se a placa estiver montada girada, a letra aparece em outra pose. Use
// ALVO: ele mostra ao vivo o angulo para cada letra, entao voce descobre
// a pose de cada uma por tentativa, sem ensinar nada.
//
// Os cinco alvos ficam a 90 ou 180 graus um do outro: a margem e grande.

constexpr uint8_t DEMO_POSE_COUNT = 5;
constexpr uint8_t DEMO_SAMPLE_COUNT = 20;
constexpr uint32_t DEMO_SAMPLE_MS = 50;
constexpr uint32_t DEMO_TIMEOUT_MS = 6000;
constexpr uint8_t DEMO_HISTORY_MAX = 32;
constexpr uint8_t DEMO_RING_INDEX = 3;   // anelar, apenas diagnostico

// 35 graus absorvem a inclinacao natural da mao e a montagem do modulo.
// Com alvos a 90 graus, isso deixa 20 graus de zona morta entre eles.
constexpr float DEMO_MAX_ANGLE_DEG = 35.0f;
constexpr float DEMO_MIN_GAP_DEG = 10.0f;

// Amostras com aceleracao fora desta faixa sao descartadas: mao em
// movimento. Precisam sobrar DEMO_SAMPLE_COUNT amostras boas.
constexpr float DEMO_MIN_NORM_G = 0.70f;
constexpr float DEMO_MAX_NORM_G = 1.30f;
constexpr uint32_t DEMO_MAX_TOTAL_SAMPLES = DEMO_SAMPLE_COUNT * 4;

const char DEMO_LETTERS[DEMO_POSE_COUNT] = {'P', 'E', 'D', 'R', 'O'};

const float DEMO_TARGET[DEMO_POSE_COUNT][3] = {
  { 0.0f,  0.0f,  1.0f},   // P
  { 0.0f,  1.0f,  0.0f},   // E
  { 0.0f, -1.0f,  0.0f},   // D
  { 1.0f,  0.0f,  0.0f},   // R
  {-1.0f,  0.0f,  0.0f}    // O
};

bool demoActive = false;
char demoHistory[DEMO_HISTORY_MAX + 1];
uint8_t demoHistoryLen = 0;

bool targetActive = false;
uint32_t targetLastMs = 0;

const char* demoPoseHint(uint8_t index) {
  switch (index) {
    case 0: return "mao de costas para cima, palma para baixo";
    case 1: return "dedos apontando para baixo";
    case 2: return "dedos apontando para cima";
    case 3: return "mao de lado, um polegar para baixo";
    case 4: return "mao de lado, o outro polegar para baixo";
    default: return "";
  }
}

// Angulo em graus entre duas direcoes de gravidade.
float demoAngle(const float* a, const float* b) {
  const float dot = constrain(a[0] * b[0] + a[1] * b[1] + a[2] * b[2],
                              -1.0f, 1.0f);

  return acosf(dot) * (180.0f / PI);
}

// Captura DEMO_SAMPLE_COUNT amostras estaveis e devolve a direcao da
// gravidade (vetor unitario). 'ring' devolve a mediana do anelar, apenas
// para diagnostico. Amostras com |a| fora de 0,7..1,3 g sao descartadas
// (mao em movimento) - nao se classifica pose com a mao mexendo.
bool demoCapture(float* gravity, uint16_t& ring, String& error) {
  float sum[3] = {0.0f, 0.0f, 0.0f};
  uint16_t ringSamples[DEMO_SAMPLE_COUNT];
  uint8_t good = 0;
  uint32_t total = 0;
  const uint32_t startMs = millis();

  error = "";

  while (good < DEMO_SAMPLE_COUNT) {
    if ((millis() - startMs) > DEMO_TIMEOUT_MS) {
      error = "tempo esgotado";
      return false;
    }

    if (total >= DEMO_MAX_TOTAL_SAMPLES) {
      error = "mao em movimento";
      return false;
    }

    readFlexSensors();

    float ax = 0.0f;
    float ay = 0.0f;
    float az = 0.0f;
    float gx = 0.0f;
    float gy = 0.0f;
    float gz = 0.0f;

    if (!mpuAvailable || !readMotion(ax, ay, az, gx, gy, gz)) {
      error = "MPU indisponivel";
      return false;
    }

    ++total;

    const float norm = sqrtf(ax * ax + ay * ay + az * az);

    if (norm >= DEMO_MIN_NORM_G && norm <= DEMO_MAX_NORM_G) {
      sum[0] += ax;
      sum[1] += ay;
      sum[2] += az;
      ringSamples[good] = flexReading[DEMO_RING_INDEX].raw;
      ++good;
    }

    delay(DEMO_SAMPLE_MS);
    yield();
  }

  const float norm = sqrtf(sum[0] * sum[0] + sum[1] * sum[1] +
                           sum[2] * sum[2]);

  if (norm < 1e-3f) {
    error = "aceleracao nula";
    return false;
  }

  gravity[0] = sum[0] / norm;
  gravity[1] = sum[1] / norm;
  gravity[2] = sum[2] / norm;

  sortU16(ringSamples, DEMO_SAMPLE_COUNT);
  ring = ringSamples[DEMO_SAMPLE_COUNT / 2];

  return true;
}

// Compara a direcao medida com os cinco alvos fixos.
// Aceita se o melhor alvo estiver a no maximo 35 graus e se o segundo
// colocado estiver ao menos 10 graus atras.
bool demoClassify(const float* gravity, char& letter,
                  float& best, float& second) {
  float angle[DEMO_POSE_COUNT];
  int8_t bestIndex = -1;
  float bestValue = 999.0f;

  best = 0.0f;
  second = 0.0f;
  letter = '?';

  for (uint8_t i = 0; i < DEMO_POSE_COUNT; ++i) {
    angle[i] = demoAngle(gravity, DEMO_TARGET[i]);

    if (angle[i] < bestValue) {
      bestValue = angle[i];
      bestIndex = (int8_t)i;
    }
  }

  if (bestIndex < 0) {
    return false;
  }

  float secondValue = 999.0f;

  for (uint8_t i = 0; i < DEMO_POSE_COUNT; ++i) {
    if ((int8_t)i == bestIndex) continue;

    if (angle[i] < secondValue) {
      secondValue = angle[i];
    }
  }

  letter = DEMO_LETTERS[bestIndex];
  best = bestValue;
  second = secondValue;

  return (bestValue <= DEMO_MAX_ANGLE_DEG) &&
         ((secondValue - bestValue) >= DEMO_MIN_GAP_DEG);
}

void demoPrintHistory() {
  if (demoHistoryLen == 0) {
    Serial.println("LETRAS= (nenhuma)");
    return;
  }

  Serial.printf("LETRAS=%s\n", demoHistory);

  for (uint8_t i = 0; i < demoHistoryLen; ++i) {
    Serial.printf("  %u: %c\n", (unsigned)(i + 1), demoHistory[i]);
  }
}

// Lista os alvos fixos e a separacao entre eles.
void demoPrintTargets() {
  Serial.printf("ALVOS: aceitacao=%.0f graus folga=%.0f graus\n",
                DEMO_MAX_ANGLE_DEG, DEMO_MIN_GAP_DEG);
  Serial.println("letra,ax,ay,az,pose");

  for (uint8_t i = 0; i < DEMO_POSE_COUNT; ++i) {
    Serial.printf("%c,%.0f,%.0f,%.0f", DEMO_LETTERS[i],
                  DEMO_TARGET[i][0], DEMO_TARGET[i][1], DEMO_TARGET[i][2]);
    Serial.printf(", %s\n", demoPoseHint(i));
  }

  for (uint8_t i = 0; i < DEMO_POSE_COUNT; ++i) {
    for (uint8_t j = (uint8_t)(i + 1); j < DEMO_POSE_COUNT; ++j) {
      Serial.printf("# angulo(%c,%c)=%.0f graus\n",
                    DEMO_LETTERS[i], DEMO_LETTERS[j],
                    demoAngle(DEMO_TARGET[i], DEMO_TARGET[j]));
    }
  }
}

void demoStart() {
  if (collecting) {
    Serial.println("ERRO: envie PAUSE antes de DEMO.");
    return;
  }

  if (!mpuAvailable) {
    Serial.println("ERRO: MPU indisponivel; a demo depende dele.");
    return;
  }

  demoActive = true;
  demoHistoryLen = 0;
  demoHistory[0] = '\0';

  Serial.printf("# DEMO ativa: aceita ate %.0f graus de desvio do alvo.\n",
                DEMO_MAX_ANGLE_DEG);
  Serial.println("# P=palma para baixo  E=dedos para baixo  D=dedos para cima");
  Serial.println("# R=lado  O=lado (o outro polegar para baixo)");
  Serial.println("# Nao sabe a pose? Envie ALVO e veja o angulo de cada letra cair.");
  Serial.println("# Mantenha a pose e envie: GO  (20 amostras, ~1 s)");
  Serial.println("# Envie stop para encerrar e ver todas as letras.");
}

void demoCaptureAndClassify() {
  float gravity[3];
  uint16_t ring = 0;
  String error;

  Serial.println("# capturando 20 amostras (~1 s); mantenha a pose");

  if (!demoCapture(gravity, ring, error)) {
    Serial.printf("ERRO: captura falhou (%s). Nenhuma letra emitida.\n",
                  error.c_str());
    return;
  }

  char letter = '?';
  float best = 0.0f;
  float second = 0.0f;

  const bool accepted = demoClassify(gravity, letter, best, second);

  Serial.printf("g=(%.2f %.2f %.2f) anelar=%u\n",
                gravity[0], gravity[1], gravity[2], (unsigned)ring);

  if (!accepted) {
    Serial.printf("NAO_RECONHECIDO angulo=%.0f segundo=%.0f limite=%.0f "
                  "folga=%.0f\n",
                  best, second, DEMO_MAX_ANGLE_DEG, second - best);
    return;
  }

  if (demoHistoryLen < DEMO_HISTORY_MAX) {
    demoHistory[demoHistoryLen++] = letter;
    demoHistory[demoHistoryLen] = '\0';
  }

  Serial.printf("LETRA=%c angulo=%.0f segundo=%.0f\n", letter, best, second);
  Serial.printf("LETRAS=%s\n", demoHistory);
}

// ============================================================
// ALVO — mostra ao vivo o angulo para cada letra
// ============================================================
// E o "guia" para alcancar as poses: escolha a letra que quer e va
// girando a mao olhando a coluna dela ate o angulo chegar perto de 0.
// O asterisco marca a letra mais proxima; OK significa dentro do limite.
// Qualquer comando encerra o modo.

void startTargetMonitor() {
  targetActive = true;
  targetLastMs = 0;

  Serial.println("# ALVO: 5 Hz, nada e gravado. Qualquer comando encerra.");
  Serial.println("# Cada numero e o angulo, em graus, ate a pose daquela letra.");
  Serial.println("# * = mais proxima   OK = dentro do limite de aceitacao");
  Serial.println("t_ms,P,E,D,R,O,gx,gy,gz,melhor,status");
}

void stopTargetMonitor() {
  targetActive = false;
  Serial.println("# ALVO encerrado.");
}

void runTargetMonitorStep() {
  if (!targetActive) {
    return;
  }

  const uint32_t now = millis();

  if (targetLastMs != 0 && (now - targetLastMs) < 200) {
    return;
  }

  targetLastMs = now;

  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  float gx = 0.0f;
  float gy = 0.0f;
  float gz = 0.0f;

  if (!mpuAvailable || !readMotion(ax, ay, az, gx, gy, gz)) {
    Serial.println("# MPU indisponivel; ALVO encerrado.");
    targetActive = false;
    return;
  }

  float gravity[3] = {ax, ay, az};
  const float norm = sqrtf(ax * ax + ay * ay + az * az);

  if (norm > 1e-3f) {
    gravity[0] /= norm;
    gravity[1] /= norm;
    gravity[2] /= norm;
  }

  char letter = '?';
  float best = 0.0f;
  float second = 0.0f;

  const bool accepted = demoClassify(gravity, letter, best, second);

  Serial.printf("%lu", (unsigned long)now);

  for (uint8_t i = 0; i < DEMO_POSE_COUNT; ++i) {
    const float angle = demoAngle(gravity, DEMO_TARGET[i]);

    Serial.printf(",%4.0f%s", angle, (DEMO_LETTERS[i] == letter) ? "*" : " ");
  }

  Serial.printf(",%.2f,%.2f,%.2f,%c,%s\n",
                gravity[0], gravity[1], gravity[2],
                accepted ? letter : '?', accepted ? "OK" : "-");
}

// ============================================================
// ANELAR — mede o curso real do flexivel do dedo anelar
// ============================================================
// Responde "o anelar nao reduz como deveria" com numeros: le o ADC cru
// (sem filtro e sem regra de validade) por 10 s enquanto o dedo e dobrado
// e esticado. O veredito diz se o canal serve para classificacao.
void runRingTest() {
  constexpr uint32_t DURATION_MS = 10000;
  constexpr uint16_t PERIOD_MS = 50;

  uint16_t lowest = ADC_MAX_VALUE;
  uint16_t highest = 0;
  uint16_t maxSpread = 0;
  uint16_t clippedCount = 0;
  uint32_t samples = 0;
  const uint32_t startMs = millis();

  if (collecting) {
    Serial.println("ERRO: envie PAUSE antes de ANELAR.");
    return;
  }

  Serial.println("# ANELAR: dobre e estique o dedo anelar durante 10 s.");
  Serial.println("# t_ms,anelar_raw,ruido,saturado");

  while ((millis() - startMs) < DURATION_MS) {
    bool clipped = false;
    uint32_t sum = 0;
    uint16_t spread = 0;

    const uint16_t value =
        readStableADC(FLEX_PINS[DEMO_RING_INDEX], clipped, sum, spread);

    if (value < lowest) lowest = value;
    if (value > highest) highest = value;
    if (spread > maxSpread) maxSpread = spread;
    if (clipped) ++clippedCount;

    ++samples;

    Serial.printf("%lu,%u,%u,%u\n",
                  (unsigned long)(millis() - startMs), value, spread,
                  clipped ? 1U : 0U);

    delay(PERIOD_MS);
    yield();
  }

  const uint16_t span = (uint16_t)(highest - lowest);

  Serial.printf("# ANELAR: amostras=%lu min=%u max=%u curso=%u ruido_max=%u "
                "saturadas=%u\n",
                (unsigned long)samples, lowest, highest, span, maxSpread,
                (unsigned)clippedCount);

  if (clippedCount > (samples / 2)) {
    Serial.println("# VEREDITO: canal preso no trilho. Divisor em 5 V, falta de");
    Serial.println("# GND ou sensor em curto. Use SCAN/SELFTEST para localizar.");
  } else if (span < 100) {
    Serial.println("# VEREDITO: sem curso util - o sensor nao responde ao dobramento.");
    Serial.println("# Causa provavel: flex rompido, solda, GPIO errado. Use SCAN.");
  } else if (span < 400) {
    Serial.println("# VEREDITO: curso curto. Da para usar como dimensao extra,");
    Serial.println("# mas com pouca margem: prefira a demo por orientacao.");
  } else {
    Serial.println("# VEREDITO: curso bom. O anelar pode entrar na classificacao");
    Serial.println("# como quarta dimensao (fale com o autor do firmware).");
  }
}

// ============================================================
// STATUS AND COMMANDS
// ============================================================

// Explica em uma palavra por que um canal flex nao esta utilizavel.
const char* flexChannelState(uint8_t index) {
  if (flexReading[index].stuck) return "travado";
  if (flexReading[index].saturatedSamples >= CHANNEL_FAULT_SAMPLES) {
    return flexReading[index].raw >= (ADC_MAX_VALUE / 2) ? "preso_em_4095"
                                                         : "preso_em_0";
  }
  if (flexReading[index].noisySamples >= CHANNEL_FAULT_SAMPLES) {
    return "pino_flutuante";
  }
  if (!flexReading[index].valid) return "invalido";
  if (!calibration[index].valid) return "sem_calibracao";
  return "ok";
}

void printStatus() {
  Serial.println("STATUS");
  Serial.printf("collecting=%u\n", collecting ? 1 : 0);
  Serial.printf("mpu_available=%u\n", mpuAvailable ? 1 : 0);
  Serial.printf("mpu_address=0x%02X\n", mpuAddress);
  Serial.printf("mpu_who_am_i=0x%02X\n", mpuWhoAmI);
  Serial.printf("gyro_offset_valid=%u\n", gyroOffsetValid ? 1 : 0);
  Serial.printf("gyro_offset_dps=%.3f,%.3f,%.3f\n",
                gyroOffset[0], gyroOffset[1], gyroOffset[2]);
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
  Serial.printf("acquisition_us=%lu (tempo dos 7 canais por amostra)\n",
                (unsigned long)lastAcquisitionUs);

  for (uint8_t i = 0; i < FLEX_COUNT; ++i) {
    Serial.printf(
        "%s: gpio=%u raw=%u valid=%u calibrated=%u stuck=%u sat=%u "
        "ruido=%u cal=%ld..%ld estado=%s\n",
        FLEX_NAMES[i],
        FLEX_PINS[i],
        flexReading[i].raw,
        flexReading[i].valid ? 1 : 0,
        calibration[i].valid ? 1 : 0,
        flexReading[i].stuck ? 1 : 0,
        flexReading[i].saturated ? 1 : 0,
        flexReading[i].spread,
        (long)calibration[i].openValue,
        (long)calibration[i].closedValue,
        flexChannelState(i));
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
  Serial.println("CALIBRATE_GYRO (keep the glove completely still)");
  Serial.println("DIAG");
  Serial.println("I2C (scan the I2C bus)");
  Serial.println("MPU (re-initialize the MPU-6050 with verbose log)");
  Serial.println("WINDOW (dump the latest 40-sample matrix; pause collection first)");
  Serial.println("GAPS (show the 3 configured finger-gap sensors)");
  Serial.println("CALIBRATE_GAP <gap> TOGETHER|APART");
  Serial.println("SCAN (raw ADC of every candidate GPIO: reveals the real wiring)");
  Serial.println("SELFTEST (min/max/noise of the 8 channels: code or hardware?)");
  Serial.println("DEMO (liga a demo PEDRO: orientacao da mao -> letra)");
  Serial.println("GO (na DEMO: captura 20 amostras e imprime a letra)");
  Serial.println("ALVO (ao vivo: angulo de cada letra, para acertar a pose)");
  Serial.println("GESTOS (lista os 5 alvos fixos e a separacao em graus)");
  Serial.println("ANELAR (mede o curso real do flexivel do anelar por 10 s)");
  Serial.println("HELP");
  Serial.println("Finger: polegar, indicador, medio, anelar, minimo");
  Serial.println("Tip: keep the finger still during CALIBRATE (~0.2 s capture)");
  Serial.println("Tip: while collecting, only PAUSE/STOP are accepted");
  Serial.println("Tip: commands are case-insensitive (START/start/Start)");
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
  // Aceita maiusculas e minusculas em QUALQUER comando (antes, apenas
  // START/PAUSE/STOP ignoravam o caso e CALIBRATE/CONFIG falhavam em
  // minusculas). Os nomes de dedo/gap sao normalizados nos helpers.
  command.toUpperCase();

  if (command.isEmpty()) {
    return;
  }

  // Qualquer comando encerra um SCAN ou um ALVO em andamento e e processado
  // em seguida, para que "ALVO" seguido de "DEMO" nao fique preso.
  if (scanActive) {
    stopScan();
  }

  if (targetActive) {
    stopTargetMonitor();
  }

  // Keep the CSV stream free of diagnostic messages.
  // Only START begins a CSV session; PAUSE/STOP end it.
  if (collecting) {
    if (command.equalsIgnoreCase("PAUSE") ||
        command.equalsIgnoreCase("STOP")) {
      collecting = false;
      Serial.println("# collection_stopped");
    } else {
      // Silencio aqui e o que mais confunde no uso real: o comando
      // simplesmente nao faz nada. Avisa explicitamente.
      Serial.printf("# coleta ativa: comando '%s' ignorado. "
                    "Envie PAUSE antes de corrigir/calibrar/ver status.\n",
                    command.c_str());
    }
    return;
  }

  if (command.equalsIgnoreCase("START")) {
    if (demoActive) {
      Serial.println("ERRO: demo ativa. Envie stop antes de START.");
      return;
    }

    ++sessionId;
    sampleId = 0;
    sensorWindow.clear();
    resetFilters();
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

    if (demoActive) {
      demoActive = false;
      demoPrintHistory();
    }

    Serial.println("OK: collection stopped");
    return;
  }

  if (command.equalsIgnoreCase("DEMO")) {
    demoStart();
    return;
  }

  if (command.equalsIgnoreCase("ALVO") ||
      command.equalsIgnoreCase("ALVOS")) {
    startTargetMonitor();
    return;
  }

  if (command.equalsIgnoreCase("ANELAR")) {
    runRingTest();
    return;
  }

  if (command.equalsIgnoreCase("GO") ||
      command.equalsIgnoreCase("CAPTURA")) {
    if (!demoActive) {
      Serial.println("ERRO: envie DEMO primeiro.");
      return;
    }

    demoCaptureAndClassify();
    return;
  }

  if (command.equalsIgnoreCase("GESTOS")) {
    demoPrintTargets();
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

  if (command.equalsIgnoreCase("SCAN") ||
      command.equalsIgnoreCase("PINS")) {
    startScan();
    return;
  }

  if (command.equalsIgnoreCase("SELFTEST") ||
      command.equalsIgnoreCase("TEST")) {
    runSelfTest();
    return;
  }

  if (command.equalsIgnoreCase("MPU")) {
    mpuConsecutiveFailures = 0;
    mpuAvailable = initializeMPU(true);
    Serial.printf("mpu_available=%u\n", mpuAvailable ? 1 : 0);
    return;
  }

  if (command.equalsIgnoreCase("CALIBRATE_GYRO")) {
    Serial.println("# calibrating gyro offset, keep the glove still...");

    if (captureGyroCalibration()) {
      Serial.printf("OK: gyro offset dps x=%.3f y=%.3f z=%.3f\n",
                    gyroOffset[0], gyroOffset[1], gyroOffset[2]);
    } else {
      Serial.println("ERROR: gyro calibration failed "
                     "(MPU unavailable or glove moved)");
    }
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
    Serial.printf("adc_samples_per_read=%u (+1 discarded)\n", ADC_SAMPLES);
    Serial.printf("filter_alpha_flex=%.2f gap=%.2f\n",
                  FLEX_FILTER_ALPHA, GAP_FILTER_ALPHA);
    Serial.printf("calibration_samples=%u trim=%u max_spread=%u\n",
                  CALIBRATION_SAMPLES, CALIBRATION_TRIM,
                  CALIBRATION_MAX_SPREAD);
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

      // A aquisicao dos 7 canais custa ~10-12 ms por amostra. Pedir um
      // intervalo menor que isso nao aumenta a taxa: apenas enche a
      // contagem de late_samples. Avisa em vez de aceitar em silencio.
      const uint32_t acquisitionMs =
          (lastAcquisitionUs + 999) / 1000;

      if ((uint32_t)value < acquisitionMs) {
        Serial.printf("# aviso: %ld ms e menor que o custo de aquisicao "
                      "(%lu ms). Nao sera possivel cumprir esse intervalo.\n",
                      value, (unsigned long)acquisitionMs);
      }
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

    const CaptureStatus status =
        captureCalibration(index, position == "OPEN");

    if (status == CAPTURE_OK) {
      Serial.printf("OK: %s %s reference=%ld calibrated=%u\n",
          FLEX_NAMES[index],
          position.c_str(),
          (long)(position == "OPEN"
              ? calibration[index].openValue
              : calibration[index].closedValue),
          calibration[index].valid ? 1 : 0);
    } else {
      Serial.printf("ERRO: calibracao de %s %s falhou: %s\n",
                    FLEX_NAMES[index], position.c_str(),
                    captureStatusText(status));
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

    const CaptureStatus status =
        captureGapCalibration(index, position == "TOGETHER");

    if (status == CAPTURE_OK) {
      Serial.printf("OK: gap_%s %s reference=%ld calibrated=%u\n",
          GAP_NAMES[index],
          position.c_str(),
          (long)(position == "TOGETHER"
              ? gapCalibration[index].togetherValue
              : gapCalibration[index].apartValue),
          gapCalibration[index].valid ? 1 : 0);
    } else {
      Serial.printf("ERRO: calibracao de gap_%s %s falhou: %s\n",
                    GAP_NAMES[index], position.c_str(),
                    captureStatusText(status));
    }
    return;
  }

  Serial.println("ERROR: unknown command; send HELP");
}

void pollSerialCommands() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();

    if (c == '\n' || c == '\r') {
      if (!commandBuffer.isEmpty()) {
        processCommand(commandBuffer);
        commandBuffer = "";
      }
      // Linha vazia NAO dispara nada de proposito: no Monitor Serial a
      // tecla ENTER pode chegar como "\r", "\n" ou "\r\n", e um gatilho
      // por linha vazia dispararia duas capturas. A demo usa GO.
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
    flexReading[i].spread = 0;
    flexReading[i].previousSum = UINT32_MAX;
    flexReading[i].valid = false;
    flexReading[i].stuck = false;
    flexReading[i].saturated = false;
    flexReading[i].unchangedSamples = 0;
    flexReading[i].saturatedSamples = 0;
    flexReading[i].noisySamples = 0;

    calibration[i] = {-1, -1, false};
  }

  // Set ADC resolution for the classic ESP32 Arduino core.
  analogReadResolution(12);

  // Limit ADC input range to the ESP32's supported range.
  // Actual voltage limits depend on the exact ESP32 variant.
  for (uint8_t i = 0; i < FLEX_COUNT; ++i) {
    analogSetPinAttenuation(FLEX_PINS[i], ADC_11db);
  }
  setupGapSensors();

  Wire.begin(MPU_SDA, MPU_SCL);
  Wire.setTimeOut(20);

  loadCalibration();
  loadGapCalibration();
  loadGyroCalibration();

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
  Serial.printf("# Gyro offset calibrated=%u (use CALIBRATE_GYRO)\n",
                gyroOffsetValid ? 1 : 0);
  Serial.println("# Sampling default: 20 Hz (50 ms)");
  Serial.println("# ADC: descarte + media de 8 leituras + filtro EMA");
  Serial.println("# A sensor is flagged stuck after 5 s with exactly unchanged ADC readings.");
  Serial.println("# Send HELP for commands");

  // Mapa de pinos em uso: imprime na inicializacao para conferir a fiacao
  // sem precisar abrir o codigo no meio do teste.
  Serial.print("# mapa flex: ");
  for (uint8_t i = 0; i < FLEX_COUNT; ++i) {
    Serial.printf("%s=%u ", FLEX_NAMES[i], FLEX_PINS[i]);
  }
  Serial.println();

  Serial.print("# mapa gap:  ");
  for (uint8_t i = 0; i < GAP_COUNT; ++i) {
    Serial.printf("%s=%u%s ", GAP_NAMES[i], GAP_PINS[i],
                  GAP_ENABLED[i] ? "" : "(desativado)");
  }
  Serial.println();

  Serial.println("# Se as leituras flex nao fizerem sentido, rode SCAN e SELFTEST:");
  Serial.println("# eles distinguem problema de codigo (pino errado) de problema");
  Serial.println("# de hardware (sensor solto, divisor em 5 V, falta de GND).");

  Serial.println("# Demo PEDRO: alvos fixos no codigo. Envie GESTOS para ver as");
  Serial.println("# poses, ALVO para o angulo ao vivo, DEMO e depois GO para jogar.");
}

void loop() {
  pollSerialCommands();

  uint32_t now = millis();

  // SCAN e ALVO sao modos de diagnostico: enquanto ativos, nao coletam e
  // nao tentam reanimar o MPU, apenas mostram numeros ao vivo.
  if (scanActive) {
    runScan();
    return;
  }

  if (targetActive) {
    runTargetMonitorStep();
    return;
  }

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

  const uint32_t acquisitionStartUs = micros();

  readFlexSensors();
  readGapSensors();

  lastAcquisitionUs = micros() - acquisitionStartUs;

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