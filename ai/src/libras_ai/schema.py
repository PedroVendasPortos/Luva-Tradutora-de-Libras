"""Contrato central do dataset e das features da primeira MLP."""

FEATURE_COLUMNS = (
    "flex_thumb",
    "flex_index",
    "flex_middle",
    "flex_ring",
    "flex_pinky",
)
METADATA_COLUMNS = ("participant_id", "session_id", "trial_id")
LABEL_COLUMN = "label"
REQUIRED_COLUMNS = FEATURE_COLUMNS + (LABEL_COLUMN,) + METADATA_COLUMNS
EXPECTED_CLASSES = ("P", "E", "D", "R", "O")
