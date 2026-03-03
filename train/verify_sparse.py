# verify_sparse.py
# jasper sinclair
#
# Verifies correctness of training_sparse.bin by comparing it against
# the original training.txt file.
#
# This script:
#   1. Rebuilds dense features from FEN (text version)
#   2. Reconstructs dense features from sparse binary
#   3. Compares both representations
#   4. Verifies result value matches
#
# If all checks pass, sparse conversion is correct.

import struct
import random
import numpy as np


# =========================
# Constants
# =========================

# 6 piece types × 64 squares × 2 (us / them)
INPUT_SIZE = 768

WHITE = 0
BLACK = 1

# Piece type mapping (color handled separately)
PIECE_TO_INDEX = {
    'P': 0, 'N': 1, 'B': 2, 'R': 3, 'Q': 4, 'K': 5,
    'p': 0, 'n': 1, 'b': 2, 'r': 3, 'q': 4, 'k': 5
}


# =========================
# Dense Feature Builder
# =========================

def build_features(fen, perspective):
    """
    Reconstruct full 768-length dense feature vector from FEN.

    This mirrors the logic used in:
        - convert_to_sparse.py
        - train.py

    Used here to validate sparse reconstruction.
    """

    board_part = fen.split()[0]
    features = np.zeros(INPUT_SIZE, dtype=np.float32)

    rank = 7
    file = 0

    for c in board_part:
        if c == '/':
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

        # Determine perspective-relative color
        index_color = 1 if piece_color != perspective else 0

        # Flip vertically for black perspective
        relative_sq = sq if perspective == WHITE else (sq ^ 56)

        idx = 384 * index_color + 64 * piece_type + relative_sq
        features[idx] = 1.0

        file += 1

    return features


# =========================
# Load Sparse Binary Offsets
# =========================
#
# We first scan the binary file once to compute record offsets.
# This mirrors the logic used in SparseDataset.

offsets = []

with open("training_sparse.bin", "rb") as f:
    pos = 0

    while True:
        header = f.read(2)

        # End of file
        if not header:
            break

        n_white = header[0]
        n_black = header[1]

        # Compute full record size
        record_size = 2 + 2*n_white + 2*n_black + 4

        offsets.append(pos)

        # Skip remaining bytes in record
        f.seek(record_size - 2, 1)
        pos += record_size

print("Binary dataset size:", len(offsets))


# =========================
# Load Original Text Dataset
# =========================

with open("training.txt", "r") as f:
    lines = f.readlines()

print("Text dataset size:", len(lines))


# =========================
# Randomized Verification
# =========================
#
# We randomly sample 10 positions and verify:
#   - White feature vector matches
#   - Black feature vector matches
#   - Result value matches

for _ in range(10):

    idx = random.randint(0, len(offsets) - 1)

    # ----- Original Text Version -----

    fen, result_txt = lines[idx].strip().split("|")
    result_txt = float(result_txt.strip())

    # Build dense features directly from FEN
    xw_txt = build_features(fen, WHITE)
    xb_txt = build_features(fen, BLACK)

    # ----- Binary Sparse Version -----

    with open("training_sparse.bin", "rb") as f:
        f.seek(offsets[idx])
        data = f.read()

    n_white = data[0]
    n_black = data[1]
    offset = 2

    # Read white indices
    white_indices = struct.unpack_from(f"<{n_white}H", data, offset)
    offset += 2 * n_white

    # Read black indices
    black_indices = struct.unpack_from(f"<{n_black}H", data, offset)
    offset += 2 * n_black

    # Read result
    result_bin = struct.unpack_from("<f", data, offset)[0]

    # Reconstruct dense vectors from sparse indices
    xw_bin = np.zeros(INPUT_SIZE, dtype=np.float32)
    xb_bin = np.zeros(INPUT_SIZE, dtype=np.float32)

    xw_bin[list(white_indices)] = 1.0
    xb_bin[list(black_indices)] = 1.0

    # ----- Assertions -----

    # Feature vectors must match exactly
    assert np.allclose(xw_txt, xw_bin)
    assert np.allclose(xb_txt, xb_bin)

    # Target value must match (allow tiny float tolerance)
    assert abs(result_txt - result_bin) < 1e-6

print("✅ All verification tests passed.")