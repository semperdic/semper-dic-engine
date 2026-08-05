# Semper DIC Engine

Hybrid Delaunay Digital Image Correlation (DIC) library — portable C++ core,
stable **C ABI**, **Android JNI** adapter, and **Python** bindings.

This repository contains **only the engine**. It has no network stack, auth, or
secrets. Downstream apps link it as a git submodule (or via `find_package(Semper)`).

## Layout

```
├── include/semper/       # public C++ / C headers
├── src/                  # math, seeding, strain, pipeline (Internal)
├── adapters/
│   ├── android/          # JNI → libsemper_core.so
│   └── c/                # stable C ABI → libsemper_c
├── bindings/python/      # pybind11 + scikit-build-core
├── cmake/                # options, OpenCV helpers, package config
├── tests/                # host suite (no NDK) + C smoke
├── docs/CONTRACT.md      # Frozen / Stable API contract
└── third_party/{eigen,opencv}
```

## Build

### Host tests

```bash
git submodule update --init --recursive
./scripts/sparse-opencv.sh          # or scripts/sparse-opencv.ps1 on Windows

cmake -S tests -B build/tests -DCMAKE_BUILD_TYPE=Release
cmake --build build/tests
./build/tests/dic_tests
```

### C SDK

```bash
cmake -S . -B build/sdk -DSEMPER_BUILD_C_SDK=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/sdk --target semper_c semper_c_smoke
./build/sdk/adapters/c/semper_c_smoke   # path may vary by generator
```

Install exports `find_package(Semper)` → `Semper::semper_c`.

### Python

```bash
pip install ./bindings/python
pytest bindings/python/tests
```

Wheels are built with `cibuildwheel` (see `.github/workflows/wheels.yml`).

### Android JNI

Configure with `-DSEMPER_ANDROID=ON`. The shared library `OUTPUT_NAME` is
`semper_core` (`System.loadLibrary("semper_core")`). JNI symbols target
`com.indicvision.semper.SemperNativeLib`.

## CMake targets

| Target | Role |
|---|---|
| `semper_math` | Portable math (no JNI / Android) |
| `semper_pipeline` | Seeding + full-field (OpenCV + OpenMP) |
| `semper` | INTERFACE alias → pipeline |
| `semper_android` | JNI adapter (`OUTPUT_NAME semper_core`) |
| `semper_c` | Stable C ABI shared library |
| `_semper` | Python extension module |

## Options

| Option | Default | Meaning |
|---|---|---|
| `SEMPER_ANDROID` | OFF | Build JNI shared library |
| `SEMPER_BUILD_C_SDK` | OFF | Build `semper_c` + smoke |
| `SEMPER_BUILD_PYTHON` | OFF | Build pybind11 module |
| `SEMPER_BUILD_TESTS` | OFF | Build `dic_tests` via parent project |

## API contract

See [docs/CONTRACT.md](docs/CONTRACT.md). Output packing (**8 floats/point**),
metrics layout (**17 floats**), and return codes are **Frozen**. Touching
`include/semper/*` requires stating the semver tier in the PR.

## License

BSD 3-Clause — see [LICENSE](LICENSE). Derivative of [DICe](https://github.com/dicengine/dice)
(Sandia / NTESS).
