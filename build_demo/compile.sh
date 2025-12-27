#!/bin/bash
set -e

LLVM_PASS_DIR="/home/ubuntu/llvm-pass-clean"
BUILD_DIR="/home/ubuntu/llvm-pass-clean/build_demo"

# 1. STB
echo "Compiling STB..."
cd "$BUILD_DIR/build_stb"
# o3 (執行檔)
clang-18 -O3 -ffast-math -fveclib=ArmPL -L/opt/arm/armpl_24.10_gcc/lib -Wl,-rpath,/opt/arm/armpl_24.10_gcc/lib -lamath "$LLVM_PASS_DIR/demo/stb/stb_resize_benchmark.c" -o stb -lm
# o3+mypass
clang-18 -O3 -ffast-math -fpass-plugin="$LLVM_PASS_DIR/build/ImgOptPass.so" -fveclib=ArmPL -L/opt/arm/armpl_24.10_gcc/lib -Wl,-rpath,/opt/arm/armpl_24.10_gcc/lib -lamath "$LLVM_PASS_DIR/demo/stb/stb_resize_benchmark.c" -o stb_pass -lm
# TODO: 補.ll檔（o3 ＆ o3+pass）(到底 ＆ mypass->vectorize之間)

# 2. libpng
echo "Compiling libpng..."
cd "$BUILD_DIR/build_libpng"
# 查-I flag, -L flag
clang-18 -O3 -ffast-math -fveclib=ArmPL -L/opt/arm/armpl_24.10_gcc/lib -Wl,-rpath,/opt/arm/armpl_24.10_gcc/lib -lamath -I"$LLVM_PASS_DIR/demo/libpng" -L"$LLVM_PASS_DIR/demo/libpng" "$LLVM_PASS_DIR/demo/libpng/contrib/libtests/pngstest.c" -o pngstest -lpng16 -lz -lm
clang-18 -O3 -ffast-math -fpass-plugin="$LLVM_PASS_DIR/build/ImgOptPass.so" -fveclib=ArmPL -L/opt/arm/armpl_24.10_gcc/lib -Wl,-rpath,/opt/arm/armpl_24.10_gcc/lib -lamath -I"$LLVM_PASS_DIR/demo/libpng" -L"$LLVM_PASS_DIR/demo/libpng" "$LLVM_PASS_DIR/demo/libpng/contrib/libtests/pngstest.c" -o pngstest_pass -lpng16 -lz -lm

# 3. Filament
echo "Compiling Filament..."
cd "$BUILD_DIR/build_filament"
clang++-18 -O3 -ffast-math -fveclib=ArmPL -L/opt/arm/armpl_24.10_gcc/lib -Wl,-rpath,/opt/arm/armpl_24.10_gcc/lib -lamath -I"$LLVM_PASS_DIR/demo/filament/filament/include" -I"$LLVM_PASS_DIR/demo/filament/libs/math/include" -I"$LLVM_PASS_DIR/demo/filament/libs/utils/include" -I"$LLVM_PASS_DIR/demo/stb" -std=c++17 "$LLVM_PASS_DIR/demo/filament/filament_resize_benchmark.cpp" -o filament -lm
clang++-18 -O3 -ffast-math -fpass-plugin="$LLVM_PASS_DIR/build/ImgOptPass.so" -fveclib=ArmPL -L/opt/arm/armpl_24.10_gcc/lib -Wl,-rpath,/opt/arm/armpl_24.10_gcc/lib -lamath -I"$LLVM_PASS_DIR/demo/filament/filament/include" -I"$LLVM_PASS_DIR/demo/filament/libs/math/include" -I"$LLVM_PASS_DIR/demo/filament/libs/utils/include" -I"$LLVM_PASS_DIR/demo/stb" -std=c++17 "$LLVM_PASS_DIR/demo/filament/filament_resize_benchmark.cpp" -o filament_pass -lm

echo "Done."
