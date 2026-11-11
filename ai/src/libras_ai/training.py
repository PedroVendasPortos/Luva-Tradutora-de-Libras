"""Ponto de entrada para o futuro pipeline de treinamento.

A divisão dos dados deverá ocorrer por sessão/participante, nunca por linhas
aleatórias de leituras correlacionadas da mesma execução.
"""

from __future__ import annotations

from pathlib import Path

import pandas as pd

from .preprocessing import prepare_features, prepare_labels


def load_dataset(csv_path: str | Path) -> tuple[pd.DataFrame, object, object]:
    """Carrega CSV, valida o contrato e retorna frame, X e y."""
    frame = pd.read_csv(csv_path)
    features = prepare_features(frame)
    labels = prepare_labels(frame)
    return frame, features, labels


def main() -> None:
    raise NotImplementedError(
        "Próxima etapa: implementar divisão por sessão, treinamento, "
        "salvamento do modelo e métricas usando um dataset real."
    )


if __name__ == "__main__":
    main()
