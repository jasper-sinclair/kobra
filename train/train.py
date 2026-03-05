# train.py
# jasper sinclair
#
# Trains a simple NNUE-style chess evaluation network using sparse binary data.
# Exports quantized weights for use inside a chess engine.

import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader, random_split
import numpy as np
import struct
import mmap
import json
import os
import sys
import random
import time
import logging


# =========================
# Logging Setup
# =========================
# Logs to both file and stdout for persistent training history.

logger = logging.getLogger()
logger.setLevel(logging.INFO)
logger.handlers.clear()

fh = logging.FileHandler("training.log", delay=False)
fh.setFormatter(logging.Formatter("%(asctime)s %(message)s"))

ch = logging.StreamHandler(sys.stdout)
ch.setFormatter(logging.Formatter("%(message)s"))

logger.addHandler(fh)
logger.addHandler(ch)


# =========================
# Constants
# =========================

# 6 piece types × 64 squares × 2 colors (us/them)
INPUT_SIZE = 768

WHITE = 0
BLACK = 1

# Piece type mapping (color handled separately)
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
# Utilities
# =========================


def load_config(path="config.json"):
    """Load JSON config file if present."""
    if not os.path.exists(path):
        logger.info("Config file not found, using defaults.")
        return {}
    with open(path, "r") as f:
        return json.load(f)


def set_seed(seed):
    """Ensure deterministic behavior."""
    random.seed(seed)
    np.random.seed(seed)
    torch.manual_seed(seed)
    torch.cuda.manual_seed_all(seed)


# =========================
# Feature Builder
# =========================


def build_features(fen, perspective):
    """
    Convert FEN string into 768-length NNUE-style binary feature vector.
    Board is flipped for black perspective.
    """

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
        if 0 <= idx < INPUT_SIZE:
            features[idx] = 1.0

        file += 1

    return features


# =========================
# Sparse Dataset
# =========================

INPUT_SIZE = 768


class SparseDataset(Dataset):
    """
    Memory-mapped sparse dataset.

    Record layout:
        uint8  n_white
        uint8  n_black
        uint16 white_indices[n_white]
        uint16 black_indices[n_black]
        float32 result
    """

    def __init__(self, path):
        self.path = path
        self.offsets = []

        # Build offset table
        with open(path, "rb") as f:
            offset = 0
            size = f.seek(0, 2)
            f.seek(0)

            while offset < size:
                self.offsets.append(offset)

                f.seek(offset)
                n_white = struct.unpack("B", f.read(1))[0]
                n_black = struct.unpack("B", f.read(1))[0]

                record_size = (
                    2
                    + 2 * n_white  # two uint8 counts
                    + 2 * n_black  # white indices
                    + 4  # black indices  # float32 result
                )

                offset += record_size

        self.file = open(path, "rb")
        self.mm = mmap.mmap(self.file.fileno(), 0, access=mmap.ACCESS_READ)

        logger.info("Dataset size: %d", len(self.offsets))

    def __len__(self):
        return len(self.offsets)

    def __getitem__(self, idx):
        offset = self.offsets[idx]
        ptr = offset

        n_white = self.mm[ptr]
        ptr += 1
        n_black = self.mm[ptr]
        ptr += 1

        xw = np.zeros(INPUT_SIZE, dtype=np.float32)
        xb = np.zeros(INPUT_SIZE, dtype=np.float32)

        for _ in range(n_white):
            idx_val = struct.unpack_from("<H", self.mm, ptr)[0]
            xw[idx_val] = 1.0
            ptr += 2

        for _ in range(n_black):
            idx_val = struct.unpack_from("<H", self.mm, ptr)[0]
            xb[idx_val] = 1.0
            ptr += 2

        result = struct.unpack_from("<f", self.mm, ptr)[0]

        return (
            torch.from_numpy(xw),
            torch.from_numpy(xb),
            torch.tensor(result, dtype=torch.float32),
        )


# =========================
# NNUE Model
# =========================


