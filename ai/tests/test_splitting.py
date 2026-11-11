import numpy as np
import pandas as pd
import pytest

from libras_ai.splitting import make_group_splits


def make_grouped_frame(group_count=15):
    rows = []
    for group in range(group_count):
        for label_index, label in enumerate(("P", "E", "D", "R", "O")):
            rows.append(
                {
                    "participant_id": f"p{group % 3}",
                    "session_id": f"s{group}",
                    "label": label,
                    "encoded_label": label_index,
                }
            )
    frame = pd.DataFrame(rows)
    return frame, frame["encoded_label"].to_numpy()


def test_splits_keep_sessions_separate_and_include_all_classes():
    frame, labels = make_grouped_frame()
    train, validation, test = make_group_splits(frame, labels)

    groups = frame["participant_id"] + "::" + frame["session_id"]
    group_sets = [set(groups.iloc[idx]) for idx in (train, validation, test)]
    assert group_sets[0].isdisjoint(group_sets[1])
    assert group_sets[0].isdisjoint(group_sets[2])
    assert group_sets[1].isdisjoint(group_sets[2])
    for indices in (train, validation, test):
        np.testing.assert_array_equal(np.unique(labels[indices]), np.arange(5))


def test_requires_at_least_three_independent_sessions():
    frame, labels = make_grouped_frame(group_count=2)
    with pytest.raises(ValueError, match="pelo menos 3"):
        make_group_splits(frame, labels)
