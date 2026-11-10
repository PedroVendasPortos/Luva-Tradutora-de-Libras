#pragma once
#include <Arduino.h>
#include <math.h>

// One gesture window: 2 seconds at 20 Hz, 15 normalized features.
constexpr uint16_t MODEL_WINDOW_SAMPLES = 40;
constexpr uint8_t MODEL_FEATURE_COUNT = 15;

class SensorWindow {
 public:
  SensorWindow() { clear(); }

  void clear() {
    writeIndex = 0;
    sampleCount = 0;
    for (uint16_t r = 0; r < MODEL_WINDOW_SAMPLES; ++r) {
      rowValid[r] = false;
      for (uint8_t c = 0; c < MODEL_FEATURE_COUNT; ++c) {
        values[r][c] = 0.0f;
      }
    }
  }

  // A full window is frozen until copied/processed and explicitly cleared.
  bool push(const float* features, bool valid) {
    if (isFull()) return false;
    for (uint8_t c = 0; c < MODEL_FEATURE_COUNT; ++c) {
      values[writeIndex][c] = isfinite(features[c]) ? features[c] : 0.0f;
    }
    rowValid[writeIndex] = valid;
    writeIndex = (writeIndex + 1) % MODEL_WINDOW_SAMPLES;
    ++sampleCount;
    return true;
  }

  bool isFull() const { return sampleCount == MODEL_WINDOW_SAMPLES; }
  uint16_t size() const { return sampleCount; }

  bool isValid() const {
    if (!isFull()) return false;
    for (uint16_t r = 0; r < MODEL_WINDOW_SAMPLES; ++r) {
      if (!rowValid[r]) return false;
    }
    return true;
  }

  // Copy in chronological order, oldest sample first.
  bool copyTo(float destination[MODEL_WINDOW_SAMPLES][MODEL_FEATURE_COUNT]) const {
    if (!isFull()) return false;
    for (uint16_t r = 0; r < MODEL_WINDOW_SAMPLES; ++r) {
      for (uint8_t c = 0; c < MODEL_FEATURE_COUNT; ++c) {
        destination[r][c] = values[r][c];
      }
    }
    return isValid();
  }

  bool getRow(uint16_t row, float destination[MODEL_FEATURE_COUNT],
              bool& valid) const {
    if (row >= sampleCount) return false;
    for (uint8_t c = 0; c < MODEL_FEATURE_COUNT; ++c) {
      destination[c] = values[row][c];
    }
    valid = rowValid[row];
    return true;
  }

 private:
  float values[MODEL_WINDOW_SAMPLES][MODEL_FEATURE_COUNT];
  bool rowValid[MODEL_WINDOW_SAMPLES];
  uint16_t writeIndex = 0;
  uint16_t sampleCount = 0;
};
