# convert_to_sparse.py
# jasper sinclair
#
# Converts dense FEN-based training data into a compact sparse binary format
# suitable for fast memory-mapped loading during NNUE training.
#
# Input format (training.txt):
#     <FEN>|<result>
#
# Output format (training_sparse.bin), per position:
#     uint8  n_white
#     uint8  n_black
#     uint16 white_indices[n_white]
#     uint16 black_indices[n_black]
#     float32 result
#
# Each record stores active feature indices instead of full 768-length vectors,
# reducing disk usage and improving loading speed.

import struct
import json
import os
import random
from tqdm import tqdm

# =========================
# Constants
# =========================

# 6 piece types × 64 squares × 2 (us / them)
INPUT_SIZE = 768
WHITE = 0
BLACK = 1

# Piece type mapping.
# Color is handled separately via perspective logic.
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

    # ---------- Selfplay format ----------
    if "|" in line:
        fen_part, score_part = line.split("|", 1)

        fen = fen_part.strip()

        try:
            result = float(score_part.strip())
        except:
            return None, None

        result = max(0.0, min(1.0, result))
        return fen, result

    # ---------- quiet.epd format ----------
    if '"' in line:

        parts = line.split('"')
        if len(parts) < 2:
            return None, None

        result_str = parts[1]

        if result_str == "1-0":
            result = 1.0
        elif result_str == "0-1":
            result = 0.0
        elif result_str == "1/2-1/2":
            result = 0.5
        else:
            return None, None

        tokens = line.split()
        fen = " ".join(tokens[:4])

        return fen, result

    return None, None


# =========================
# Feature extraction
# =========================


def extract_indices(fen, perspective):
    """
    Extract active NNUE feature indices from a FEN string.

    Features are encoded relative to a given perspective:
        - Own pieces occupy first 384 indices
        - Opponent pieces occupy second 384 indices
        - Board is vertically flipped for black perspective

    Returns:
        List of active feature indices (sparse representation).
    """

    board_part = fen.split()[0]
    indices = []

    rank = 7
    file = 0

    for c in board_part:
        # Move to next rank
        if c == "/":
            rank -= 1
            file = 0
            continue

        # Skip empty squares
        if c.isdigit():
            file += int(c)
            continue

        # Skip unsupported symbols
        if c not in PIECE_TO_INDEX:
            file += 1
            continue

        # Convert (rank, file) into square index 0–63
        sq = rank * 8 + file

        piece_type = PIECE_TO_INDEX[c]
        piece_color = WHITE if c.isupper() else BLACK

        # Determine whether piece belongs to perspective side
        index_color = 1 if piece_color != perspective else 0

        # Flip board vertically for black perspective
        relative_sq = sq if perspective == WHITE else (sq ^ 56)

        # Compute final 0–767 feature index
        idx = 384 * index_color + 64 * piece_type + relative_sq
        indices.append(idx)

        file += 1

    return indices


# =========================
# Conversion Pipeline
# =========================


def convert(input_path, output_path, skip_invalid=True):

    valid = 0
    invalid = 0

    seen = set()
    MAX_HASH = 2000000

    with open(input_path, "r") as fin, open(output_path, "wb") as fout:

        lines = list(fin)

        for line in tqdm(lines):

            fen, result = parse_epd_line(line)

            if fen is None:
                invalid += 1
                continue

            key = fen
            if key in seen:
                continue

            if len(seen) < MAX_HASH:
                seen.add(key)

            # Extract sparse indices for both perspectives
            white_indices = extract_indices(fen, WHITE)
            black_indices = extract_indices(fen, BLACK)

            # ---------------------------------
            # Write binary record
            # ---------------------------------

            record = bytearray()

            # Write feature counts (uint8)
            record += struct.pack("B", len(white_indices))
            record += struct.pack("B", len(black_indices))

            # Write white feature indices (uint16)
            for idx in white_indices:
                record += struct.pack("<H", idx)

            # Write black feature indices (uint16)
            for idx in black_indices:
                record += struct.pack("<H", idx)

            # Write training target (float32)
            record += struct.pack("<f", result)

            fout.write(record)

            valid += 1

        return valid, invalid


# =========================
# Entry Point
# =========================

if __name__ == "__main__":

    config = load_config()

    input_path = config.get("training_file", "training.txt")
    output_path = config.get("sparse_training_file", "training_sparse.bin")
    skip_invalid = config.get("skip_invalid", True)
    print("Input:", input_path)
    print("Output:", output_path)

    valid, invalid = convert(input_path, output_path, skip_invalid)

    print(f"\nValid positions:   {valid}")
    print(f"Skipped positions: {invalid}")
