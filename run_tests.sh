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

# Test artifacts stay out of the repository root and are removed by Rack's
# ordinary `make clean` target.
OUT_DIR="./build"
mkdir -p "$OUT_DIR"

RACK_DIR="${RACK_DIR:-dep/Rack-SDK}"

TESTS=(
  "src/tests/test_tapestry.cpp:build_test_tapestry"
  "src/tests/test_korupt.cpp:build_test_korupt"
  "src/tests/test_fray_core.cpp:build_test_fray_core"
)

echo "Running tests..."
for entry in "${TESTS[@]}"; do
  src="${entry%%:*}"
  bin="${entry##*:}"
  out_bin="${OUT_DIR}/${bin}"
  "$CXX" -std=c++11 -O2 -Wall -Wextra -Wpedantic -Isrc -I"$RACK_DIR/include" -I"$RACK_DIR/dep/include" -DSHORTWAV_DSP_RUN_TESTS -o "$out_bin" "$src"
  "$out_bin"
done

# fray-effects.h intentionally uses Rack's supported public SDK header. Keep its
# unit test Rack-linked while fray-core.h remains portable and standalone.
rack_effects_bin="${OUT_DIR}/build_test_fray_effects"
"$CXX" -std=c++11 -O2 -Wall -Wextra -Wpedantic -Wno-unused-parameter -pthread \
  -isystem "$RACK_DIR/include" -isystem "$RACK_DIR/dep/include" \
  src/tests/test_fray_effects.cpp \
  -L"$RACK_DIR" -lRack -o "$rack_effects_bin"
if [ -x "${rack_effects_bin}.exe" ]; then
  rack_effects_bin="${rack_effects_bin}.exe"
fi
DYLD_LIBRARY_PATH="$RACK_DIR:${DYLD_LIBRARY_PATH:-}" \
LD_LIBRARY_PATH="$RACK_DIR:${LD_LIBRARY_PATH:-}" \
PATH="$RACK_DIR:$PATH" \
  "$rack_effects_bin"

# The Rack-linked Fray adapter belongs in the test runner rather than the plugin
# Makefile, which stays limited to Rack's normal build/package integration.
rack_bin="${OUT_DIR}/build_test_fray_module"
"$CXX" -std=c++11 -O2 -Wall -Wextra -Wpedantic -Wno-unused-parameter -pthread \
  -isystem "$RACK_DIR/include" -isystem "$RACK_DIR/dep/include" \
  src/tests/test_fray_module.cpp src/Fray.cpp \
  -L"$RACK_DIR" -lRack -o "$rack_bin"
if [ -x "${rack_bin}.exe" ]; then
  rack_bin="${rack_bin}.exe"
fi
DYLD_LIBRARY_PATH="$RACK_DIR:${DYLD_LIBRARY_PATH:-}" \
LD_LIBRARY_PATH="$RACK_DIR:${LD_LIBRARY_PATH:-}" \
PATH="$RACK_DIR:$PATH" \
  "$rack_bin"

echo "Tests passed."
