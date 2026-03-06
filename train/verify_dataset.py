# verify_dataset.py
# jasper sinclair
#
# Simple integrity check for the sparse NNUE training dataset.
#
# This script scans the binary file produced by convert_to_sparse.py
# and verifies that all stored feature indices are valid.
#
# Dataset record layout:
#
#   uint8   n_white           number of white-perspective features
#   uint8   n_black           number of black-perspective features
#   uint16  white_indices[]   sparse feature indices
#   uint16  black_indices[]   sparse feature indices
#   float32 result            training target
#
# Each index must be within the valid NNUE feature range:
#
#   0 <= index < 768
#
# If any index is outside this range, the dataset is corrupted.

import struct
import json
import os

INPUT_SIZE = 768


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
# Main verification
# =========================

def main():

    config = load_config()

    # Dataset path from config
    sparse_path = config.get("sparse_training_file", "training_sparse.bin")

    # Optional limit (useful for quickly checking large datasets)
    sample_limit = config.get("dataset_sample_limit", 0)

    print("Verifying dataset:", sparse_path)

    checked = 0

    # Open sparse training dataset
    with open(sparse_path, "rb") as f:

        while True:

            # Optional limit for quick verification
            if sample_limit and checked >= sample_limit:
                break

            # Read record header (2 bytes)
            header = f.read(2)

            # End of file
            if not header:
                break

            # Number of sparse features for each perspective
            n_white, n_black = header

            # -------------------------
            # Verify white feature indices
            # -------------------------
            for _ in range(n_white):

                # Each index is stored as uint16
                idx = struct.unpack("<H", f.read(2))[0]

                # Feature index must be within NNUE input range
                if idx >= INPUT_SIZE:
                    print("BAD INDEX", idx)
                    return

            # -------------------------
            # Verify black feature indices
            # -------------------------
            for _ in range(n_black):

                idx = struct.unpack("<H", f.read(2))[0]

                if idx >= INPUT_SIZE:
                    print("BAD INDEX", idx)
                    return

            # Skip result value (float32)
            f.read(4)

            checked += 1

    print("Dataset OK")
    print("Records checked:", checked)


if __name__ == "__main__":
    main()