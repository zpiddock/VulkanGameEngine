#version 450

// No outputs - depth is written automatically by the GPU
// Fragment shader is required even for depth-only rendering, but it can be empty

void main() {
    // Depth is written automatically to the depth attachment
    // No color output needed for depth pre-pass
}
