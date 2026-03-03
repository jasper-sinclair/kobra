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
    'P': 0, 'N': 1, 'B': 2, 'R': 3, 'Q': 4, 'K': 5,
    'p': 0, 'n': 1, 'b': 2, 'r': 3, 'q': 4, 'k': 5
}


# =========================
# Feature Extraction
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
        if c == '/':
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

def convert(input_path="training.txt",
            output_path="training_sparse.bin"):
    """
    Convert dense text training file into sparse binary dataset.

    Input file format:
        FEN|result

    Example:
        rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1|0.5

    Output file is optimized for:
        - Minimal disk footprint
        - Fast mmap-based random access
        - Efficient reconstruction into dense tensors
    """

    with open(input_path, "r") as fin, \
         open(output_path, "wb") as fout:

        for line in tqdm(fin, desc="Converting positions"):

            # Skip malformed lines
            if "|" not in line:
                continue

            fen, result = line.strip().split("|")
            result = float(result.strip())

            # Extract sparse indices for both perspectives
            white_indices = extract_indices(fen, WHITE)
            black_indices = extract_indices(fen, BLACK)

            # ---------------------------------
            # Write binary record
            # ---------------------------------

            # Write feature counts (uint8)
            fout.write(struct.pack("B", len(white_indices)))
            fout.write(struct.pack("B", len(black_indices)))

            # Write white feature indices (uint16)
            for idx in white_indices:
                fout.write(struct.pack("<H", idx))

            # Write black feature indices (uint16)
            for idx in black_indices:
                fout.write(struct.pack("<H", idx))

            # Write training target (float32)
            fout.write(struct.pack("<f", result))


# =========================
# Entry Point
# =========================

if __name__ == "__main__":
    convert()