// C ABI implementation — a thin translation over the C++ core. No algorithm here.
#include <semper/semper_c.h>

#include <semper/io.hpp>
#include <semper/pipeline.hpp>
#include <semper/version.hpp>
#include <opencv2/core.hpp>

#include <new>

using Semper::pipeline::CancelToken;
using Semper::pipeline::FullFieldParams;
using Semper::pipeline::ReferenceCache;

// One engine instance owns a reference cache and its own cancel token, so several
// instances in a process cancel independently (see ENGINE_APP_CONTRACT A.2).
struct semper_engine {
    ReferenceCache cache;
    CancelToken cancel;
};

extern "C" {

SEMPER_C_API semper_engine* semper_create(void) {
    return new (std::nothrow) semper_engine();
}

SEMPER_C_API void semper_destroy(semper_engine* eng) {
    delete eng;
}

SEMPER_C_API int semper_set_reference(semper_engine* eng,
                         const uint8_t* img, size_t img_len,
                         const uint8_t* mask, size_t mask_len,
                         int expected_w, int expected_h) {
    if (eng == nullptr || img == nullptr || img_len == 0) return SEMPER_ERR_INIT;
    cv::Mat ref = Semper::io::decode_gray(img, img_len, expected_w, expected_h);
    if (ref.empty()) return SEMPER_ERR_INIT;
    cv::Mat roi;
    if (mask != nullptr && mask_len > 0) {
        roi = Semper::io::decode_gray(mask, mask_len, ref.cols, ref.rows);
    }
    eng->cache.set_from_gray(ref, roi);
    return 0;
}

SEMPER_C_API int semper_run(semper_engine* eng,
               const uint8_t* def, size_t def_len,
               const uint8_t* mask, size_t mask_len,
               const semper_params* params,
               float* out, int out_capacity,
               float* metrics, int metrics_len,
               semper_progress_cb cb, void* user) {
    if (eng == nullptr || def == nullptr || def_len == 0 || params == nullptr || out == nullptr)
        return SEMPER_ERR_INIT;
    // Decode against the cached reference's dimensions, matching the Android path.
    cv::Mat def_gray = Semper::io::decode_gray(def, def_len, eng->cache.width, eng->cache.height);
    if (def_gray.empty()) return SEMPER_ERR_INIT;
    cv::Mat roi;
    if (mask != nullptr && mask_len > 0) {
        roi = Semper::io::decode_gray(mask, mask_len, eng->cache.width, eng->cache.height);
    }

    FullFieldParams p;
    p.rect_x = params->rect_x;
    p.rect_y = params->rect_y;
    p.rect_w = params->rect_w;
    p.rect_h = params->rect_h;
    p.step = params->step;
    p.subset_size = params->subset_size;
    p.strain_window = params->strain_window;
    p.use_6x6_interpolator = params->use_6x6_interpolator != 0;

    Semper::pipeline::ProgressCallback progress;
    if (cb != nullptr) {
        progress = [cb, user](int pct) { cb(pct, user); };
    }

    return Semper::pipeline::run_full_field(
        eng->cache, def_gray, roi, p, out, out_capacity,
        metrics, metrics_len, eng->cancel, progress);
}

SEMPER_C_API void semper_cancel(semper_engine* eng) {
    if (eng != nullptr) eng->cancel.request();
}

SEMPER_C_API const char* semper_version(void) {
    return SEMPER_VERSION_STRING;
}

} // extern "C"
