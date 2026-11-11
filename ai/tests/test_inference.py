import pytest

from libras_ai.inference import interpret_probabilities


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
