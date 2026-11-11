import numpy as np
import pytest

from libras_ai.evaluation import evaluate_predictions


def test_evaluation_returns_fixed_five_by_five_confusion_matrix():
    result = evaluate_predictions(
        np.array([0, 1, 2]),
        np.array([0, 1, 1]),
    )
    assert result["accuracy"] == pytest.approx(2 / 3)
    assert len(result["confusion_matrix"]) == 5
    assert all(len(row) == 5 for row in result["confusion_matrix"])


def test_evaluation_rejects_mismatched_lengths():
    with pytest.raises(ValueError):
        evaluate_predictions(np.array([0]), np.array([0, 1]))
