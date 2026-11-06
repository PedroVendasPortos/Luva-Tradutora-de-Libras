#pragma once

#include <Arduino.h>
#include <math.h>
#include <stdint.h>

// Janela temporal para a entrada futura do modelo.
// 40 amostras a 20 Hz representam aproximadamente 2 segundos.
// Ordem das 15 features:
// 0..4  sensores flexíveis (polegar, indicador, médio, anelar, mínimo)
// 5..7  acelerômetro X/Y/Z (g, normalizado por 2 g)
// 8..10 giroscópio X/Y/Z (graus/s, normalizado por 250 graus/s)
// 11..14 sensores de distância entre dedos (normalizados 0..1)
//
// Esta dimensão precisa permanecer sincronizada com:
// - addCurrentSampleToWindow() no firmware;
// - a preparação do conjunto de treinamento;
// - a forma de entrada do modelo TensorFlow Lite Micro.

constexpr uint16_t MODEL_WINDOW_SAMPLES = 40;
constexpr uint8_t MODEL_FEATURE_COUNT = 15;

class SensorWindow {
 public:
  SensorWindow() { clear(); }

  void clear() {
    writeIndex_ = 0;
    sampleCount_ = 0;

    for (uint16_t row = 0; row < MODEL_WINDOW_SAMPLES; ++row) {
      rowValid_[row] = false;
      for (uint8_t col = 0; col < MODEL_FEATURE_COUNT; ++col) {
        values_[row][col] = 0.0f;
      }
    }
  }

  // Adiciona uma amostra. Quando a janela está cheia,
  // a amostra mais antiga é sobrescrita.
  void push(const float* features, bool valid) {
    if (features == nullptr) {
      valid = false;
    }

    for (uint8_t col = 0; col < MODEL_FEATURE_COUNT; ++col) {
      const float value = (features != nullptr) ? features[col] : 0.0f;
      values_[writeIndex_][col] = isfinite(value) ? value : 0.0f;
    }

    rowValid_[writeIndex_] = valid;
    writeIndex_ = (writeIndex_ + 1) % MODEL_WINDOW_SAMPLES;

    if (sampleCount_ < MODEL_WINDOW_SAMPLES) {
      ++sampleCount_;
    }
  }

  bool isFull() const {
    return sampleCount_ == MODEL_WINDOW_SAMPLES;
  }

  // Só considera a janela utilizável quando todas as 40 linhas
  // foram preenchidas e cada leitura foi validada pelo firmware.
  bool isValid() const {
    if (!isFull()) return false;

    for (uint16_t row = 0; row < MODEL_WINDOW_SAMPLES; ++row) {
      if (!rowValid_[row]) return false;
    }
    return true;
  }

  uint16_t size() const {
    return sampleCount_;
  }

  // Copia a janela em ordem cronológica: mais antiga -> mais recente.
  // Retorna false se a janela ainda não estiver cheia ou tiver linha inválida.
  bool copyTo(float destination[MODEL_WINDOW_SAMPLES][MODEL_FEATURE_COUNT]) const {
    if (destination == nullptr || !isFull()) return false;

    for (uint16_t row = 0; row < MODEL_WINDOW_SAMPLES; ++row) {
      const uint16_t source = (writeIndex_ + row) % MODEL_WINDOW_SAMPLES;
      for (uint8_t col = 0; col < MODEL_FEATURE_COUNT; ++col) {
        destination[row][col] = values_[source][col];
      }
    }

    return isValid();
  }

  // Lê uma linha por índice cronológico (0 = mais antiga).
  bool getRow(uint16_t chronologicalRow,
              float destination[MODEL_FEATURE_COUNT],
              bool& valid) const {
    if (destination == nullptr || chronologicalRow >= sampleCount_) {
      valid = false;
      return false;
    }

    const uint16_t oldestIndex = isFull() ? writeIndex_ : 0;
    const uint16_t source = (oldestIndex + chronologicalRow) % MODEL_WINDOW_SAMPLES;

    for (uint8_t col = 0; col < MODEL_FEATURE_COUNT; ++col) {
      destination[col] = values_[source][col];
    }

    valid = rowValid_[source];
    return true;
  }

 private:
  float values_[MODEL_WINDOW_SAMPLES][MODEL_FEATURE_COUNT];
  bool rowValid_[MODEL_WINDOW_SAMPLES];
  uint16_t writeIndex_ = 0;
  uint16_t sampleCount_ = 0;
};
