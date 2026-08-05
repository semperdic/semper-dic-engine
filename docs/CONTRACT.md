# Engine API Contract

**Audience:** contributors to this repository.

A private Android application links this engine through the JNI adapter in
`adapters/android/`. That app lives in a different repository and **cannot** be
seen from an engine pull request, so this document is the contract that keeps
the two in sync.

> **The rule in one sentence:** anything marked **Frozen** or **Stable** below is
> load-bearing for a downstream app; change it carelessly and you break a shipping
> product whose code you can't see. The golden contract test (§A.6) is the
> automated backstop; this document is the human one.

You do **not** need the app to contribute. Build and test against `tests/` as
usual. Respect the surface described here, and declare your change's
[tier](#a1-stability-tiers) in the PR.

---

## A.1 Stability tiers

| Tier | What it covers | Rule for contributors |
|---|---|---|
| **Frozen** | Binary/data formats: output packing, metrics layout, error codes, and the *meaning* of existing `FullFieldParams` fields | Never change in place. A change here is a **major** version bump and requires a coordinated change in the downstream app. |
| **Stable** | Public function **signatures** in `include/semper/{pipeline,io,cancel,version}.hpp` and `include/semper/semper_c.h` | May be **extended additively** (new overloads / new functions). Existing signatures, parameter order, defaults, and semantics must not change without a **major** bump. |
| **Additive-only** | Deliberate growth points: trailing `FullFieldParams` fields, unused metrics slots | Append only, each with a default that reproduces prior behavior. Never reorder or repurpose an existing entry. |
| **Internal** | Everything in `src/`, `tuning.hpp` constants, `PointState`, `types.hpp`, algorithm internals, private headers | Change freely. Behavior is guarded by the golden test, not by this contract. |

**Semantic versioning** (`include/semper/version.hpp` / `SEMPER_VERSION`):

- **major** — any Frozen or Stable break.
- **minor** — additive Stable / Additive-only change, or a new capability.
- **patch** — Internal-only improvement (accuracy, speed, robustness) with no surface change.

Downstream apps pin an **exact** engine tag and only auto-adopt minor/patch bumps.

---

## A.2 Public API reference (the app-facing surface)

These are the **only** engine symbols the Android app calls (through the JNI
adapter). Keep the signatures here in sync with the code — they are authoritative.

### `pipeline::run_full_field` — the solve · *Stable; behavior Frozen via golden test*

```cpp
// include/semper/pipeline.hpp
int run_full_field(ReferenceCache& cache,
                   const cv::Mat& def_gray,
                   const cv::Mat& roi_mask,
                   const FullFieldParams& params,
                   float* output_ptr, int output_capacity,
                   float* metrics, int metrics_len,
                   ProgressCallback on_progress = nullptr);

// Additive overload — per-solve cancel for SDK / Python:
int run_full_field(..., CancelToken& token, ProgressCallback on_progress = nullptr);
```

- **Returns** *(Frozen)*: `>= 0` → number of valid output points. `-2` → invalid ROI.
  `-3` → init/argument failure. `-99` (`kCancelled`) → cancelled mid-solve.
- **Writes** *(Frozen)*: at most `output_capacity` floats, **8 per point** (see
  [A.4](#a4-frozen-data-formats)). Points beyond capacity are **dropped, never
  overflow**.
- **Metrics** *(Frozen layout)*: when `metrics != nullptr && metrics_len >= 16`,
  fill telemetry; 17 slots preferred. Slot indices are frozen.
- **Cancellation**: the legacy overload clears the process-global cancel flag on
  entry and polls it inside point loops. The `CancelToken&` overload binds that
  token for the duration of the solve.
- **You may** make it faster or more accurate (Internal). **You may not** change
  return-code meanings, 8-float packing, the capacity-drop rule, or metric slot
  indices without a major bump.

### `pipeline::ReferenceCache` — cached reference state · *Stable*

```cpp
struct ReferenceCache {                 // non-copyable, non-movable
    ReferenceCache();
    void reset();
    void set_from_gray(const cv::Mat& gray_in, const cv::Mat& roi_mask);
    std::string debug_dir;              // "" disables debug export
    // internal members are INTERNAL
};
```

Must remain **non-copyable / non-movable**.

### `pipeline::FullFieldParams` — solve inputs · *Frozen fields + Additive-only tail*

```cpp
struct FullFieldParams {
    int rect_x, rect_y, rect_w, rect_h;
    int step;
    int subset_size;
    int strain_window;
    bool use_6x6_interpolator;
};
```

New tunables must be **appended** with a default that reproduces current behavior.

### `pipeline::ProgressCallback` · *Stable*

```cpp
using ProgressCallback = std::function<void(int percentage)>;   // 0..100
```

### Cancellation — `include/semper/cancel.hpp` · *Stable*

```cpp
constexpr int kCancelled = -99;   // Frozen value
void request_cancel();
void clear_cancel();
bool cancel_requested();
// Plus CancelToken for per-instance cancel (SDK / Python)
```

The free functions stay — JNI `setCancelRequested` maps to them.

### Image I/O — `include/semper/io.hpp` · *Stable*

```cpp
cv::Mat decode_gray(const uint8_t* data, size_t len, int expected_w = 0, int expected_h = 0);
cv::Mat decode_bgr (const uint8_t* data, size_t len);
void    image_dimensions(const uint8_t* data, size_t len, int& out_w, int& out_h);
```

Empty `cv::Mat` = failure; do not switch to throwing.

### C ABI — `include/semper/semper_c.h` · *Stable (ABI)*

`semper_create` / `destroy`, `semper_set_reference`, `semper_run`, `semper_cancel`,
`semper_version`. Same Frozen return codes and 8-float packing.

---

## A.3 The JNI mapping (informative)

JNI exports live in `adapters/android/jni/SemperJNI.cpp` under the package
`com.indicvision.semper.SemperNativeLib`. All OpenMP work must stay on one pinned
thread on Android.

| JNI export | Public engine calls used |
|---|---|
| `JNI_OnLoad` | `cv::setNumThreads(1)` |
| `setDebugOutputDir(String?)` | `ReferenceCache::debug_dir` |
| `setCancelRequested(bool)` | `request_cancel` / `clear_cancel` |
| `getImageDimensions(byte[])` | `io::image_dimensions` |
| `getPreviewFromBytes(byte[], int)` | `io::decode_bgr` |
| `initializeReference(byte[], byte[]?, int, int)` | `io::decode_gray`, `ReferenceCache::set_from_gray` / `reset` |
| `computeFullFieldDirect(…)` | `io::decode_gray`, `run_full_field` |

---

## A.4 Frozen data formats

### Output buffer — packed `float32`, **8 per point**

```
index:  0   1   2   3     4     5     6      7
field:  x   y   u   v    exx   eyy   exy   corr
```

### Metrics buffer — **17 × `float32`**

Slot indices are Frozen; new telemetry appends. `metrics_len == 16` is the
minimum honored; 17 preferred.

### Return / error codes — Frozen

| Value | Meaning |
|---|---|
| `>= 0` | number of valid output points |
| `-2` | invalid ROI |
| `-3` | init / argument failure |
| `-99` (`kCancelled`) | cancelled mid-solve |

---

## A.5 Change checklist

- **Free to change (patch):** correlation math, ICGN, AKAZE, SIMD, threading,
  `tuning.hpp`, anything under `src/` — golden test must still pass.
- **Additive only (minor):** trailing `FullFieldParams` field, new metrics slot,
  new overload/function, new SDK entry point.
- **Do not without major + app coordination:** change any §A.2 signature; change
  8-float packing; change a metric slot's meaning; change return-code values;
  make `decode_*` throw; make `ReferenceCache` copyable/movable.

Every PR that touches `include/semper/*` must state its tier (**patch / minor /
major**) and update this document.

---

## A.6 How improvements reach downstream apps

1. Land the change behind the same public signatures (or additively). Update
   `SEMPER_VERSION` per §A.1.
2. Tag a release. Downstream apps bump their **pinned submodule tag**, review the
   diff, and run a golden contract test through the real entry point.
3. Ship. Source compatibility of `include/semper/*` is what matters for static
   linkers; `semper_c.h` additionally guarantees ABI stability for external
   consumers.
