#pragma once
#include <Arduino.h>
#include "SensorWindow.h"

// The model has not yet been trained/exported into this repository.
// Keep the inference boundary explicit: never fabricate a gesture result.
struct ModelPrediction {
  int classIndex;
  float confidence;
};

inline bool modelIsAvailable() {
  return false;
}

// Input contract for the future TensorFlow Lite Micro adapter:
// [40 time steps][15 features], oldest-to-newest.
// Feature order:
// 0..4 flex thumb/index/middle/ring/pinky (0..1)
// 5..7 accelerometer X/Y/Z in g divided by 2
// 8..10 gyroscope X/Y/Z in dps divided by 250
// 11..14 adjacent-finger magnetic gaps (0..1)
//
// Replace this stub after adding the trained .tflite model, generated
// model_data header, operator resolver, tensor arena and class labels.
// Returns false until a real model is linked.
inline bool runModelInference(
    const float input[MODEL_WINDOW_SAMPLES][MODEL_FEATURE_COUNT],
    ModelPrediction& prediction) {
  (void)input;
  (void)prediction;
  return false;
}
