"""Inferência e rejeição de previsões pouco confiáveis."""

from __future__ import annotations

import numpy as np

from .schema import EXPECTED_CLASSES


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
