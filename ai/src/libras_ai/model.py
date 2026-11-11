"""Definição da MLP de referência."""

from __future__ import annotations

from tensorflow import keras


def build_mlp(
    input_features: int = 5,
    num_classes: int = 5,
    hidden_units: tuple[int, int] = (16, 8),
) -> keras.Model:
    """Constrói uma MLP simples; dimensões e classes são configuráveis."""
    if input_features <= 0:
        raise ValueError("input_features deve ser positivo.")
    if num_classes < 2:
        raise ValueError("num_classes deve ser pelo menos 2.")
    if len(hidden_units) != 2 or any(units <= 0 for units in hidden_units):
        raise ValueError("hidden_units deve conter dois tamanhos positivos.")

    model = keras.Sequential(
        [
            keras.layers.Input(shape=(input_features,), name="sensor_features"),
            keras.layers.Dense(hidden_units[0], activation="relu", name="dense_1"),
            keras.layers.Dense(hidden_units[1], activation="relu", name="dense_2"),
            keras.layers.Dense(num_classes, activation="softmax", name="gesture_probabilities"),
        ],
        name="libras_static_mlp",
    )
    model.compile(
        optimizer=keras.optimizers.Adam(),
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"],
    )
    return model
