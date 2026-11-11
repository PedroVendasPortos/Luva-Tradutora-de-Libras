"""Métricas básicas para avaliar classificadores multiclasse."""

from __future__ import annotations

import numpy as np
from sklearn.metrics import accuracy_score, classification_report, confusion_matrix

from .schema import EXPECTED_CLASSES


def evaluate_predictions(y_true: np.ndarray, y_pred: np.ndarray) -> dict:
    """Calcula métricas com ordem de classes fixa e inclui classes sem ocorrência."""
    true_values = np.asarray(y_true, dtype=np.int64)
    predicted_values = np.asarray(y_pred, dtype=np.int64)

    if true_values.ndim != 1 or predicted_values.ndim != 1:
        raise ValueError("y_true e y_pred devem ser vetores unidimensionais.")
    if true_values.size == 0 or true_values.size != predicted_values.size:
        raise ValueError("Os vetores devem ter o mesmo tamanho não nulo.")

    class_indices = list(range(len(EXPECTED_CLASSES)))
    if not np.isin(true_values, class_indices).all():
        raise ValueError("y_true contém índice de classe inválido.")
    if not np.isin(predicted_values, class_indices).all():
        raise ValueError("y_pred contém índice de classe inválido.")

    return {
        "accuracy": float(accuracy_score(true_values, predicted_values)),
        "confusion_matrix": confusion_matrix(
            true_values, predicted_values, labels=class_indices
        ).tolist(),
        "classification_report": classification_report(
            true_values,
            predicted_values,
            labels=class_indices,
            target_names=list(EXPECTED_CLASSES),
            output_dict=True,
            zero_division=0,
        ),
    }