class NNUE(nn.Module):
    """
    Simple NNUE-style architecture:
        shared first layer
        clipped squared activation
        concatenation
        final linear output
    """

    def __init__(self, l1_size):
        super().__init__()
        self.fc1 = nn.Linear(INPUT_SIZE, l1_size)
        self.fc2 = nn.Linear(l1_size * 2, 1)

    def forward(self, xw, xb):

        hw = torch.clamp(self.fc1(xw), 0, 1)
        hb = torch.clamp(self.fc1(xb), 0, 1)

        hw = hw * hw
        hb = hb * hb

        h = torch.cat([hw, hb], dim=1)
        out = self.fc2(h)

        return out.squeeze(1)


# =========================
# Export Quantized Model
# =========================


def export_model(model, path, scale):
    """
    Export weights as int16 scaled values.
    """

    fc1_w = model.fc1.weight.detach().cpu().numpy()
    fc1_b = model.fc1.bias.detach().cpu().numpy()
    fc2_w = model.fc2.weight.detach().cpu().numpy()
    fc2_b = model.fc2.bias.detach().cpu().numpy()

    l1_size = fc1_w.shape[0]

    tmp_path = path + ".tmp"
    with open(tmp_path, "wb") as f:

        # fc1 weights
        for i in range(INPUT_SIZE):
            for j in range(l1_size):
                val = int(round(fc1_w[j][i] * scale))
                val = max(-32768, min(32767, val))
                f.write(struct.pack("<h", val))

        # fc1 bias (none in model → write zeros)
        for j in range(l1_size):
            val = int(round(fc1_b[j] * scale))
            val = max(-32768, min(32767, val))
            f.write(struct.pack("<h", val))

        # fc2 weights
        for j in range(l1_size * 2):
            val = int(round(fc2_w[0][j] * scale))
            val = max(-32768, min(32767, val))
            f.write(struct.pack("<h", val))

        # fc2 bias
        val = int(round(fc2_b[0] * scale))
        val = max(-32768, min(32767, val))
        f.write(struct.pack("<h", val))

    os.replace(tmp_path, path)


# =========================
# Training
# =========================


