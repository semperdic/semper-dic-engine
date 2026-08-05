# Test suite reference

Complete catalog of the automated tests: what each one proves, why it exists,
and how to run everything. The engine it exercises is described in
[ARCHITECTURE.md](ARCHITECTURE.md); CI wiring is in
[`.github/workflows/ci.yml`](../.github/workflows/ci.yml).

---

## Running the tests

### Host engine tests (no device / NDK needed)

Built out of the source tree (from the engine repo root) so `tests/` stays pure
sources — the build dir is regenerated on demand and gitignored:

```bash
git submodule update --init --recursive
./scripts/sparse-opencv.sh   # optional; shrinks OpenCV worktree

cmake -S tests -B build/tests -DCMAKE_BUILD_TYPE=Release  # any C++17 compiler
cmake --build build/tests
./build/tests/dic_tests                 # all suites
./build/tests/dic_tests Engine          # one suite
./build/tests/dic_tests Engine.PureTranslation_Subpixel   # one test
```

Dependencies come from the git submodules (`git submodule update --init`):
Eigen (`third_party/eigen`) and OpenCV's universal-intrinsics headers
(`third_party/opencv/modules/core/include`). OpenCV's generated
`opencv2/opencv_modules.hpp` + `cvconfig.h` are committed under
`tests/shim/opencv2/` so the host build needs no OpenCV configure.
Logging uses portable `src/util/log.hpp` (stderr on host; Android log on device).

### C ABI smoke

```bash
cmake -S . -B build/sdk -DSEMPER_BUILD_C_SDK=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/sdk --target semper_c semper_c_smoke
# Ensure the shared lib is on the loader path (Windows: adapters/c next to PATH)
./build/sdk/bin/semper_c_smoke   # path may vary by generator
```

### Python smoke

```bash
pip install ./bindings/python
pytest bindings/python/tests
```

> Host math/DICe tests compile with `-ffast-math` deliberately — the same
> floating-point model as the production library, so numerical regressions
> surface here first.

---

## The synthetic-deformation methodology

The engine suite's ground truth is **exact by construction**, not rendered:

1. A continuous speckle function `g(x,y)` (seeded sum of ~400 Gaussian blobs,
   `framework/synthetic.h`) is sampled to create the reference image.
2. The deformed image is created by sampling `g` at the **analytic inverse**
   of the affine warp: `def(q) = g(c + A⁻¹(q − c − t))`.

No image resampling is involved, so the true 6-DOF parameters are known to
machine precision, and every error the engine reports is the engine's own.
The warp convention matches the engine's shape function exactly
(see ARCHITECTURE.md → "Warp convention").

**Tolerances** (literature-standard for ICGN + bicubic on smooth speckle):
translation ≤ 0.02 px, displacement gradients ≤ 2×10⁻³.

---

## Numerical reproducibility contract

What "the same result" means across builds, and when a difference is a bug.

**Same APK, same device, same inputs → bit-identical results.**
Guarded by `Engine.RepeatSolve_BitIdentical`. Any run-to-run variation on
identical inputs is a defect (threading race, uninitialized memory).

**Different builds / ABIs / dependency versions → small drift is expected.**
The engine compiles with `-ffast-math` and uses FMA-based SIMD reductions, so
any change to the compiler, NDK, OpenCV build (e.g. Carotene on ARM vs the
generic path), Eigen version, or kernel summation order legally perturbs
floating-point rounding. Because ICGN is iterative, last-bit differences per
iteration shift the convergence path. Empirically (verified across the
prebuilt-SDK → from-source OpenCV migration, ARM NEON → portable SIMD):

| Quantity | Expected cross-build agreement |
|---|---|
| Displacements (U, V) | ≤ ~1×10⁻⁴ px (typically identical to 5 decimals) |
| Strains (Exx, Eyy, Exy) | ≤ ~1 µε (0.001 mε) |
| Solver stats (solved/dead counts, convergence %) | identical |
| Report max/min **locations** | may hop between near-tied grid points |

Anything beyond this — values off in the first or second significant digit,
extrema in unrelated regions, changed dead-point counts on the same input —
is a real regression: bisect with the `Engine` suite per-ABI.

Trade-off note: `-ffast-math` makes results build-specific by design. If
bit-reproducibility across builds ever becomes a requirement, compile `core/`
and `preprocessing/` with `-fno-fast-math` and re-benchmark; the explicit SIMD
kernels already do the heavy lifting, so the expected cost is small.

