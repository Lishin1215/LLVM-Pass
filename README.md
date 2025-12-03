# ImgOptPass - LLVM Optimization Pass for Image Processing

An LLVM compiler pass that optimizes gamma correction operations (`pow(x, gamma)`) commonly used in image processing applications. The pass transforms these operations into vectorization-friendly `exp(gamma * log(x))` form, enabling better SIMD optimization when combined with Arm Performance Libraries (ArmPL).

## Overview

Image processing libraries frequently perform gamma correction using `pow(x, gamma)` operations for sRGB ↔ linear color space conversions. This pass:

1. **Detects** `pow()` calls with common gamma values (2.2, 1/2.2, 2.4, 1/2.4, etc.)
2. **Transforms** `pow(x, gamma)` → `exp(gamma * log(x))`
3. **Enables** LLVM's loop vectorizer to generate efficient SIMD code
4. **Leverages** ArmPL's optimized `exp()` and `log()` implementations

## Requirements

- LLVM/Clang 18
- CMake 3.15+
- Arm Performance Libraries (ArmPL) 24.10+
- Linux on AArch64 (ARM64)

## Building

### 1. Build the LLVM Pass

```bash
# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake ..

# Build
make

# The pass will be at: build/ImgOptPass.so
```

### 2. Download Test Libraries

```bash
# Go back to project root
cd ..

# Download STB, libpng, and Filament
./download_library.sh
```

This script will:
- Clone STB, libpng, and Filament into `demo/`
- Build libpng
- Copy benchmark files to the library directories

## Usage

```bash
# Compile with the optimization pass
clang -O3 -ffast-math \
    -Xclang -fpass-plugin=build/ImgOptPass.so \
    -fveclib=ArmPL \
    -I/opt/arm/armpl_24.10_gcc/include \
    -L/opt/arm/armpl_24.10_gcc/lib \
    -Wl,-rpath,/opt/arm/armpl_24.10_gcc/lib \
    -lamath \
    your_code.c -o output -lm
```

## Benchmark Results

Tested on AWS Graviton3 (ARM Neoverse V1), processing 10 PNG images with bilinear resize in linear color space.

### Performance Summary

| Library  | -O3 -ffast-math | -O3 -ffast-math + ArmPL | -O3 -ffast-math + ArmPL + Pass | Speedup |
|----------|-----------------|-------------------------|--------------------------------|---------|
| STB      | 3.11s | 2.92s | **2.66s** | **14.7%** |
| Filament | 2.83s | 2.57s | **2.40s** | **15.4%** |
| libpng   | 39.57s | 39.70s | **37.86s** | **4.3%** |

### Detailed Metrics

#### STB Image Library
| Metric | -O3 -ffast-math | -O3 -ffast-math + ArmPL | -O3 -ffast-math + ArmPL + Pass |
|--------|-----------------|-------------------------|--------------------------------|
| Time | 3.11s | 2.92s | 2.66s |
| Cycles | 7.73B | 7.26B | 6.61B |
| Instructions | 23.4B | 20.9B | 19.6B |
| IPC | 3.02 | 2.87 | 2.97 |

#### Filament (Google's PBR Engine)
| Metric | -O3 -ffast-math | -O3 -ffast-math + ArmPL | -O3 -ffast-math + ArmPL + Pass |
|--------|-----------------|-------------------------|--------------------------------|
| Time | 2.83s | 2.57s | 2.40s |
| Cycles | 7.04B | 6.39B | 5.95B |
| Instructions | 21.8B | 18.0B | 16.9B |
| IPC | 3.09 | 2.81 | 2.84 |

#### libpng
| Metric | -O3 -ffast-math | -O3 -ffast-math + ArmPL | -O3 -ffast-math + ArmPL + Pass |
|--------|-----------------|-------------------------|--------------------------------|
| Time | 39.57s | 39.70s | 37.86s |
| Cycles | 98.6B | 98.9B | 94.2B |
| Instructions | 301.7B | 299.3B | 291.3B |
| IPC | 3.06 | 3.03 | 3.09 |

## How It Works

### Transformation

The pass converts scalar `pow()` calls:

```c
// Before: scalar pow (hard to vectorize)
float result = powf(x, 2.2f);

// After: exp/log form (vectorization-friendly)
float result = expf(2.2f * logf(x));
```

### Why This Works

1. **pow() is complex**: The standard `pow(x, y)` function handles many edge cases, making it difficult for compilers to vectorize
2. **exp/log are simpler**: These functions have straightforward SIMD implementations
3. **ArmPL provides optimized SIMD**: ARM Performance Libraries include hand-tuned vector math functions
4. **LLVM can now vectorize**: The transformed code is recognized by LLVM's loop vectorizer

### Supported Gamma Values

- sRGB/Rec.709: 2.2, 1/2.2 (≈0.4545)
- Adobe RGB: 2.4, 1/2.4 (≈0.4167)
- Apple RGB: 1.8, 1/1.8 (≈0.5556)
- Gamma encoding: 1.2
- CIE Lab: 1/3 (≈0.3333)

## Project Structure

```
.
├── ImgOptPass.cpp          # Main LLVM pass implementation
├── CMakeLists.txt          # Build configuration
├── download_library.sh     # Script to download test libraries
├── example/
│   ├── stb_resize_benchmark.c
│   └── filament_resize_benchmark.cpp
└── build_evaluation/
    ├── compile.sh          # Compile benchmarks
    └── test.sh             # Run benchmarks
```

## Running Benchmarks

```bash
cd build_evaluation

# Compile all benchmarks
./compile.sh

# Run performance tests
./test.sh
```

## License

MIT License

## References

- [LLVM Pass Documentation](https://llvm.org/docs/WritingAnLLVMPass.html)
- [Arm Performance Libraries](https://developer.arm.com/tools-and-software/server-and-hpc/compile/arm-compiler-for-linux/arm-performance-libraries)
- [STB Image Library](https://github.com/nothings/stb)
- [Google Filament](https://github.com/google/filament)
- [libpng](https://github.com/pnggroup/libpng)
