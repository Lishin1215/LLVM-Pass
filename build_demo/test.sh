#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PERF="/usr/lib/linux-tools-6.8.0-86/perf"

TEST_IMG="$SCRIPT_DIR/test_images/bench_4k.png"
IMG_MEDIUM="$SCRIPT_DIR/test_images/rgb-alpha-8.png"

extract_time() { grep "seconds time elapsed" | awk '{print $1}'; }

run_bench() {
    $PERF stat $1 2>&1 | tee /tmp/perf_output.txt
    TIME=$(cat /tmp/perf_output.txt | extract_time)
}

# STB
echo "=== STB ==="
cd "$SCRIPT_DIR/build_stb"
run_bench "./stb_o3 --input-dir $SCRIPT_DIR/test_images --output-dir /tmp"; STB_O3=$TIME
run_bench "./stb_armpl --input-dir $SCRIPT_DIR/test_images --output-dir /tmp"; STB_ARMPL=$TIME
run_bench "./stb_pass_armpl --input-dir $SCRIPT_DIR/test_images --output-dir /tmp"; STB_PASS=$TIME

# Filament
echo "=== Filament ==="
cd "$SCRIPT_DIR/build_filament"
run_bench "./filament_o3 --input-dir $SCRIPT_DIR/test_images --output-dir /tmp"; FIL_O3=$TIME
run_bench "./filament_armpl --input-dir $SCRIPT_DIR/test_images --output-dir /tmp"; FIL_ARMPL=$TIME
run_bench "./filament_pass_armpl --input-dir $SCRIPT_DIR/test_images --output-dir /tmp"; FIL_PASS=$TIME

# libpng
echo "=== libpng ==="
cd "$SCRIPT_DIR/build_libpng"
PNGS="$SCRIPT_DIR/test_images/*.png"
run_bench "./pngstest_o3 $PNGS"; PNG_O3=$TIME
run_bench "./pngstest_armpl $PNGS"; PNG_ARMPL=$TIME
run_bench "./pngstest_pass_armpl $PNGS"; PNG_PASS=$TIME

# Summary
echo ""
echo "=== Summary ==="
printf "%-12s %10s %10s %10s\n" "" "O3" "ArmPL" "Pass+ArmPL"
printf "%-12s %10s %10s %10s\n" "STB" "${STB_O3}s" "${STB_ARMPL}s" "${STB_PASS}s"
printf "%-12s %10s %10s %10s\n" "Filament" "${FIL_O3}s" "${FIL_ARMPL}s" "${FIL_PASS}s"
printf "%-12s %10s %10s %10s\n" "libpng" "${PNG_O3}s" "${PNG_ARMPL}s" "${PNG_PASS}s"
