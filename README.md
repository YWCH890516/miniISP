# miniISP

A C++17 learning project that implements core image-signal-processing stages and two multi-exposure RGB pipelines. The code favors readable reference implementations over hardware-specific optimization.

## Implemented modules

- DNG loading and metadata extraction with LibRaw
- 8-bit binary PPM (`P6`) input/output
- Bayer/CFA utilities for RGGB, BGGR, GRBG, and GBRG
- Black-level correction with one level or four CFA-phase levels
- Synthetic hot/dead/stuck pixels and median-based defective-pixel correction
- Global gray-world AWB and a variant that selects 2x2 Bayer blocks using brightness and channel-spread thresholds
- 8-bit RGGB bilinear / edge-aware demosaic, and 16-bit bilinear demosaic supporting four Bayer patterns
- Flat-field lens-shading map generation and bilinear gain interpolation
- Color-chart patch sampling, neutral-patch white-balance gains, and 3x3 color-correction matrix fitting/application
- sRGB and Adobe RGB transfer functions, and linear Adobe RGB to linear sRGB conversion
- Exposure-time-based radiance merge with Reinhard tone mapping
- Block-adaptive exposure fusion with contrast and exposure-confidence weights

The raw-domain pipeline is conceptually:

```text
DNG -> DPC -> BLC -> LSC -> AWB -> Demosaic -> CCM -> Output color conversion
```

Exact DPC/BLC ordering can vary in a production ISP because defect detection thresholds and black-level calibration may be designed together.

The exposure-time-based HDR and adaptive exposure-fusion experiments are separate RGB pipelines. They are not additional RAW stages in the diagram above. The CLI exposes a subset of the library; color-chart calibration and CCM currently live in the core library and the experimental `main.cpp` workflow.

## Project layout

```text
include/miniISP/   Public C++ interfaces
src/              ISP algorithm implementations
main.cpp          Original notebook-style experiments
tools/            CLI and offline experiment utilities
tests/            Self-contained synthetic regression tests
docs/reports/     Technical notes and controlled experiments
Image/            Local test assets (not distributed)
```

`main.cpp` retains local image paths and calibration examples. Use `miniisp_cli` for commands with your own input/output paths; edit the experiment paths before running `miniISP` directly.

## Dependencies

- CMake 3.20+
- A C++17 compiler
- LibRaw
- `pkg-config`

On macOS with Homebrew:

```sh
brew install libraw pkg-config
```

## Build and test

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

If Apple Command Line Tools selects `MacOSX27.0.sdk` but its linker cannot parse the SDK's `arm64e.x1` entries, select an installed compatible SDK when configuring. For example:

```sh
cmake -S . -B build -DCMAKE_OSX_SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
```

This is a local Apple toolchain mismatch rather than a source-code error.

## Command-line interface

`main.cpp` remains the experimental notebook-style entry point. `miniisp_cli` exposes the stable modules without requiring source edits.

Inspect DNG metadata:

```sh
./build/miniisp_cli inspect-dng input.dng
```

Run the basic RAW pipeline (`DNG -> BLC -> bilinear demosaic -> PPM`):

```sh
./build/miniisp_cli raw input.dng output.ppm
```

Enable additional RAW stages when their calibration data or parameters are available:

```sh
./build/miniisp_cli raw input.dng output.ppm \
  --inject-hot 100 --dpc 500 \
  --flat-field flat_field.dng --lsc-grid 17 13 \
  --awb
```

Useful RAW options:

- `--no-blc`: skip black-level correction.
- `--black-levels R GR GB B`: use per-CFA-phase BLC instead of the DNG's single level.
- `--dpc THRESHOLD`: enable DPC.
- `--inject-hot COUNT`: inject deterministic synthetic hot pixels for testing.
- `--flat-field FILE.dng`: build and apply an LSC map from a matching flat field.
- `--lsc-nearest`: select the V1 cell-based LSC instead of bilinear V2.
- `--awb`: enable the 16-bit neutral gray-world AWB.

Exercise the 8-bit RGGB simulation and demosaic variants:

```sh
./build/miniisp_cli bayer-demo input.ppm output.ppm \
  --awb neutral --demosaic edge-v2
```

Available demosaic names are `basic`, `pointer`, `optimal`, `edge`, and `edge-v2`. The AWB names are `none`, `gray`, and `neutral`.

### RGB multi-exposure demos

Exposure-time-based radiance HDR (times are in the same unit):

```sh
./build/miniisp_cli hdr low.ppm mid.ppm high.ppm output.ppm 0.1666667 1.3 5
```

Adaptive exposure fusion:

```sh
./build/miniisp_cli fusion low.ppm mid.ppm high.ppm output.ppm 8 8
```

All three inputs must be aligned, 8-bit `P6` PPM images with identical dimensions. The radiance path assumes sRGB-encoded inputs, converts them to linear light, divides by exposure time, merges them, and tone-maps the result. The adaptive path is exposure fusion in the encoded RGB domain; it is not a physically calibrated radiance reconstruction.

## Current limitations and next work

- This is an offline learning/portfolio project, not a real-time or production ISP implementation.
- Edge-aware demosaic is currently 8-bit RGGB only; the 16-bit RAW path uses bilinear demosaic.
- Demosaic edge handling clamps coordinates and therefore does not preserve CFA parity at the boundary.
- The DPC implementation is a simple local outlier/median method and does not yet include a calibrated defect map or directional replacement.
- LSC calibration currently uses the center grid cell as its reference and needs stronger flat-field validation and gain limits for production use.
- RGB exposure fusion assumes pre-aligned frames and does not yet perform motion compensation or deghosting.
- CCM fitting and output transfer functions are implemented. Independent color-chart validation, broader RAW testing, robust gamut handling, and performance benchmarks remain future work.
- AWB block selection is heuristic: low channel spread before white balance does not guarantee a physically neutral object. Dominant scene colors can bias both AWB variants.

## Experiments

The [demosaic false-color report](docs/reports/demosaic_false_color/README.md) compares the existing 16-bit bilinear reconstruction with an offline chroma-median experiment on synthetic patterns. It documents the trade-off between suppressing false color and losing genuine color detail; the post-filter is not part of the production library API.

The CTest suite uses synthetic inputs and does not require the local image collection. Report-generation scripts may additionally require local images and a macOS rendering toolchain, as described in their own README.

## Test data

Large DNGs, the local `Image/` collection, build outputs, and personal career documents are excluded by `.gitignore`. The technical report includes small generated illustrations and selected crops; its README records their provenance. Keep input metadata such as exposure times alongside local test files so results remain reproducible.