def main():

    config_path = sys.argv[1] if len(sys.argv) > 1 else "config.json"
    config = load_config(config_path)

    set_seed(config.get("seed", 42))

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    logger.info("Using device: %s", device)
    use_gpu = device.type == "cuda"

    # Only enable AMP if CUDA is available
    use_amp = config.get("use_amp", True) and device.type == "cuda"

    dataset = SparseDataset(config.get("training_file", "training_sparse.bin"))

    val_split = config.get("validation_split", 0.05)
    val_size = int(len(dataset) * val_split)
    train_size = len(dataset) - val_size

    train_set, val_set = random_split(dataset, [train_size, val_size])

    train_loader = DataLoader(
        train_set,
        batch_size=config.get("batch_size", 256),
        shuffle=True,
        num_workers=config.get("num_workers", 0),
        pin_memory=use_gpu        
    )

    val_loader = DataLoader(
        val_set, batch_size=config.get("batch_size", 256), shuffle=False
    )

    model = NNUE(config.get("l1_size", 128)).to(device)

    optimizer = optim.Adam(model.parameters(), lr=config.get("learning_rate", 1e-3))

    scheduler = optim.lr_scheduler.ReduceLROnPlateau(optimizer, factor=0.5, patience=3)

    criterion = nn.BCEWithLogitsLoss()

    # NEW AMP API (future-proof)
    scaler = torch.amp.GradScaler("cuda", enabled=use_amp)

    start_epoch = 0
    best_val_loss = float("inf")

    # Resume
    checkpoint_path = config.get("checkpoint_path", "checkpoint.pt")

    if os.path.exists(checkpoint_path):
        logger.info("Resuming from checkpoint...")
        checkpoint = torch.load(checkpoint_path, map_location=device)
        model.load_state_dict(checkpoint["model"])
        optimizer.load_state_dict(checkpoint["optimizer"])
        if "scheduler" in checkpoint:
            scheduler.load_state_dict(checkpoint["scheduler"])
        start_epoch = checkpoint["epoch"]
        best_val_loss = checkpoint["best_val_loss"]

    epochs = config.get("epochs", 10)
    log_every = config.get("log_every", 5000)
    mid_checkpoint_every = config.get("mid_checkpoint_every", 10000)

    for epoch in range(start_epoch, epochs):
        logger.info("\nEpoch %d/%d", epoch + 1, epochs)
        model.train()
        train_loss = 0.0
        start_time = time.time()

        for batch_idx, (xb_w, xb_b, yb) in enumerate(train_loader):

            xb_w = xb_w.to(device)
            xb_b = xb_b.to(device)
            yb = yb.to(device)

            optimizer.zero_grad()

            if use_amp:
                with torch.amp.autocast("cuda", enabled=True):
                    pred = model(xb_w, xb_b)
                    loss = criterion(pred, yb)

                scaler.scale(loss).backward()
                scaler.step(optimizer)
                scaler.update()
            else:
                pred = model(xb_w, xb_b)
                loss = criterion(pred, yb)
                loss.backward()
                optimizer.step()

            train_loss += loss.item()

            # progress logging
            # Determine triggers
            log_trigger = log_every > 0 and batch_idx % log_every == 0 and batch_idx > 0

            checkpoint_trigger = (
                mid_checkpoint_every > 0
                and batch_idx % mid_checkpoint_every == 0
                and batch_idx > 0
            )

            # Only proceed if either event happens
            if log_trigger or checkpoint_trigger:

                message = ""

                # Build full progress line only if it's a log interval
                if log_trigger:
                    elapsed = time.time() - start_time
                    rate = batch_idx / elapsed
                    remaining = (len(train_loader) - batch_idx) / rate

                    elapsed_h = int(elapsed // 3600)
                    elapsed_m = int((elapsed % 3600) // 60)
                    elapsed_s = int(elapsed % 60)

                    message = (
                        f"Epoch {epoch+1} | "
                        f"{batch_idx}/{len(train_loader)} "
                        f"({100*batch_idx/len(train_loader):.1f}%) | "
                        f"{rate:.1f} it/s | "
                        f"Elapsed {elapsed_h:02d}:{elapsed_m:02d}:{elapsed_s:02d} | "
                        f"ETA {remaining/60:.1f} min"
                    )

                # Run checkpoint independently
                if checkpoint_trigger:
                    state = {
                        "epoch": epoch,
                        "model": model.state_dict(),
                        "optimizer": optimizer.state_dict(),
                        "scheduler": scheduler.state_dict(),
                        "best_val_loss": best_val_loss,
                    }

                    tmp_path = "checkpoint_mid_epoch.pt.tmp"
                    torch.save(state, tmp_path)
                    os.replace(tmp_path, "checkpoint_mid_epoch.pt")

                    if not message:
                        # If checkpoint fires without log firing,
                        # build minimal message so formatting stays clean
                        message = f"Epoch {epoch+1} | {batch_idx}/{len(train_loader)}"

                    message += " | checkpoint saved"

                logger.info(message)
        # ---- end batch loop ----

        train_loss /= len(train_loader)

        # Validation
        model.eval()
        val_loss = 0.0

        with torch.no_grad():
            for xb_w, xb_b, yb in val_loader:

                xb_w = xb_w.to(device)
                xb_b = xb_b.to(device)
                yb = yb.to(device)

                pred = model(xb_w, xb_b)
                loss = criterion(pred, yb)

                val_loss += loss.item()

        val_loss /= len(val_loader)

        scheduler.step(val_loss)

        logger.info("Current LR: %s", optimizer.param_groups[0]["lr"])
        logger.info("Train Loss: %.6f", train_loss)
        logger.info("Val Loss:   %.6f", val_loss)

        # Save best model
        if val_loss < best_val_loss:
            best_val_loss = val_loss

            best_path = config.get("best_model_path", "best_model.pt")
            tmp_path = best_path + ".tmp"
            torch.save(model.state_dict(), tmp_path)
            os.replace(tmp_path, best_path)

            export_path = config.get("export_path", "network.bin")

            export_model(model, export_path, config.get("scale", 128))
            logger.info(f"Saved best model and exported {export_path}")
            print("Network size:", os.path.getsize(export_path))

        state = {
            "epoch": epoch + 1,
            "model": model.state_dict(),
            "optimizer": optimizer.state_dict(),
            "scheduler": scheduler.state_dict(),
            "best_val_loss": best_val_loss,
        }

        tmp_path = checkpoint_path + ".tmp"
        torch.save(state, tmp_path)
        os.replace(tmp_path, checkpoint_path)

        export_model(
            model, f"network_best_epoch_{epoch+1}.bin", config.get("scale", 128)
        )


if __name__ == "__main__":
    main()