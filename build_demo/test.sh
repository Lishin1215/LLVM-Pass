#!/bin/bash

BUILD_DIR="/home/ubuntu/llvm-pass-clean/build_demo"
PERF="/usr/lib/linux-tools-6.8.0-86/perf"

# STB
echo "=== STB ==="
cd "$BUILD_DIR/build_stb"
#--input-dir "$BUILD_DIR/test_images": 要處理的圖片 的資料夾
$PERF stat ./stb --input-dir "$BUILD_DIR/test_images"
$PERF stat ./stb_pass --input-dir "$BUILD_DIR/test_images"

# Filament
echo "=== Filament ==="
cd "$BUILD_DIR/build_filament"
$PERF stat ./filament --input-dir "$BUILD_DIR/test_images"  
$PERF stat ./filament_pass --input-dir "$BUILD_DIR/test_images" 

# libpng
echo "=== libpng ==="
cd "$BUILD_DIR/build_libpng"
# "$BUILD_DIR"/test_images/*.png: libpng 官方自己設計的 ”讀圖片的方法“
$PERF stat ./pngstest "$BUILD_DIR"/test_images/*.png
$PERF stat ./pngstest_pass "$BUILD_DIR"/test_images/*.png
