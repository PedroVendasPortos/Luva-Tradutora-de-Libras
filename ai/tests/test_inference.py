import numpy as np
import pytest

from libras_ai.inference import interpret_probabilities, predict_gesture


def test_returns_class_when_confident():
    label, confidence = interpret_probabilities([0.8, 0.05, 0.05, 0.05, 0.05])
    assert label == "P"
    assert confidence == pytest.approx(0.8)


def test_rejects_low_confidence_prediction():
    label, confidence = interpret_probabilities([0.25, 0.2, 0.2, 0.2, 0.15])
    assert label is None
    assert confidence == pytest.approx(0.25)


def test_rejects_wrong_probability_vector():
    with pytest.raises(ValueError):
        interpret_probabilities([0.5, 0.5])


class FakeModel:
    def predict(self, values, verbose=0):
        assert values.shape == (1, 5)
        return np.array([[0.8, 0.05, 0.05, 0.05, 0.05]], dtype=np.float32)


def test_predict_gesture_accepts_five_calibrated_values():
    label, confidence = predict_gesture(FakeModel(), [0.1, 0.2, 0.3, 0.4, 0.5])
    assert label == "P"
    assert confidence == pytest.approx(0.8)


def test_predict_gesture_rejects_out_of_range_sensor_values():
    with pytest.raises(ValueError, match="intervalo"):
        predict_gesture(FakeModel(), [0.1, 0.2, 0.3, 0.4, 1.5])
