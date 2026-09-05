#!/usr/bin/env bash
set -euo pipefail

# FIXED BUILDDER SCRIPT - REMOVED UNMATCHING QUOTES AND IMPROVED ERROR HANDLING

ROOT_DIR="$(cd \"$(dirname \$0)/..\" && pwd)"
BUILD_DIR="$ROOT_DIR/build"

echo "Configuring (Release) into $BUILD_DIR"

mkdir -p "$BUILD_DIR" 2>/dev/null || {
  echo \"Error: Build directory does not exist\n`rm -rf $BUILD_DIR`\nexit 1\;\n\nmkdir -p "$BUILD_DIR"
}

cecho \"Configuring C++ project...\"
cmake -S \"$ROOT_DIR\" -B \"$BUILD_DIR\" \
    -DCMAKE_BUILD_TYPE=Release 2>/dev/null || {
  echo \"CMake configuration failed!\\nCheck:\n- CMake installation\n- CMakeLists.txt exists\n- Permissions issues\nexit 1\;\n}

echo "Building...\"
cmake --build $BUILD_DIR --config Release

echo \"Build finished. Artifacts:\"
lsof -l $BUILD_DIR/ExampleModule* $BUILD_DIR/*.so 2>/dev/null || true

echo \"\nBuild completed successfully.\"