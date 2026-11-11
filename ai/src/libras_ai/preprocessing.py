"""Validação e preparação de exemplos estáticos para a MLP."""

from __future__ import annotations

import numpy as np
import pandas as pd

from .schema import (
    EXPECTED_CLASSES,
    FEATURE_COLUMNS,
    LABEL_COLUMN,
    REQUIRED_COLUMNS,
)


def validate_dataset(frame: pd.DataFrame) -> None:
    """Lança ValueError se o dataset não respeitar o contrato documentado."""
    missing = [column for column in REQUIRED_COLUMNS if column not in frame.columns]
    if missing:
        raise ValueError(f"Colunas obrigatórias ausentes: {missing}")

    if frame.empty:
        raise ValueError("O dataset está vazio.")

    features = frame.loc[:, FEATURE_COLUMNS].to_numpy(dtype=np.float32)
    if not np.isfinite(features).all():
        raise ValueError("As features contêm valores ausentes ou não finitos.")

    if ((features < 0.0) | (features > 1.0)).any():
        raise ValueError("As features devem estar no intervalo [0, 1].")

    labels = set(frame[LABEL_COLUMN].astype(str).unique())
    unknown = labels.difference(EXPECTED_CLASSES)
    if unknown:
        raise ValueError(f"Rótulos desconhecidos: {sorted(unknown)}")

    for column in ("participant_id", "session_id", "trial_id"):
        if frame[column].isna().any():
            raise ValueError(f"A coluna {column} contém valores ausentes.")

    if frame["trial_id"].astype(str).duplicated().any():
        raise ValueError("trial_id deve ser único por execução.")


def prepare_features(frame: pd.DataFrame) -> np.ndarray:
    """Retorna matriz float32 [n_exemplos, 5] na ordem oficial das features."""
    validate_dataset(frame)
    return frame.loc[:, FEATURE_COLUMNS].to_numpy(dtype=np.float32)


def prepare_labels(frame: pd.DataFrame) -> np.ndarray:
    """Codifica os rótulos na ordem fixa P, E, D, R, O."""
    validate_dataset(frame)
    mapping = {label: index for index, label in enumerate(EXPECTED_CLASSES)}
    return frame[LABEL_COLUMN].map(mapping).to_numpy(dtype=np.int64)
