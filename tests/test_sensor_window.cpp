#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include "SensorWindow.h"

static void makeSample(float (&sample)[MODEL_FEATURE_COUNT], float value) {
  for (uint8_t i = 0; i < MODEL_FEATURE_COUNT; ++i) sample[i] = value + i;
}
static void testStartsEmpty() {
  SensorWindow window;
  assert(window.size() == 0 && !window.isFull() && !window.isValid());
}
static void testFillsAndPreservesOrder() {
  SensorWindow window; float sample[MODEL_FEATURE_COUNT];
  for (uint16_t row=0; row<MODEL_WINDOW_SAMPLES; ++row) {
    makeSample(sample, static_cast<float>(row*100)); window.push(sample,true);
  }
  assert(window.isFull() && window.isValid() && window.size()==MODEL_WINDOW_SAMPLES);
  float output[MODEL_WINDOW_SAMPLES][MODEL_FEATURE_COUNT]; assert(window.copyTo(output));
  for (uint16_t row=0; row<MODEL_WINDOW_SAMPLES; ++row) {
    assert(output[row][0]==static_cast<float>(row*100));
    assert(output[row][MODEL_FEATURE_COUNT-1]==static_cast<float>(row*100+MODEL_FEATURE_COUNT-1));
  }
}
static void testRingBufferWrapsChronologically() {
  SensorWindow window; float sample[MODEL_FEATURE_COUNT];
  for (uint16_t row=0; row<MODEL_WINDOW_SAMPLES+5; ++row) {
    makeSample(sample, static_cast<float>(row)); window.push(sample,true);
  }
  float output[MODEL_WINDOW_SAMPLES][MODEL_FEATURE_COUNT]; assert(window.copyTo(output));
  for (uint16_t row=0; row<MODEL_WINDOW_SAMPLES; ++row) assert(output[row][0]==static_cast<float>(row+5));
}
static void testInvalidRowInvalidatesWindow() {
  SensorWindow window; float sample[MODEL_FEATURE_COUNT]; makeSample(sample,1.0f);
  for (uint16_t row=0; row<MODEL_WINDOW_SAMPLES; ++row) window.push(sample,row!=7);
  assert(window.isFull() && !window.isValid());
  float output[MODEL_WINDOW_SAMPLES][MODEL_FEATURE_COUNT]; assert(!window.copyTo(output));
  bool valid=true; float rowData[MODEL_FEATURE_COUNT]; assert(window.getRow(7,rowData,valid)); assert(!valid);
}
static void testNonFiniteValueInvalidatesRow() {
  SensorWindow window; float sample[MODEL_FEATURE_COUNT]; makeSample(sample,0.0f); sample[3]=NAN;
  window.push(sample,true); bool valid=true; float rowData[MODEL_FEATURE_COUNT];
  assert(window.getRow(0,rowData,valid)); assert(!valid && rowData[3]==0.0f);
}
static void testNullSampleIsInvalid() {
  SensorWindow window; window.push(nullptr,true); bool valid=true; float rowData[MODEL_FEATURE_COUNT];
  assert(window.getRow(0,rowData,valid)); assert(!valid);
  for(float value:rowData) assert(value==0.0f);
}
static void testClearResetsState() {
  SensorWindow window; float sample[MODEL_FEATURE_COUNT]; makeSample(sample,10.0f);
  for(uint16_t row=0; row<MODEL_WINDOW_SAMPLES; ++row) window.push(sample,true);
  window.clear(); assert(window.size()==0 && !window.isFull() && !window.isValid());
}
int main() {
  testStartsEmpty(); testFillsAndPreservesOrder(); testRingBufferWrapsChronologically();
  testInvalidRowInvalidatesWindow(); testNonFiniteValueInvalidatesRow();
  testNullSampleIsInvalid(); testClearResetsState();
  std::cout << "SensorWindow: 7 test groups passed\n"; return 0;
}
