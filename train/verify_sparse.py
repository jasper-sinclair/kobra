# verify_sparse.py
# jasper sinclair
#
# Verifies correctness of training_sparse.bin by comparing it against
# the original training dataset used for conversion.
#
# This script:
#   1. Rebuilds dense features from FEN (text version)
#   2. Reconstructs dense features from sparse binary
#   3. Compares both representations
#   4. Verifies result value matches

import struct
import random
import numpy as np
import json
import os

# =========================
# Constants
# =========================

INPUT_SIZE = 768

WHITE = 0
BLACK = 1

PIECE_TO_INDEX = {
    "P": 0,
    "N": 1,
    "B": 2,
    "R": 3,
    "Q": 4,
    "K": 5,
    "p": 0,
    "n": 1,
    "b": 2,
    "r": 3,
    "q": 4,
    "k": 5,
}

MAX_HASH = 2000000

# =========================
# Config loader
# =========================


def load_config(path="config.json"):
    if not os.path.exists(path):
        return {}
    with open(path, "r") as f:
        return json.load(f)


# =========================
# Parsing
# =========================


def parse_epd_line(line):

    line = line.strip()

    if '"' not in line:
        return None, None

    result_str = line.split('"')[1]

    if result_str == "1-0":
        result = 1.0
    elif result_str == "0-1":
        result = 0.0
    elif result_str == "1/2-1/2":
        result = 0.5
    else:
        return None, None

    parts = line.split()
    fen = " ".join(parts[:4])

    return fen, result


# =========================
# Feature Builder
# =========================


def build_features(fen, perspective):

    board_part = fen.split()[0]
    features = np.zeros(INPUT_SIZE, dtype=np.float32)

    rank = 7
    file = 0

    for c in board_part:

        if c == "/":
            rank -= 1
            file = 0
            continue

        if c.isdigit():
            file += int(c)
            continue

        if c not in PIECE_TO_INDEX:
            file += 1
            continue

        sq = rank * 8 + file
        piece_type = PIECE_TO_INDEX[c]
        piece_color = WHITE if c.isupper() else BLACK

        index_color = 1 if piece_color != perspective else 0
        relative_sq = sq if perspective == WHITE else (sq ^ 56)

        idx = 384 * index_color + 64 * piece_type + relative_sq
        features[idx] = 1.0

        file += 1

    return features


# =========================
# Build filtered dataset
# =========================


def build_filtered_dataset(epd_path):

    dataset = []
    seen = set()

    with open(epd_path, "r") as f:

        for line in f:

            fen, result = parse_epd_line(line)

            if fen is None:
                continue

            if fen in seen:
                continue

            if len(seen) < MAX_HASH:
                seen.add(fen)

            dataset.append((fen, result))

    return dataset


# =========================
# Compute binary offsets
# =========================


def compute_offsets(sparse_path):

    offsets = []

    with open(sparse_path, "rb") as f:

        pos = 0

        while True:

            header = f.read(2)
            if not header:
                break

            n_white = header[0]
            n_black = header[1]

            record_size = 2 + 2 * n_white + 2 * n_black + 4

            offsets.append(pos)

            f.seek(record_size - 2, 1)

            pos += record_size

    return offsets


# =========================
# Main verification
# =========================


def main():

    config = load_config()

    sparse_path = config.get("sparse_training_file", "training_sparse.bin")
    epd_path = config.get("verification_epd", "quiet.epd")

    print("Sparse dataset:", sparse_path)
    print("EPD reference:", epd_path)

    offsets = compute_offsets(sparse_path)

    print("Binary dataset size:", len(offsets))

    dataset = build_filtered_dataset(epd_path)

    print("Filtered dataset size:", len(dataset))

    if len(dataset) != len(offsets):
        print("WARNING: dataset sizes differ")

    # =========================
    # Random verification
    # =========================

    samples = 10

    for _ in range(samples):

        idx = random.randint(0, min(len(dataset), len(offsets)) - 1)

        fen, result = dataset[idx]

        xw_txt = build_features(fen, WHITE)
        xb_txt = build_features(fen, BLACK)

        with open(sparse_path, "rb") as f:

            f.seek(offsets[idx])

            header = f.read(2)
            n_white = header[0]
            n_black = header[1]

            record_size = 2 + 2 * n_white + 2 * n_black + 4

            data = header + f.read(record_size - 2)

        offset = 2

        white_indices = struct.unpack_from(f"<{n_white}H", data, offset)
        offset += 2 * n_white

        black_indices = struct.unpack_from(f"<{n_black}H", data, offset)
        offset += 2 * n_black

        result_bin = struct.unpack_from("<f", data, offset)[0]

        xw_bin = np.zeros(INPUT_SIZE, dtype=np.float32)
        xb_bin = np.zeros(INPUT_SIZE, dtype=np.float32)

        xw_bin[list(white_indices)] = 1.0
        xb_bin[list(black_indices)] = 1.0

        assert np.allclose(xw_txt, xw_bin)
        assert np.allclose(xb_txt, xb_bin)
        assert abs(result - result_bin) < 1e-6

    print("✅ Verification passed.")


if __name__ == "__main__":
    main()