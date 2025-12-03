#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEMO_DIR="${SCRIPT_DIR}/demo"

echo "=== Creating demo directory ==="
mkdir -p "$DEMO_DIR"
cd "$DEMO_DIR"

# 1. Download STB
echo "=== Downloading STB ==="
if [ ! -d "stb" ]; then
    git clone --depth 1 https://github.com/nothings/stb.git
else
    echo "STB already exists, skipping..."
fi

# 2. Download libpng
echo "=== Downloading libpng ==="
if [ ! -d "libpng" ]; then
    git clone --depth 1 https://github.com/pnggroup/libpng.git
    cd libpng
    # Build libpng
    ./autogen.sh
    ./configure
    make -j$(nproc)
    cd ..
else
    echo "libpng already exists, skipping..."
fi

# 3. Download Filament
echo "=== Downloading Filament ==="
if [ ! -d "filament" ]; then
    git clone --depth 1 https://github.com/google/filament.git
else
    echo "Filament already exists, skipping..."
fi

# 4. Copy example files to corresponding library folders
echo "=== Copying example files ==="
EXAMPLE_DIR="${SCRIPT_DIR}/example"

# Copy STB benchmark to stb folder
if [ -f "${EXAMPLE_DIR}/stb_resize_benchmark.c" ]; then
    cp "${EXAMPLE_DIR}/stb_resize_benchmark.c" "${DEMO_DIR}/stb/"
    echo "Copied stb_resize_benchmark.c to stb/"
fi

# Copy Filament benchmark to filament folder
if [ -f "${EXAMPLE_DIR}/filament_resize_benchmark.cpp" ]; then
    cp "${EXAMPLE_DIR}/filament_resize_benchmark.cpp" "${DEMO_DIR}/filament/"
    echo "Copied filament_resize_benchmark.cpp to filament/"
fi

echo "=== Done! ==="
echo "Demo libraries downloaded to: $DEMO_DIR"
