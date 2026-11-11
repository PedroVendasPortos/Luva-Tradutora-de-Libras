"""Treinamento reproduzível da MLP para gestos estáticos."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
import pandas as pd
import tensorflow as tf

from .evaluation import evaluate_predictions
from .model import build_mlp
from .preprocessing import prepare_features, prepare_labels
from .schema import EXPECTED_CLASSES, FEATURE_COLUMNS
from .splitting import make_group_splits


def load_dataset(csv_path: str | Path) -> tuple[pd.DataFrame, np.ndarray, np.ndarray]:
    """Carrega CSV, valida o contrato e retorna frame, X e y."""
    frame = pd.read_csv(csv_path)
    features = prepare_features(frame)
    labels = prepare_labels(frame)
    return frame, features, labels


def train_model(
    csv_path: str | Path,
    output_dir: str | Path,
    *,
    epochs: int = 200,
    batch_size: int = 16,
    random_state: int = 42,
    test_size: float = 0.20,
    validation_size: float = 0.20,
) -> dict:
    """Treina, avalia em sessões isoladas e salva artefatos do experimento."""
    if epochs < 1 or batch_size < 1:
        raise ValueError("epochs e batch_size devem ser positivos.")

    frame, features, labels = load_dataset(csv_path)
    train_idx, validation_idx, test_idx = make_group_splits(
        frame,
        labels,
        test_size=test_size,
        validation_size=validation_size,
        random_state=random_state,
    )

    tf.keras.utils.set_random_seed(random_state)
    model = build_mlp(input_features=len(FEATURE_COLUMNS), num_classes=len(EXPECTED_CLASSES))
    callbacks = [
        tf.keras.callbacks.EarlyStopping(
            monitor="val_loss",
            patience=20,
            restore_best_weights=True,
        )
    ]

    history = model.fit(
        features[train_idx],
        labels[train_idx],
        validation_data=(features[validation_idx], labels[validation_idx]),
        epochs=epochs,
        batch_size=batch_size,
        callbacks=callbacks,
        verbose=2,
    )

    probabilities = model.predict(features[test_idx], verbose=0)
    predictions = np.argmax(probabilities, axis=1)
    metrics = evaluate_predictions(labels[test_idx], predictions)

    destination = Path(output_dir)
    destination.mkdir(parents=True, exist_ok=True)
    model_path = destination / "static_mlp.keras"
    model.save(model_path)

    groups = (
        frame["participant_id"].astype(str)
        + "::"
        + frame["session_id"].astype(str)
    )
    split_summary = {
        "train": _split_summary(frame, labels, train_idx, groups),
        "validation": _split_summary(frame, labels, validation_idx, groups),
        "test": _split_summary(frame, labels, test_idx, groups),
    }
    report = {
        "model": str(model_path.name),
        "classes": list(EXPECTED_CLASSES),
        "features": list(FEATURE_COLUMNS),
        "random_state": random_state,
        "epochs_requested": epochs,
        "epochs_completed": len(history.history["loss"]),
        "batch_size": batch_size,
        "split": split_summary,
        "test_metrics": metrics,
        "confidence_threshold": 0.70,
        "confidence_threshold_status": "provisional_not_calibrated",
        "warning": (
            "Resultados dependem deste dataset e não comprovam generalização "
            "para outros participantes ou condições."
        ),
    }
    (destination / "metrics.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    return report


def _split_summary(
    frame: pd.DataFrame,
    labels: np.ndarray,
    indices: np.ndarray,
    groups: pd.Series,
) -> dict:
    counts = pd.Series(labels[indices]).value_counts().to_dict()
    return {
        "examples": int(len(indices)),
        "groups": sorted(groups.iloc[indices].unique().tolist()),
        "class_counts": {
            label: int(counts.get(index, 0))
            for index, label in enumerate(EXPECTED_CLASSES)
        },
    }


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Treina e avalia a MLP de gestos estáticos da luva."
    )
    parser.add_argument("csv", help="Caminho para o dataset CSV real e rotulado.")
    parser.add_argument(
        "--output",
        default="models/experimento_001",
        help="Diretório para salvar modelo e métricas.",
    )
    parser.add_argument("--epochs", type=int, default=200)
    parser.add_argument("--batch-size", type=int, default=16)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--test-size", type=float, default=0.20)
    parser.add_argument("--validation-size", type=float, default=0.20)
    args = parser.parse_args()

    report = train_model(
        args.csv,
        args.output,
        epochs=args.epochs,
        batch_size=args.batch_size,
        random_state=args.seed,
        test_size=args.test_size,
        validation_size=args.validation_size,
    )
    print(json.dumps(report["test_metrics"], ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
