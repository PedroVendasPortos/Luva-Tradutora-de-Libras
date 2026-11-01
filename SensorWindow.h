#pragma once

#include <Arduino.h>

// ============================================================
// SENSOR WINDOW - rolling buffer for the future ML model
// ============================================================
// Default acquisition: 20 Hz.
// Window duration: 2 seconds.
// Shape: [40 time steps][11 sensor features].
// The ring buffer always keeps the newest samples.

constexpr uint16_t MODEL_WINDOW_SAMPLES = 40;
constexpr uint8_t MODEL_FEATURE_COUNT = 11;

class SensorWindow {
 public:
  SensorWindow() { clear(); }

  void clear() {
    writeIndex = 0;
    sampleCount = 0;
    for (uint16_t row = 0; row < MODEL_WINDOW_SAMPLES; ++row) {
      rowValid[row] = false;
      for (uint8_t col = 0; col < MODEL_FEATURE_COUNT; ++col) {
        values[row][col] = 0.0f;
      }
    }
  }

  void push(const float* features, bool valid) {
    for (uint8_t col = 0; col < MODEL_FEATURE_COUNT; ++col) {
      values[writeIndex][col] = isfinite(features[col]) ? features[col] : 0.0f;
    }

    rowValid[writeIndex] = valid;
    writeIndex = (writeIndex + 1) % MODEL_WINDOW_SAMPLES;

    if (sampleCount < MODEL_WINDOW_SAMPLES) {
      ++sampleCount;
    }
  }

  bool isFull() const {
    return sampleCount == MODEL_WINDOW_SAMPLES;
  }

  bool isValid() const {
    if (!isFull()) return false;

    for (uint16_t i = 0; i < MODEL_WINDOW_SAMPLES; ++i) {
      uint16_t index = (writeIndex + i) % MODEL_WINDOW_SAMPLES;
      if (!rowValid[index]) return false;
    }
    return true;
  }

  uint16_t size() const {
    return sampleCount;
  }

  // Copies rows in chronological order (oldest to newest).
  // Returns true only when the window is full and every row is valid.
  bool copyTo(float destination[MODEL_WINDOW_SAMPLES][MODEL_FEATURE_COUNT]) const {
    if (!isFull()) return false;

    for (uint16_t row = 0; row < MODEL_WINDOW_SAMPLES; ++row) {
      uint16_t sourceIndex = (writeIndex + row) % MODEL_WINDOW_SAMPLES;
      for (uint8_t col = 0; col < MODEL_FEATURE_COUNT; ++col) {
        destination[row][col] = values[sourceIndex][col];
      }
    }

    return isValid();
  }

  bool getRow(uint16_t chronologicalRow,
              float destination[MODEL_FEATURE_COUNT],
              bool& valid) const {
    if (chronologicalRow >= sampleCount) return false;

    uint16_t oldestIndex =
        isFull() ? writeIndex : 0;
    uint16_t sourceIndex =
        (oldestIndex + chronologicalRow) % MODEL_WINDOW_SAMPLES;

    for (uint8_t col = 0; col < MODEL_FEATURE_COUNT; ++col) {
      destination[col] = values[sourceIndex][col];
    }

    valid = rowValid[sourceIndex];
    return true;
  }

 private:
  float values[MODEL_WINDOW_SAMPLES][MODEL_FEATURE_COUNT];
  bool rowValid[MODEL_WINDOW_SAMPLES];
  uint16_t writeIndex;
  uint16_t sampleCount;
};
