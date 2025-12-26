#!/bin/bash
set -e

LLVM_PASS_DIR="/home/ubuntu/llvm-pass-clean"
BUILD_DIR="/home/ubuntu/llvm-pass-clean/build_demo"

# 1. STB
echo "Compiling STB..."
cd "$BUILD_DIR/build_stb"
clang-18 -O3 -ffast-math -o stb_o3 "$LLVM_PASS_DIR/demo/stb/stb_resize_benchmark.c" -lm
clang-18 -O3 -ffast-math -fveclib=ArmPL -L/opt/arm/armpl_24.10_gcc/lib -Wl,-rpath,/opt/arm/armpl_24.10_gcc/lib -lamath -o stb_armpl "$LLVM_PASS_DIR/demo/stb/stb_resize_benchmark.c" -lm
clang-18 -O3 -ffast-math -Xclang -fpass-plugin="$LLVM_PASS_DIR/build/ImgOptPass.so" -fveclib=ArmPL -L/opt/arm/armpl_24.10_gcc/lib -Wl,-rpath,/opt/arm/armpl_24.10_gcc/lib -lamath -o stb_pass_armpl "$LLVM_PASS_DIR/demo/stb/stb_resize_benchmark.c" -lm

# 2. libpng
echo "Compiling libpng..."
cd "$LLVM_PASS_DIR/demo/libpng/contrib/libtests"
clang-18 -O3 -ffast-math -I"$LLVM_PASS_DIR/demo/libpng" -L"$LLVM_PASS_DIR/demo/libpng" -lpng16 -lz -o "$BUILD_DIR/build_libpng/pngstest_o3" pngstest.c -lm
clang-18 -O3 -ffast-math -fveclib=ArmPL -L/opt/arm/armpl_24.10_gcc/lib -Wl,-rpath,/opt/arm/armpl_24.10_gcc/lib -lamath -I"$LLVM_PASS_DIR/demo/libpng" -L"$LLVM_PASS_DIR/demo/libpng" -lpng16 -lz -o "$BUILD_DIR/build_libpng/pngstest_armpl" pngstest.c -lm
clang-18 -O3 -ffast-math -Xclang -fpass-plugin="$LLVM_PASS_DIR/build/ImgOptPass.so" -fveclib=ArmPL -L/opt/arm/armpl_24.10_gcc/lib -Wl,-rpath,/opt/arm/armpl_24.10_gcc/lib -lamath -I"$LLVM_PASS_DIR/demo/libpng" -L"$LLVM_PASS_DIR/demo/libpng" -lpng16 -lz -o "$BUILD_DIR/build_libpng/pngstest_pass_armpl" pngstest.c -lm

# 3. Filament
echo "Compiling Filament..."
cd "$BUILD_DIR/build_filament"
clang++-18 -O3 -ffast-math -I"$LLVM_PASS_DIR/demo/filament/filament/include" -I"$LLVM_PASS_DIR/demo/filament/libs/math/include" -I"$LLVM_PASS_DIR/demo/filament/libs/utils/include" -I"$LLVM_PASS_DIR/demo/stb" -std=c++17 -o filament_o3 "$LLVM_PASS_DIR/demo/filament/filament_resize_benchmark.cpp" -lm
clang++-18 -O3 -ffast-math -fveclib=ArmPL -L/opt/arm/armpl_24.10_gcc/lib -Wl,-rpath,/opt/arm/armpl_24.10_gcc/lib -lamath -I"$LLVM_PASS_DIR/demo/filament/filament/include" -I"$LLVM_PASS_DIR/demo/filament/libs/math/include" -I"$LLVM_PASS_DIR/demo/filament/libs/utils/include" -I"$LLVM_PASS_DIR/demo/stb" -std=c++17 -o filament_armpl "$LLVM_PASS_DIR/demo/filament/filament_resize_benchmark.cpp" -lm
clang++-18 -O3 -ffast-math -Xclang -fpass-plugin="$LLVM_PASS_DIR/build/ImgOptPass.so" -fveclib=ArmPL -L/opt/arm/armpl_24.10_gcc/lib -Wl,-rpath,/opt/arm/armpl_24.10_gcc/lib -lamath -I"$LLVM_PASS_DIR/demo/filament/filament/include" -I"$LLVM_PASS_DIR/demo/filament/libs/math/include" -I"$LLVM_PASS_DIR/demo/filament/libs/utils/include" -I"$LLVM_PASS_DIR/demo/stb" -std=c++17 -o filament_pass_armpl "$LLVM_PASS_DIR/demo/filament/filament_resize_benchmark.cpp" -lm

echo "Done."
