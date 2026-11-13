# ESP32 USB serial data capture

This utility records the current firmware CSV stream without changing sensor values.
It is intended for static-gesture dataset collection. It does not train or run the neural network.

## Requirements

- Python 3.11 (use the project's existing \`ai/.venv\` virtual environment).
- The ESP32 firmware from \`firmware/Luva_ESP32\`.
- USB data cable and the correct COM port.
- Arduino Serial Monitor/Plotter must be closed while this script is connected.

Install the only additional Python dependency from the repository root:

\`\`\`powershell
.\ai\.venv\Scripts\Activate.ps1
python -m pip install pyserial
\`\`\`

If the virtual environment is already active, run only the pip command.

## Run

From the repository root:

\`\`\`powershell
python .\tools\serial_capture.py COM3 --label P
\`\`\`

Replace COM3 with the ESP32 port shown in Arduino IDE. Keep the hand in the target static pose.
Press Ctrl+C to stop. The script sends START on connection and PAUSE on exit.

Examples:

\`\`\`powershell
python .\tools\serial_capture.py COM3 --label E
python .\tools\serial_capture.py COM3 --label D --duration 15
\`\`\`

Files are saved under \`data/raw/\` with a timestamp and an added \`gesture_label\` column.
Each CSV row is one firmware sample. Diagnostic lines beginning with \`#\` are ignored.
Rows with a column count different from the firmware header are skipped.

## Current firmware contract

The utility expects the existing firmware command protocol:

- \`START\`: begins collection and emits the CSV header.
- \`PAUSE\`: stops collection.
- Baud rate: 115200.
- Header begins with \`session_id,sample_id,timestamp_ms\`.

It records the complete firmware CSV (raw/calibrated flex readings, validity flags, motion fields and magnetic-gap fields). It does not assume that all 15 model features are calibrated or valid.

## Important data-collection guidance

- Calibrate sensors before collecting samples.
- Collect each gesture in separate captures.
- Keep the pose stable during each capture.
- Do not mix labels in one capture.
- Collect multiple independent captures, with brief breaks and repositioning.
- Do not train on overlapping or adjacent samples as if they were independent examples.
- Review validity columns before using the CSV for training.
- These captures are development data, not proof of Libras recognition accuracy.

The current firmware's 40 x 15 window is retained for later temporal/aggregation work. Static-gesture training input and feature selection must be finalized separately after checking real sensor availability and repeatability.
