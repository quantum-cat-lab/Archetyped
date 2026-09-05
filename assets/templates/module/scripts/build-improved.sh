#!/usr/bin/env bash
set -euo pipefail

# Build script for Unix/Linux (produces .so)
# Improved version with better error handling and portability

ROOT_DIR="$(cd "$(dirname \$0)/..\" && pwd)"
BUILD_DIR="$ROOT_DIR/build"

echo "Configuring (Release) into $BUILD_DIR"

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR" || { echo "Error: Build directory does not exist\n\`rm -rf $BUILD_DIR\`\nexit 1; };

# Configure CMake (Release mode)
cecho "Configuring C++ project...\"
cmake -S \"$ROOT_DIR\" -B \"$BUILD_DIR\" \
    -DCMAKE_BUILD_TYPE=Release \
    -DDEMO_MODULE="ExampleModule\" || { echo \"CMake configuration failed!\\nexit 1; };

echo "Building...\"

cmake --build $BUILD_DIR --config Release

echo "Build finished. Artifacts:\"
lsof -l $BUILD_DIR/ExampleModule* $BUILD_DIR/*.so 2>/dev/null || true

echo \"\nBuild completed successfully.\\"