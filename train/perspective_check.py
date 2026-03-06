# perspective_check.py
# jasper sinclair
#
# Quick sanity check for selfplay training data.
#
# This script reads a sample of positions from training.txt
# and checks if the training labels match the side-to-move
# perspective.
#
# Expected behavior:
#   - If white is to move, labels should generally be > 0.5
#   - If black is to move, labels should generally be < 0.5
#
# If many positions violate this expectation, it usually means
# the perspective or label generation logic in selfplay is broken.

import random
import json
import os


# =========================
# Config loader
# =========================

def load_config(path="config.json"):
    """Load configuration file if present."""
    if not os.path.exists(path):
        return {}

    with open(path, "r") as f:
        return json.load(f)


# =========================
# Main check
# =========================

def main():

    config = load_config()

    # Dataset path
    training_file = config.get("training_file", "training.txt")

    # How many positions to sample (default 20k like original script)
    sample_limit = config.get("dataset_sample_limit", 20000)

    # Threshold for warning
    warning_threshold = config.get("perspective_warning_threshold", 0.2)

    bad = 0
    total = 0

    print("Checking dataset:", training_file)

    with open(training_file) as f:

        for line in f:

            # Skip lines without label separator
            if "|" not in line:
                continue

            fen, val = line.split("|", 1)

            label = float(val.strip())

            # Extract side-to-move
            stm = fen.split()[1]

            # Suspicious cases
            if stm == "w" and label < 0.2:
                bad += 1

            if stm == "b" and label > 0.8:
                bad += 1

            total += 1

            # Stop once sample limit reached
            if sample_limit and total >= sample_limit:
                break

    print("checked:", total)
    print("suspicious:", bad)

    if total == 0:
        print("No valid positions found.")
        return

    ratio = bad / total

    print("suspicious ratio:", ratio)

    # Warn if suspicious positions exceed threshold
    if ratio > warning_threshold:
        print("⚠ perspective likely broken")
    else:
        print("dataset looks healthy")


if __name__ == "__main__":
    main()