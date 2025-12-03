#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LLVM_PASS_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PASS_PATH="${LLVM_PASS_DIR}/build/ImgOptPass.so"
ARMPL_DIR="/opt/arm/armpl_24.10_gcc"

CC="clang-18"
CXX="clang++-18"

[ ! -f "$PASS_PATH" ] && echo "ERROR: Pass not found" && exit 1
[ ! -d "$ARMPL_DIR" ] && echo "ERROR: ArmPL not found" && exit 1

COMMON_FLAGS="-O3 -ffast-math -march=native"
PASS_FLAGS="-Xclang -fpass-plugin=$PASS_PATH"
ARMPL_FLAGS="-fveclib=ArmPL -I$ARMPL_DIR/include -L$ARMPL_DIR/lib -Wl,-rpath,$ARMPL_DIR/lib -lamath"

# 1. STB
echo "Compiling STB..."
cd "$SCRIPT_DIR/build_stb"
STB_SRC="${LLVM_PASS_DIR}/demo/stb/stb_resize_benchmark.c"
$CC $COMMON_FLAGS -o stb_o3 "$STB_SRC" -lm
$CC $COMMON_FLAGS $ARMPL_FLAGS -o stb_armpl "$STB_SRC" -lm
$CC $COMMON_FLAGS $PASS_FLAGS $ARMPL_FLAGS -o stb_pass_armpl "$STB_SRC" -lm 2>/dev/null

# 2. libpng
echo "Compiling libpng..."
LIBPNG_DIR="${LLVM_PASS_DIR}/demo/libpng"
LIBPNG_FLAGS="-I${LIBPNG_DIR} -I${LIBPNG_DIR}/.. -L${LIBPNG_DIR} -lpng16 -lz"
cd "${LIBPNG_DIR}/contrib/libtests"
$CC $COMMON_FLAGS $LIBPNG_FLAGS -o "$SCRIPT_DIR/build_libpng/pngstest_o3" pngstest.c -lm
$CC $COMMON_FLAGS $ARMPL_FLAGS $LIBPNG_FLAGS -o "$SCRIPT_DIR/build_libpng/pngstest_armpl" pngstest.c -lm
$CC $COMMON_FLAGS $PASS_FLAGS $ARMPL_FLAGS $LIBPNG_FLAGS -o "$SCRIPT_DIR/build_libpng/pngstest_pass_armpl" pngstest.c -lm 2>/dev/null

# 3. Filament
echo "Compiling Filament..."
cd "$SCRIPT_DIR/build_filament"
FILAMENT_DIR="${LLVM_PASS_DIR}/demo/filament"
FILAMENT_SRC="${FILAMENT_DIR}/filament_resize_benchmark.cpp"
FILAMENT_FLAGS="-I${FILAMENT_DIR}/filament/include -I${FILAMENT_DIR}/libs/math/include -I${FILAMENT_DIR}/libs/utils/include -I${LLVM_PASS_DIR}/demo/stb -std=c++17"
$CXX $COMMON_FLAGS $FILAMENT_FLAGS -o filament_o3 "$FILAMENT_SRC" -lm
$CXX $COMMON_FLAGS $FILAMENT_FLAGS $ARMPL_FLAGS -o filament_armpl "$FILAMENT_SRC" -lm
$CXX $COMMON_FLAGS $FILAMENT_FLAGS $PASS_FLAGS $ARMPL_FLAGS -o filament_pass_armpl "$FILAMENT_SRC" -lm 2>/dev/null

echo "Done."
