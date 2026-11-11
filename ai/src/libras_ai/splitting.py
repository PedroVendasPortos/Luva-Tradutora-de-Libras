"""Divisão de treino/validação/teste sem separar linhas da mesma sessão."""

from __future__ import annotations

import numpy as np
import pandas as pd
from sklearn.model_selection import GroupShuffleSplit

from .schema import EXPECTED_CLASSES


def make_group_splits(
    frame: pd.DataFrame,
    labels: np.ndarray,
    *,
    test_size: float = 0.20,
    validation_size: float = 0.20,
    random_state: int = 42,
    max_attempts: int = 200,
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Divide por participante+sessão e exige todas as classes em cada parte.

    O algoritmo tenta sementes diferentes porque GroupShuffleSplit não
    estratifica. Se não encontrar uma divisão válida, falha explicitamente.
    """
    if not 0 < test_size < 1 or not 0 < validation_size < 1:
        raise ValueError("test_size e validation_size devem estar entre 0 e 1.")
    if test_size + validation_size >= 1:
        raise ValueError("A soma de test_size e validation_size deve ser menor que 1.")
    if max_attempts < 1:
        raise ValueError("max_attempts deve ser positivo.")
    labels = np.asarray(labels)
    if labels.ndim != 1:
        raise ValueError("labels deve ser um vetor unidimensional.")
    if len(frame) != len(labels):
        raise ValueError("frame e labels devem ter o mesmo número de linhas.")
    if not np.isin(labels, np.arange(len(EXPECTED_CLASSES))).all():
        raise ValueError("labels contém índice de classe inválido.")

    required = {"participant_id", "session_id"}
    if not required.issubset(frame.columns):
        raise ValueError(f"Metadados obrigatórios ausentes: {sorted(required - set(frame.columns))}")

    groups = (
        frame["participant_id"].astype(str)
        + "::"
        + frame["session_id"].astype(str)
    ).to_numpy()
    unique_groups = np.unique(groups)
    if len(unique_groups) < 3:
        raise ValueError(
            "São necessárias pelo menos 3 sessões/grupos independentes "
            "para treino, validação e teste."
        )

    all_classes = set(range(len(EXPECTED_CLASSES)))
    test_splitter = GroupShuffleSplit(
        n_splits=max_attempts,
        test_size=test_size,
        random_state=random_state,
    )
    for train_validation_idx, test_idx in test_splitter.split(
        np.zeros(len(labels)), labels, groups
    ):
        relative_validation_size = validation_size / (1.0 - test_size)
        validation_splitter = GroupShuffleSplit(
            n_splits=max_attempts,
            test_size=relative_validation_size,
            random_state=random_state + 1,
        )
        local_groups = groups[train_validation_idx]
        for train_local, validation_local in validation_splitter.split(
            np.zeros(len(train_validation_idx)),
            labels[train_validation_idx],
            local_groups,
        ):
            train_idx = train_validation_idx[train_local]
            validation_idx = train_validation_idx[validation_local]
            partitions = (train_idx, validation_idx, test_idx)
            if all(set(np.unique(labels[idx])) == all_classes for idx in partitions):
                # Defesa adicional: nenhum grupo pode aparecer em duas partições.
                group_sets = [set(groups[idx]) for idx in partitions]
                if (
                    group_sets[0].isdisjoint(group_sets[1])
                    and group_sets[0].isdisjoint(group_sets[2])
                    and group_sets[1].isdisjoint(group_sets[2])
                ):
                    return train_idx, validation_idx, test_idx

    raise ValueError(
        "Não foi possível criar divisões com todas as cinco classes em treino, "
        "validação e teste sem misturar sessões. Colete mais sessões independentes "
        "e garanta que cada sessão contenha exemplos de P, E, D, R e O."
    )
