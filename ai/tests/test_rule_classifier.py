import numpy as np
import pytest

from libras_ai.experimental.classifier import classify
from libras_ai.experimental.gestures import GESTURE_PROFILES


@pytest.mark.parametrize("label", ["P", "E", "D", "R", "O"])
def test_each_exact_profile_is_classified(label):
    result = classify(GESTURE_PROFILES[label])
    assert result.accepted
    assert result.label == label
    assert result.distance == pytest.approx(0.0)


def test_noisy_profile_can_still_be_classified():
    values = np.asarray(GESTURE_PROFILES["P"]) + np.array([0.03, -0.02, 0.01, 0.0, -0.01])
    result = classify(values)
    assert result.accepted
    assert result.label == "P"


def test_rejects_vector_with_wrong_size():
    with pytest.raises(ValueError, match="cinco"):
        classify([0.1, 0.2, 0.3])


@pytest.mark.parametrize("values", [[0, 0, 0, 0, 1.1], [0, 0, 0, float("nan"), 0]])
def test_rejects_invalid_sensor_values(values):
    with pytest.raises(ValueError):
        classify(values)


def test_rejects_reading_far_from_all_profiles():
    result = classify([0.5, 0.5, 0.5, 0.5, 0.5])
    assert not result.accepted
    assert result.label is None
    assert result.reason == "fora_dos_perfis"