---

## Suite: `Engine` — `integration/test_optimization_engine.cpp`

The synthetic deformation regression suite. Covers `OptimizationEngine`
end-to-end (ICGN, Simplex, auto-search, guards).

| Test | Proves | Failure would mean |
|---|---|---|
| `ZeroDeformation_RecoversZero` | Identity warp → (0,0), ZNSSD ≈ 0 | Broken normalization or warp math — nothing else can be trusted |
| `PureTranslation_IntegerPixel` | Integer shift recovered from an exact guess | Interpolation at integer coords broken |
| `PureTranslation_Subpixel` | **The canonical DIC benchmark.** (2.25, −1.50) px recovered from integer guess; gradients ≈ 0 | Sub-pixel interpolation, gradient, or Hessian regression |
| `PureTranslation_SubpixelPhaseSweep` | 6 fractional phases (0.1…0.9) all recovered | Phase-dependent interpolation bias (a single phase can hide it) |
| `UniaxialStrain_Recovered` | 1% exx strain recovered, other DOFs ≈ 0 | Gradient-DOF columns of steepest-descent images broken |
| `SimpleShear_Recovered` | uy shear recovered without cross-talk | uy/vx column swap or sign error |
| `General6DOF_Recovered` | Translation + all four gradients simultaneously | Cross-DOF coupling errors invisible in single-DOF tests |
| `MultiSubsetGrid_ConsistentRigidTranslation` | DICe-style field consistency: a 3×3 grid of POIs all recover the same rigid translation (0.03 px grid tolerance — see the in-test note on per-POI speckle variance) | Position-dependent defect: coordinate-origin mistake, asymmetric boundary handling, or a row-stride bug invisible to the center-only tests |
| `AutoSearch_FindsLargeTranslation` | (6.4, −8.3) px found with NO initial guess | Coarse SSD search / Simplex / hand-off between stages broken |
| `ZnssdInvariantToBrightnessAndContrast` | Solution unchanged under `I' = 0.7·I + 30` | ZNSSD normalization broken — real-world lighting robustness lost |
| `SubsetOffImage_ReportsFailureStatus` | Impossible warp → `status != 0` | Engine fabricates answers instead of failing (silent corruption) |
| `BothInterpolatorsConverge` | Bicubic AND Keys 6×6 paths both solve | One interpolator selector path regressed |
| `LmDampingPreservesWellPosedSolution` | LM damping (α=1e-3) doesn't shift a good solution | LM applied to wrong DOFs or damping leaking into the answer |
| `RepeatSolve_BitIdentical` | The same solve twice is bit-identical | Threading race, uninitialized buffer, or run-to-run nondeterminism |
| `SuccessfulSolve_CorrelationNonNegative` | Successful solves report ZNSSD ≥ 0 | Sentinel contract broken — the JNI layer marks failed/skipped points with `CORR_INVALID = -1`, so a real score must never be negative |

## Suite: `DiceTranslationSynthetic` — `dice/test_translation_synthetic.cpp`

