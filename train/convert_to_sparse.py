import struct
from tqdm import tqdm

INPUT_SIZE = 768
WHITE = 0
BLACK = 1

PIECE_TO_INDEX = {
    'P': 0, 'N': 1, 'B': 2, 'R': 3, 'Q': 4, 'K': 5,
    'p': 0, 'n': 1, 'b': 2, 'r': 3, 'q': 4, 'k': 5
}

def extract_indices(fen, perspective):
    board_part = fen.split()[0]
    indices = []

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

        index_color = 1 if piece_color != perspective else 0
        relative_sq = sq if perspective == WHITE else (sq ^ 56)

        idx = 384 * index_color + 64 * piece_type + relative_sq
        indices.append(idx)

        file += 1

    return indices


def convert(input_path="training.txt",
            output_path="training_sparse.bin"):

    with open(input_path, "r") as fin, \
         open(output_path, "wb") as fout:

        for line in tqdm(fin):
            if "|" not in line:
                continue

            fen, result = line.strip().split("|")
            result = float(result.strip())

            white_indices = extract_indices(fen, WHITE)
            black_indices = extract_indices(fen, BLACK)

            # write counts
            fout.write(struct.pack("B", len(white_indices)))
            fout.write(struct.pack("B", len(black_indices)))

            # write indices
            for idx in white_indices:
                fout.write(struct.pack("<H", idx))

            for idx in black_indices:
                fout.write(struct.pack("<H", idx))

            # write result
            fout.write(struct.pack("<f", result))


if __name__ == "__main__":
    convert()