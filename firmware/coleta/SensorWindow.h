#pragma once

#include <Arduino.h>
#include <math.h>
#include <stdint.h>

// Janela temporal: 40 amostras a 20 Hz (~2 segundos).
// Ordem das 14 features:
// 0..4: flexiveis (polegar, indicador, medio, anelar, minimo)
// 5..7: acelerometro X/Y/Z.
// 8..10: giroscopio X/Y/Z.
// 11..13: gaps indicador-medio, medio-anelar e anelar-minimo.
// O gap medio-anelar fica reservado como zero enquanto o sensor estiver quebrado.
// A MLP estatica experimental usa somente as cinco primeiras features.

constexpr uint16_t MODEL_WINDOW_SAMPLES = 40;
constexpr uint8_t MODEL_FEATURE_COUNT = 14;

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

  void push(const float* features, bool valid) {
    if (features == nullptr) valid = false;

    for (uint8_t col = 0; col < MODEL_FEATURE_COUNT; ++col) {
      const float value = (features != nullptr) ? features[col] : 0.0f;
      if (!isfinite(value)) {
        values_[writeIndex_][col] = 0.0f;
        valid = false;
      } else {
        values_[writeIndex_][col] = value;
      }
    }

    rowValid_[writeIndex_] = valid;
    writeIndex_ = (writeIndex_ + 1) % MODEL_WINDOW_SAMPLES;
    if (sampleCount_ < MODEL_WINDOW_SAMPLES) ++sampleCount_;
  }

  bool isFull() const {
    return sampleCount_ == MODEL_WINDOW_SAMPLES;
  }

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

  // chronologicalRow=0 sempre representa a amostra mais antiga.
  bool getRow(uint16_t chronologicalRow,
              float destination[MODEL_FEATURE_COUNT],
              bool& valid) const {
    if (destination == nullptr || chronologicalRow >= sampleCount_) {
      valid = false;
      if (destination != nullptr) {
        for (uint8_t col = 0; col < MODEL_FEATURE_COUNT; ++col) {
          destination[col] = 0.0f;
        }
      }
      return false;
    }

    const uint16_t oldest = isFull() ? writeIndex_ : 0;
    const uint16_t source = (oldest + chronologicalRow) % MODEL_WINDOW_SAMPLES;
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