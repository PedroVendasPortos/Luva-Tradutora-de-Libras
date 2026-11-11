import numpy as np
import pandas as pd
import pytest

from libras_ai.preprocessing import prepare_features, prepare_labels, validate_dataset
from libras_ai.schema import FEATURE_COLUMNS


def make_frame():
    return pd.DataFrame(
        [
            [0.1, 0.2, 0.3, 0.4, 0.5, "P", "p01", "s01", "t001"],
            [0.5, 0.4, 0.3, 0.2, 0.1, "E", "p01", "s01", "t002"],
        ],
        columns=list(FEATURE_COLUMNS)
        + ["label", "participant_id", "session_id", "trial_id"],
    )


def test_prepare_features_has_expected_shape_and_order():
    frame = make_frame()
    features = prepare_features(frame)
    assert features.shape == (2, 5)
    assert features.dtype == np.float32
    np.testing.assert_allclose(features[0], [0.1, 0.2, 0.3, 0.4, 0.5])


def test_prepare_labels_uses_fixed_class_order():
    labels = prepare_labels(make_frame())
    np.testing.assert_array_equal(labels, [0, 1])


def test_rejects_values_outside_unit_interval():
    frame = make_frame()
    frame.loc[0, "flex_thumb"] = 1.2
    with pytest.raises(ValueError, match="intervalo"):
        validate_dataset(frame)


def test_rejects_unknown_class():
    frame = make_frame()
    frame.loc[0, "label"] = "Z"
    with pytest.raises(ValueError, match="Rótulos desconhecidos"):
        validate_dataset(frame)


def test_rejects_duplicate_trial_id():
    frame = make_frame()
    frame.loc[1, "trial_id"] = "t001"
    with pytest.raises(ValueError, match="trial_id"):
        validate_dataset(frame)
