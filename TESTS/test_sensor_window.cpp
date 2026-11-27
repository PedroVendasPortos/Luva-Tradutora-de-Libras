#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include "SensorWindow.h"

static void fill(float (&sample)[MODEL_FEATURE_COUNT], float base) {
  for (uint8_t i = 0; i < MODEL_FEATURE_COUNT; ++i) sample[i] = base + i;
}

int main() {
  SensorWindow window;
  assert(window.size() == 0 && !window.isFull() && !window.isValid());

  float sample[MODEL_FEATURE_COUNT];
  for (uint16_t i = 0; i < MODEL_WINDOW_SAMPLES; ++i) {
    fill(sample, static_cast<float>(i));
    window.push(sample, true);
  }
  assert(window.isFull() && window.isValid());

  bool valid = false;
  float row[MODEL_FEATURE_COUNT];
  assert(window.getRow(0, row, valid) && valid);
  assert(row[0] == 0.0f);
  assert(window.getRow(39, row, valid) && valid);
  assert(row[0] == 39.0f);

  fill(sample, 40.0f);
  window.push(sample, true);
  assert(window.getRow(0, row, valid) && valid);
  assert(row[0] == 1.0f);

  window.clear();
  assert(window.size() == 0 && !window.isFull() && !window.isValid());

  sample[3] = NAN;
  window.push(sample, true);
  assert(window.getRow(0, row, valid) && !valid);
  assert(row[3] == 0.0f);

  window.clear();
  window.push(nullptr, true);
  assert(window.getRow(0, row, valid) && !valid);
  for (float value : row) assert(value == 0.0f);

  std::cout << "SensorWindow 40x14: tests passed\n";
  return 0;
}
