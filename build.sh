BUILD_DIR="build"
CACHE_FILE="$BUILD_DIR/CMakeCache.txt"
ROOT_PATH=$(pwd)

if [ -f "$CACHE_FILE" ]; then
  CACHED_PATH=$(grep "CMAKE_HOME_DIRECTORY" "$CACHE_FILE" | cut -d= -f2)
  if [ "$CACHED_PATH" != "$ROOT_PATH" ]; then
    echo "[SmartBuild] Путь изменился: $CACHED_PATH -> $ROOT_PATH. Очистка..."
    rm -rf "$BUILD_DIR"
  fi
fi

mkdir -p "$BUILD_DIR"
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel $(nproc)

if [ -f "$BUILD_DIR/forge" ]; then
  "$BUILD_DIR/forge" engine-build
fi
