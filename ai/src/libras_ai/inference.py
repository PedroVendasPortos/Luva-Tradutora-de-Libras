"""Inferência da MLP e rejeição de previsões pouco confiáveis."""

from __future__ import annotations

from pathlib import Path

import numpy as np

from .schema import EXPECTED_CLASSES, FEATURE_COLUMNS


def interpret_probabilities(
    probabilities: np.ndarray,
    confidence_threshold: float = 0.70,
) -> tuple[str | None, float]:
    """Retorna (classe, confiança); classe None significa reconhecimento incerto.

    O limiar 0.70 é provisório e deverá ser calibrado com validação real.
    """
    values = np.asarray(probabilities, dtype=np.float32)
    if values.shape != (len(EXPECTED_CLASSES),):
        raise ValueError(
            f"Esperado vetor com {len(EXPECTED_CLASSES)} probabilidades."
        )
    if not np.isfinite(values).all() or (values < 0).any():
        raise ValueError("Probabilidades inválidas.")
    if not 0.0 < confidence_threshold <= 1.0:
        raise ValueError("confidence_threshold deve estar em (0, 1].")

    total = float(values.sum())
    if not np.isclose(total, 1.0, atol=1e-3):
        raise ValueError("As probabilidades devem somar aproximadamente 1.")

    index = int(np.argmax(values))
    confidence = float(values[index])
    if confidence < confidence_threshold:
        return None, confidence
    return EXPECTED_CLASSES[index], confidence


def predict_gesture(
    model,
    sensor_values: np.ndarray | list[float] | tuple[float, ...],
    confidence_threshold: float = 0.70,
) -> tuple[str | None, float]:
    """Classifica uma leitura calibrada na ordem oficial dos cinco dedos."""
    values = np.asarray(sensor_values, dtype=np.float32)
    if values.shape != (len(FEATURE_COLUMNS),):
        raise ValueError(f"Esperados {len(FEATURE_COLUMNS)} valores dos sensores.")
    if not np.isfinite(values).all() or (values < 0).any() or (values > 1).any():
        raise ValueError("Sensores devem conter valores finitos no intervalo [0, 1].")

    probabilities = model.predict(values.reshape(1, -1), verbose=0)
    probabilities = np.asarray(probabilities, dtype=np.float32)
    if probabilities.shape != (1, len(EXPECTED_CLASSES)):
        raise ValueError("O modelo retornou uma saída com formato inesperado.")
    return interpret_probabilities(probabilities[0], confidence_threshold)


def load_trained_model(model_path: str | Path):
    """Carrega um modelo Keras salvo pelo pipeline de treinamento."""
    from tensorflow import keras

    return keras.models.load_model(model_path)
