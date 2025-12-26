#!/bin/bash
set -e

BUILD_DIR="/home/ubuntu/llvm-pass-clean/build_demo"
PERF="/usr/lib/linux-tools-6.8.0-86/perf"

# STB
echo "=== STB ==="
cd "$BUILD_DIR/build_stb"
$PERF stat ./stb_o3 --input-dir "$BUILD_DIR/test_images" --output-dir /tmp
$PERF stat ./stb_armpl --input-dir "$BUILD_DIR/test_images" --output-dir /tmp
$PERF stat ./stb_pass_armpl --input-dir "$BUILD_DIR/test_images" --output-dir /tmp

# Filament
echo "=== Filament ==="
cd "$BUILD_DIR/build_filament"
$PERF stat ./filament_o3 --input-dir "$BUILD_DIR/test_images" --output-dir /tmp
$PERF stat ./filament_armpl --input-dir "$BUILD_DIR/test_images" --output-dir /tmp
$PERF stat ./filament_pass_armpl --input-dir "$BUILD_DIR/test_images" --output-dir /tmp

# libpng
echo "=== libpng ==="
cd "$BUILD_DIR/build_libpng"
$PERF stat ./pngstest_o3 "$BUILD_DIR"/test_images/*.png
$PERF stat ./pngstest_armpl "$BUILD_DIR"/test_images/*.png
$PERF stat ./pngstest_pass_armpl "$BUILD_DIR"/test_images/*.png
