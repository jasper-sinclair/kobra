King-Relative HalfKP NNUE
This is a C++ SIMD Engine Model

Feature Encoding:
King-relative HalfKP indexing
ps_end = 10 × 64 + 1 = 641

Input size:
64 king squares × 641 piece-square features
= 41,024 inputs

Feature representation:
“Piece on square relative to king square”
This allows the network to directly model:
King safety
Pawn shelter
Attack patterns
Long-term positional structure

Architecture:
Input:   41,024
Hidden:  256 (per side accumulator)
Output:  1 scalar evaluation

Parameter Count:
~10.5 million first-layer weights

Implementation Details:
Fully quantized int8 weights
AVX2 SIMD (__m256i)
256-bit vectorized inference
Incremental accumulator updates
Engine-optimized memory layout