Cross-validation against **DICe** (Digital Image Correlation Engine,
[github.com/dicengine/dice](https://github.com/dicengine/dice), BSD 3-Clause),
an established reference implementation. Reproduces the **input→output
contract** of DICe's `tests/examples/custom_app` — a rigid 0.4 px
X-translation, subset size 27, four subsets, each recovered within DICe's
`errorTol = 0.1 px` — on our engine and our **analytic synthetic** images. No
DICe code is used; only the published scenario and tolerance. It shows our ICGN
solver matches a reference DIC engine's accuracy on the canonical translation
case, in milliseconds on a laptop (no Trilinos/MPI).

| Test | Proves | Failure would mean |
|---|---|---|
| `PureTranslation_0p4px_FourSubsets` | All 4 subsets recover the 0.4 px shift within DICe's 0.1 px tolerance (we actually land ≤ 0.02 px — see `Engine.PureTranslation_Subpixel`) | Our engine no longer agrees with a reference DIC implementation on rigid translation — a correlation/interpolation regression, or accuracy fallen below the field's accepted bar |

## Suite: `DiceTranslationReal` — `dice/test_translation_real_image.cpp`

The real-image companion to `DiceTranslationSynthetic`: same 0.4 px / 0.1 px contract, but on
DICe's **actual** 512×512 speckle images (`fixtures/dice/ref.tif`, `def.tif` —
their `custom_app` `ref.tif`/`def.tif`, BSD-3, see
[`fixtures/dice/LICENSE.DICe`](../tests/fixtures/dice/LICENSE.DICe)).
Where `DiceTranslationSynthetic` proves accuracy on math-perfect synthetic texture, this adds
**real-speckle robustness** and **independence** — an image we did not generate,
a target we did not compute. The fixtures path is injected by CMake as
`DICE_FIXTURES_DIR`; a minimal P5 reader loads the PGMs (the host build has no
image codec).

| Test | Proves | Failure would mean |
|---|---|---|
| `CustomApp_0p4px_RealSpeckle` | All 4 subsets recover the 0.4 px X-shift on DICe's real images, within DICe's 0.1 px tolerance | Our engine disagrees with DICe on their own experimental data — a real-texture/robustness gap the synthetic tests don't expose |

## Suite: `SimdKernels` — `unit/test_simd_kernels.cpp`

Every kernel is checked against a **double-precision scalar oracle**. On an
SSE/NEON host this validates the vectorized path; on other compilers, the
scalar fallback.

| Test | Proves |
|---|---|
| `SumSqDiff_AllSizes` | Exact for n = 1…1000, covering full vector blocks, ragged tails, and n < lane-width |
| `SumSqDiff_ZeroWhenAllEqualMean` | No catastrophic cancellation on the degenerate case |
| `Znssd_AllSizes` | ZNSSD reduction matches oracle across sizes |
| `Znssd_PerfectMatchIsZero` | Perfectly correlated signals score ≈ 0 (the convergence anchor) |
| `ErrorAndGradient_MatchesScalarOracle` | Fused kernel: error AND all 6 SoA gradient projections match |
| `FusedErrorEqualsStandaloneZnssd` | Contract: fused error term ≡ standalone `znssd_sum` (callers assume it) |

## Suite: `Image` — `unit/test_image.cpp`

Interpolation ladder + gradients + blur, validated via mathematical identities
(no golden files).

| Test | Property exploited |
|---|---|
| `InterpolatorsReproducePixelValuesAtIntegerCoords` | All three kernels are *interpolating*: weight 1 at s=0, 0 at other integers |
| `InterpolatorsExactOnLinearRamp` | Catmull-Rom & Keys reproduce degree-1 polynomials exactly at any sub-pixel position |
| `GradientOfLinearRampIsExactSlope` | 5-point central difference is exact for degree ≤ 4 |
| `BlurPreservesConstantImage` | Normalized kernel ⇒ constant in = constant out |
| `BilinearOutOfBoundsReturnsZero` | The `0.0f` dead-pixel sentinel contract that ICGN's `val > 0` guard relies on |
| `BoundaryDemotionLadderIsContinuousInRange` | 6×6→bilinear and 4×4→bilinear demotion never extrapolates outside [0,255] |

## Suite: `SubsetPrecomputer` — `unit/test_subset_precomputer.cpp`

| Test | Proves |
|---|---|
| `MeanAndStdMatchManualComputation` | Stats vs a double-precision manual computation; normalized intensities are zero-mean/unit-RMS |
| `SdiPlanesMirrorSteepestDescentImages` | **The SIMD-refactor regression guard**: SoA `sdi_planes` ≡ AoS `steepest_descent_images` element-wise. The SIMD fast path reads planes, the masked path reads vectors — drift silently corrupts solutions |
| `SteepestDescentImagesFollowDefinition` | `sd = [gx, gy, gx·x, gx·y, gy·x, gy·y]/σ` at spot-checked pixels |
| `HessianIsSymmetricAndInverseIsValid` | `H = Hᵀ` and `H·H⁻¹ ≈ I` |
| `RejectsSubsetOffImageBoundary` | Boundary subsets → `is_initialized == false` (never solved) |
| `FastPathMatchesFullPrecompute` | `compute_hessian_only` + `precompute_subset_fast` ≡ full `precompute_subset` — the batch pipeline treats them as interchangeable |

## Suite: `Strain` — `unit/test_strain_calculator.cpp`

Linear displacement fields have closed-form Green-Lagrange strain, and VSG's
linear least-squares fits them **exactly** — so interior tolerances are float
precision, not "close enough".

| Test | Proves |
|---|---|
| `VsgRecoversUniaxialStrainExactly` | 1% exx from u = 0.01·X, exact at grid center |
| `VsgRecoversGeneralLinearField` | All three strain components for a general 4-coefficient field, at multiple interior points |
| `VsgRigidBodyTranslationGivesZeroStrain` | Constant displacement → zero strain (the classic false-strain bug) |
| `VsgLeavesSentinelWhereWindowUnsupported` | Corner points (<90% window fill) and invalidated points keep the `−1000` sentinel |

## Suite: `CancelToken` — `unit/test_cancel_token.cpp`

The flag `run_full_field` polls to stop a solve mid-frame. The polling itself
needs a real solve (OpenCV, threads) and is covered on the Android side; what is
pinned here is the contract those polls rest on.

| Test | Proves |
|---|---|
| `StartsClear` / `RequestIsObserved` | The flag reads back what was set |
| `StaysSetUntilCleared` | Sticky across a thousand polls — the caller must clear it before a run, which is why both run paths assign `false` on entry |
| `CrossesThreads` | A worker spinning on the flag sees a request made from another thread — the flag is set on the UI thread, read by solver workers |
| `CancelledCodeIsDistinctFromEngineErrors` | `kCancelled` is −99, matching `AnalysisRunCodes.ERROR_CANCELLED`, and is neither the −2 ROI nor the −3 init failure |

`cancel.cpp` is deliberately its own translation unit with no OpenCV in it, so
the host suite can link it without the rest of the pipeline.

## Downstream app tests (informative)

Kotlin JVM unit tests, instrumented JNI smokes, and backend contract tests live
in the **private** application repository that consumes this engine as a
submodule. They are not part of this tree. This engine repo owns algorithmic
correctness; the app owns bridge/orchestration contracts.

---

## Test layout

Host tests live under `tests/`, grouped by scope:

```
tests/
  test_main.cpp           micro-framework runner entry point
  framework/              the test harness — synthetic.h, test_framework.h
  shim/                   host stand-ins for OpenCV configs
  c/smoke.c               C ABI smoke (built with SEMPER_BUILD_C_SDK)
  unit/                   one component vs. an oracle / mathematical identity
                            test_simd_kernels, test_image,
                            test_subset_precomputer, test_strain_calculator,
                            test_cancel_token
  integration/            the assembled engine end-to-end + robustness
                            test_optimization_engine, test_robustness
  dice/                   DICe golden comparisons
  perf/                   throughput gates
```

`tests/CMakeLists.txt` lists sources under `DIC_UNIT_TESTS` /
`DIC_INTEGRATION_TESTS` / `DIC_DICE_TESTS` / `DIC_PERF_TESTS`.

## Adding a new test

1. Pick the suite file, or create `unit/test_<module>.cpp` (component) or
   `integration/test_<module>.cpp` (end-to-end) and add it to the matching
   `DIC_UNIT_TESTS` / `DIC_INTEGRATION_TESTS` list in `CMakeLists.txt`.
2. `TEST_CASE(Suite, Name) { ... }` — use `CHECK`, `REQUIRE`, `CHECK_NEAR`,
   `CHECK_REL` (see `framework/test_framework.h`).
3. For engine tests, build ground truth with `SpeckleField` +
   `AffineDeformation` — never by resampling images, and never with warps that
   don't match the engine's shape-function convention.
4. Document the test's *reason to exist* in this file. A test whose failure
   nobody can interpret is a liability.

## Host vs JNI responsibility split

- **Host C++ tests** (this suite) own algorithmic correctness: displacement
  accuracy, strain math, SIMD equivalence, determinism, and robustness.
- **C ABI / Python smokes** own binding marshalling at the public SDK surface.
- **Downstream Android JNI / JVM tests** (private app) own runtime/bridge
  correctness: `System.loadLibrary`, OpenMP on the Android runtime, and Kotlin
  orchestration. They do not re-assert displacement accuracy here.

See [app/TESTING.md](../app/TESTING.md) for the full workflow-chunk map.

## Known limitations / future work

- **Per-ABI numerical drift**: the host suite runs on x86 SSE. To compare ABIs,
  build the same suite with the NDK toolchain per-ABI and run on devices —
  tolerances are already set to absorb fast-math reassociation differences.
