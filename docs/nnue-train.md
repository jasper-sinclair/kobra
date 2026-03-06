## Minimal 768-256 Piece-Square NNUE

This folder contains scripts and tools for training a **minimal NNUE chess evaluation network** using sparse training data.

The design focuses on **simplicity, speed, and debuggability** rather than maximum strength.

This architecture is well suited for experimentation and typically reaches **~2300–2500 Elo** when trained on a sufficiently large dataset.

---

## Feature Encoding

The network uses a **piece-square feature representation**.

```
6 piece types × 64 squares × 2 perspectives
Total input size: 768
```

Features represent:

```
Piece type on square from side-to-move perspective
```

Characteristics:

* Perspective-relative encoding
* Board flipped vertically for black
* No king-relative indexing
* No HalfKP features
* No handcrafted evaluation inputs

This produces a **simple and compact NNUE input layer**.

---

## Architecture

```
Input: 768
Hidden: 256 (shared first layer)
Output: 1 scalar evaluation
```

The hidden layer is **shared between both perspectives**, following the classic NNUE accumulator design.

---

## Forward Pass

```
Linear layer (768 → 256)

Activation:
    clamp(x, 0, 1)

Squared activation:
    x²
```

Two accumulators are maintained:

```
White perspective accumulator
Black perspective accumulator
```

The final evaluation is computed as:

```
Concatenate accumulators (256 + 256 = 512)

Final linear layer (512 → 1)

Output:
    scalar evaluation
```

---

## Parameter Count

First layer weights

```
768 × 256 = 196,608
```

First layer bias

```
256
```

Output layer weights

```
512
```

Output bias

```
1
```

Total parameters

```
197,377
```

Approximately

```
~197k parameters
```

---

## Quantized Network Format

The trained network is exported as **int16 weights** for fast inference inside the chess engine.

The exported file contains:

```
768 × 256 input weights
256 input biases
512 output weights
1 output bias
```

Stored as 16-bit integers.

Expected file size:

```
≈ 395 KB
```

---

## Data Format

Training data is stored in a **sparse binary format** for efficient loading.

Record layout:

```
uint8   n_white
uint8   n_black
uint16  white_indices[n_white]
uint16  black_indices[n_black]
float32 result
```

Each position stores only **active feature indices**, avoiding full 768-element vectors.

---

## Training Pipeline

Typical workflow:

```
training.txt / quiet.epd
        ↓
convert_to_sparse.py
        ↓
training_sparse.bin
        ↓
train.py
        ↓
network.bin
        ↓
C++ engine
```

The trainer uses:

```
Loss: BCEWithLogitsLoss
Optimizer: Adam
Scheduler: ReduceLROnPlateau
```

---

## Design Goals

This network intentionally avoids the complexity of modern NNUE designs.

Not included:

```
King-relative indexing
HalfKP features
Bucket networks
Multi-layer heads
Additional handcrafted features
```

Advantages:

```
Very simple training pipeline
Small and fast evaluation
Easy debugging
Easy visualization
Minimal engine integration
```

---

## Network Summary

```
Network type: Piece-square NNUE
Feature count: 768
Hidden nodes: 256
Parameters: ~197k
Quantized size: ~395 KB
```

---

## Visualization Tools

Optional scripts can visualize learned weights and neuron activations.

Useful for detecting:

```
Dead neurons
Feature imbalance
Broken training data
Overfitting
```

These tools typically require:

```
pip install matplotlib numpy seaborn
```

---

## License

This project is provided for experimentation and research.

Use freely in your chess engine projects.
