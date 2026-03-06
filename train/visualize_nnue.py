# visualize_nnue.py
# jasper sinclair
#
# Advanced visualization tool for inspecting trained NNUE networks.
#
# This script loads the trained PyTorch model and analyzes the first
# layer (fc1) weights, which correspond directly to the NNUE input
# features (piece-square features).
#
# The script provides:
#
#   • Network health analysis (dead neuron detection)
#   • Histogram of neuron strengths
#   • Piece specialization analysis
#   • Global feature importance heatmap
#   • Interactive neuron visualization
#
# Useful for diagnosing:
#   - dead neurons
#   - training collapse
#   - piece-square feature learning
#   - specialization patterns


import torch
import numpy as np
import matplotlib.pyplot as plt
import sys
import json


# =========================
# Constants
# =========================

# NNUE input size:
# 6 piece types × 64 squares × 2 perspectives = 768
INPUT_SIZE = 768

# Piece labels used in visualizations
PIECES = ["Pawn", "Knight", "Bishop", "Rook", "Queen", "King"]


# =========================
# Model Loader
# =========================

def load_model(path):
    """
    Load the trained PyTorch model and extract the first-layer weights.

    We only care about fc1.weight because it maps
    piece-square inputs → hidden neurons.

    Shape:
        (neurons, 768)
    """

    model = torch.load(path, map_location="cpu")

    # PyTorch checkpoint format
    if isinstance(model, dict) and "fc1.weight" in model:
        weights = model["fc1.weight"].numpy()
    else:
        raise RuntimeError("Could not find fc1 weights")

    return weights


# =========================
# Single Neuron Visualization
# =========================

def visualize_neuron(weights, neuron):
    """
    Visualize the piece-square weights for a single neuron.

    Each neuron is split into 6 boards:
        Pawn, Knight, Bishop, Rook, Queen, King

    Each board is an 8x8 heatmap showing which squares
    activate that neuron.
    """

    fig, axes = plt.subplots(2, 3, figsize=(10, 6))

    for piece in range(6):

        start = piece * 64
        end = start + 64

        # Extract piece-square slice
        data = weights[neuron, start:end]

        board = data.reshape(8, 8)

        ax = axes[piece // 3][piece % 3]

        im = ax.imshow(board, cmap="coolwarm")

        ax.set_title(PIECES[piece])
        ax.axis("off")

    fig.colorbar(im)

    fig.suptitle(f"Neuron {neuron}")

    plt.show()


# =========================
# Network Health Analysis
# =========================

def analyze_network(weights):
    """
    Compute overall neuron activity statistics.

    Detects:
        - dead neurons
        - weak neurons
        - overall weight magnitude distribution
    """

    # Mean absolute weight per neuron
    neuron_strength = np.mean(np.abs(weights), axis=1)

    # Count neurons with extremely small weights
    dead = np.sum(neuron_strength < 1e-5)

    print("Neurons:", len(neuron_strength))
    print("Dead neurons:", dead)

    # Histogram of neuron strengths
    plt.hist(neuron_strength, bins=50)
    plt.title("Neuron strength distribution")
    plt.show()


# =========================
# Piece Specialization Analysis
# =========================

def neuron_specialization(weights):
    """
    Determine which piece type each neuron responds to most.

    For each neuron:
        measure average weight magnitude per piece type
        report the strongest piece category.
    """

    print("\nNeuron specialization:\n")

    for n in range(weights.shape[0]):

        scores = []

        for piece in range(6):

            start = piece * 64
            end = start + 64

            val = np.mean(np.abs(weights[n, start:end]))

            scores.append(val)

        best_piece = np.argmax(scores)

        print(f"Neuron {n:3d} -> {PIECES[best_piece]}")


# =========================
# Interactive Neuron Viewer
# =========================

def interactive_view(weights):
    """
    Interactive inspection of neurons.

    User enters a neuron index and the script
    displays its piece-square heatmaps.
    """

    while True:

        try:
            n = int(input("Neuron index (-1 to quit): "))

            if n < 0:
                break

            visualize_neuron(weights, n)

        except Exception as e:
            print("Error:", e)


# =========================
# Global Feature Importance
# =========================

def global_heatmap(weights):
    """
    Compute global importance of each piece-square feature.

    For each square and piece type we compute the
    average absolute weight across all neurons.

    This reveals which squares are most important
    for each piece.
    """

    heatmap = np.zeros((6, 64))

    for piece in range(6):

        start = piece * 64
        end = start + 64

        piece_weights = weights[:, start:end]

        # Average importance across neurons
        heatmap[piece] = np.mean(np.abs(piece_weights), axis=0)

    fig, axes = plt.subplots(2, 3, figsize=(10,6))

    for piece in range(6):

        board = heatmap[piece].reshape(8,8)

        ax = axes[piece//3][piece%3]

        im = ax.imshow(board, cmap="inferno")

        ax.set_title(PIECES[piece])
        ax.axis("off")

    fig.colorbar(im)

    plt.suptitle("Global NNUE Feature Importance")

    plt.show()


# =========================
# Main Entry Point
# =========================

def main():

    # Load training configuration
    with open("config.json") as f:
        config = json.load(f)

    # Load best trained model
    path = config.get("best_model_path", "best_model.pt")

    print("Loading model:", path)

    weights = load_model(path)

    # Basic network diagnostics
    analyze_network(weights)

    # Determine neuron specialization
    neuron_specialization(weights)

    # Show global feature importance
    global_heatmap(weights)

    # Interactive neuron viewer
    interactive_view(weights)


if __name__ == "__main__":
    main()