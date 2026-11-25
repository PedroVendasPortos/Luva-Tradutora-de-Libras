#!/usr/bin/env python3
"""Integra o CSV do firmware ESP32 ao classificador experimental P/E/D/R/O.

O firmware continua responsavel pela aquisicao, calibracao e validacao.
Este programa roda no computador, recebe o CSV pela USB e classifica blocos
de leituras validas dos cinco sensores flexiveis.
"""

from __future__ import annotations

import argparse
import csv
import sys
import time
from pathlib import Path

import numpy as np
import serial
from serial import SerialException

ROOT = Path(__file__).resolve().parents[1]
AI_SOURCE = ROOT / "ai" / "src"
sys.path.insert(0, str(AI_SOURCE))

from libras_ai.experimental.classifier import classify  # noqa: E402

FINGERS = ("polegar", "indicador", "medio", "anelar", "minimo")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Recebe sensores da luva e executa o classificador experimental."
    )
    parser.add_argument("port", help="Porta serial da ESP32, por exemplo COM4")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument(
        "--window-samples",
        type=int,
        default=20,
        help="Leituras validas por classificacao (20 = aproximadamente 1 segundo a 20 Hz).",
    )
    parser.add_argument(
        "--startup-wait",
        type=float,
        default=2.5,
        help="Tempo para a ESP32 reiniciar ao abrir a porta serial.",
    )
    return parser.parse_args()


def required_columns(header: list[str]) -> list[int]:
    columns = []
    for finger in FINGERS:
        name = f"flex_{finger}_calibrated"
        if name not in header:
            raise ValueError(f"Coluna obrigatoria ausente no CSV: {name}")
        columns.append(header.index(name))

        for suffix in ("valid", "stuck"):
            flag = f"flex_{finger}_{suffix}"
            if flag not in header:
                raise ValueError(f"Coluna obrigatoria ausente no CSV: {flag}")
    return columns


def valid_sensor_vector(fields: list[str], header: list[str],
                        value_columns: list[int]) -> list[float] | None:
    values = []
    for finger, value_index in zip(FINGERS, value_columns):
        valid_index = header.index(f"flex_{finger}_valid")
        stuck_index = header.index(f"flex_{finger}_stuck")

        if fields[valid_index] != "1" or fields[stuck_index] != "0":
            return None
        if not fields[value_index].strip():
            return None

        value = float(fields[value_index]) / 1000.0
        if not np.isfinite(value) or value < 0.0 or value > 1.0:
            return None
        values.append(value)
    return values


def main() -> int:
    args = parse_args()
    if args.baud <= 0 or args.window_samples < 1 or args.startup_wait < 0:
        print("Parametros invalidos: baud e window-samples devem ser positivos.", file=sys.stderr)
        return 2

    print(f"Conectando a {args.port} em {args.baud} baud.")
    print("Feche o Monitor Serial da Arduino IDE antes de continuar.")
    print("A classificacao so comeca quando os cinco flexiveis estiverem validos e calibrados.")

    try:
        with serial.Serial(args.port, args.baud, timeout=1, write_timeout=2) as device:
            time.sleep(args.startup_wait)
            device.reset_input_buffer()
            device.write(b"START\n")
            device.flush()
            print("START enviado. Mantenha uma pose estatica por vez. Ctrl+C encerra.")

            header = None
            value_columns = None
            batch: list[list[float]] = []
            warned_invalid = False

            try:
                while True:
                    raw = device.readline()
                    if not raw:
                        continue

                    line = raw.decode("utf-8", errors="replace").strip()
                    if not line or line.startswith("#"):
                        continue

                    try:
                        fields = next(csv.reader([line]))
                    except (csv.Error, StopIteration):
                        continue

                    if header is None:
                        if len(fields) < 3 or fields[:3] != [
                            "session_id", "sample_id", "timestamp_ms"
                        ]:
                            continue
                        header = fields
                        value_columns = required_columns(header)
                        print(f"Cabecalho recebido: {len(header)} colunas.")
                        print("Aguardando bloco de leituras flexiveis validas...")
                        continue

                    if len(fields) != len(header):
                        batch.clear()
                        continue

                    try:
                        int(fields[0])
                        int(fields[1])
                        int(fields[2])
                        vector = valid_sensor_vector(fields, header, value_columns)
                    except (ValueError, IndexError):
                        vector = None

                    if vector is None:
                        batch.clear()
                        if not warned_invalid:
                            print("Aguardando: flexivel sem leitura valida ou sem calibracao.")
                            warned_invalid = True
                        continue

                    warned_invalid = False
                    batch.append(vector)
                    if len(batch) < args.window_samples:
                        continue

                    # A mediana reduz pequenas oscilacoes sem misturar poses diferentes.
                    stable_vector = np.median(np.asarray(batch, dtype=np.float32), axis=0)
                    result = classify(stable_vector)
                    vector_text = ",".join(f"{value:.3f}" for value in stable_vector)

                    if result.accepted:
                        print(
                            f"GESTO={result.label} | aceito=1 | "
                            f"distancia={result.distance:.4f} | "
                            f"segunda={result.second_distance:.4f} | "
                            f"sensores=[{vector_text}]"
                        )
                    else:
                        print(
                            f"NAO_RECONHECIDO | motivo={result.reason} | "
                            f"distancia={result.distance:.4f} | "
                            f"segunda={result.second_distance:.4f} | "
                            f"sensores=[{vector_text}]"
                        )
                    batch.clear()

            except KeyboardInterrupt:
                print("\nEncerrando classificacao...")

            finally:
                try:
                    device.write(b"PAUSE\n")
                    device.flush()
                except SerialException:
                    pass

            if header is None:
                print("Nenhum cabecalho CSV recebido. Verifique firmware e porta.", file=sys.stderr)
                return 1

    except SerialException as exc:
        print(f"Erro serial: {exc}", file=sys.stderr)
        print("Confira a porta COM e feche o Monitor Serial da Arduino IDE.", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
