#!/usr/bin/env bash
set -e

echo "Compiling tests..."

# Allow overriding compiler via CXX, default to g++, fall back to clang++ if needed.
if [ -z "$CXX" ]; then
  if command -v g++ >/dev/null 2>&1; then
    CXX="g++"
  elif command -v clang++ >/dev/null 2>&1; then
    CXX="clang++"
  else
    echo "Error: No suitable C++ compiler found (g++ or clang++ required)." >&2
    exit 1
  fi
fi

# Optional: use ./build if it exists, otherwise current directory.
OUT_DIR="."
if [ -d "./build" ]; then
  OUT_DIR="./build"
fi

RACK_SDK_DIR="${RACK_DIR:-dep/Rack-SDK}"

TESTS=(
  "src/tests/test_tapestry.cpp:build_test_tapestry"
  "src/tests/test_korupt.cpp:build_test_korupt"
)

echo "Running tests..."
for entry in "${TESTS[@]}"; do
  src="${entry%%:*}"
  bin="${entry##*:}"
  out_bin="${OUT_DIR}/${bin}"
  "$CXX" -std=c++17 -O2 -Wall -Isrc -I"${RACK_SDK_DIR}/include" -I"${RACK_SDK_DIR}/dep/include" -DSHORTWAV_DSP_RUN_TESTS -o "$out_bin" "$src"
  "$out_bin"
done

echo "Tests passed."
