"""Classificador por protótipos para validar o caminho sensor -> letra.

Não é um modelo treinado. Compara a leitura calibrada com perfis configurados.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass

import numpy as np

from .gestures import GESTURE_PROFILES, MAX_DISTANCE, MIN_MARGIN


@dataclass(frozen=True)
class Classification:
    label: str | None
    distance: float
    second_distance: float
    accepted: bool
    reason: str


def classify(
    values,
    *,
    max_distance: float = MAX_DISTANCE,
    min_margin: float = MIN_MARGIN,
) -> Classification:
    """Classifica cinco valores normalizados; rejeita leituras distantes/ambíguas.

    A distância é o RMSE entre os cinco sensores e o perfil de cada letra.
    O intervalo dos sensores deve ser [0, 1], na ordem oficial do projeto.
    """
    vector = np.asarray(values, dtype=np.float32)
    if vector.shape != (5,):
        raise ValueError("A leitura deve conter exatamente cinco valores.")
    if not np.isfinite(vector).all():
        raise ValueError("A leitura contém valores ausentes ou não finitos.")
    if ((vector < 0.0) | (vector > 1.0)).any():
        raise ValueError("Os sensores devem estar no intervalo [0, 1].")
    if max_distance < 0 or min_margin < 0:
        raise ValueError("Os limites de distância e margem não podem ser negativos.")

    distances = sorted(
        (
            float(np.sqrt(np.mean((vector - np.asarray(profile, dtype=np.float32)) ** 2))),
            label,
        )
        for label, profile in GESTURE_PROFILES.items()
    )
    best_distance, best_label = distances[0]
    second_distance = distances[1][0]
    margin = second_distance - best_distance

    if best_distance > max_distance:
        return Classification(None, best_distance, second_distance, False, "fora_dos_perfis")
    if margin < min_margin:
        return Classification(None, best_distance, second_distance, False, "ambiguo")
    return Classification(best_label, best_distance, second_distance, True, "reconhecido")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Testa uma leitura calibrada da luva contra os perfis experimentais P/E/D/R/O."
    )
    parser.add_argument(
        "values", nargs=5, type=float,
        metavar=("POLEGAR", "INDICADOR", "MEDIO", "ANELAR", "MINIMO"),
        help="Cinco valores calibrados entre 0 e 1, nessa ordem.",
    )
    args = parser.parse_args()
    result = classify(args.values)
    print(f"resultado={result.label or 'NAO_RECONHECIDO'}")
    print(f"aceito={result.accepted}")
    print(f"motivo={result.reason}")
    print(f"distancia={result.distance:.4f}")
    print(f"segunda_distancia={result.second_distance:.4f}")


if __name__ == "__main__":
    main()
