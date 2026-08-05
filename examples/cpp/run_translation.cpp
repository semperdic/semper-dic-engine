/*
 * Beginner example — DICe custom_app translation contract (verified).
 *
 * Samples: examples/samples/translation/{ref,def}.tif
 * Same assertion as tests/dice/test_translation_real_image.cpp:
 *   four subsets, size 27, |u - 0.4| <= 0.1 px
 *
 * Build (from engine repo root):
 *   cmake -S . -B build/sdk -DCMAKE_BUILD_TYPE=Release -DSEMPER_BUILD_EXAMPLES=ON
 *   cmake --build build/sdk --target run_translation
 *   ./build/sdk/examples/cpp/run_translation \
 *       examples/samples/translation/ref.tif \
 *       examples/samples/translation/def.tif
 */
#include <semper/image.hpp>
#include <semper/io.hpp>
#include <semper/solver.hpp>
#include <semper/subset.hpp>
#include <semper/version.hpp>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::vector<uint8_t> read_file(const char* path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    in.seekg(0, std::ios::end);
    const auto n = static_cast<size_t>(in.tellg());
    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(n);
    if (n && !in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(n))) {
        return {};
    }
    return buf;
}

}  // namespace

int main(int argc, char** argv) {
    const char* ref_path = (argc > 1) ? argv[1] : "examples/samples/translation/ref.tif";
    const char* def_path = (argc > 2) ? argv[2] : "examples/samples/translation/def.tif";

    std::printf("semper %s — DICe translation demo (subset solver)\n", SEMPER_VERSION_STRING);
    std::printf("  ref: %s\n  def: %s\n", ref_path, def_path);

    const auto ref_bytes = read_file(ref_path);
    const auto def_bytes = read_file(def_path);
    if (ref_bytes.empty() || def_bytes.empty()) {
        std::fprintf(stderr, "failed to read input images\n");
        return 1;
    }

    cv::Mat ref_gray = Semper::io::decode_gray(ref_bytes.data(), ref_bytes.size());
    cv::Mat def_gray = Semper::io::decode_gray(def_bytes.data(), def_bytes.size());
    if (ref_gray.empty() || def_gray.empty()) {
        std::fprintf(stderr, "decode_gray failed (is OpenCV imgcodecs linked?)\n");
        return 1;
    }
    if (ref_gray.cols != 512 || ref_gray.rows != 512) {
        std::fprintf(stderr, "unexpected size %dx%d (want 512x512)\n",
                     ref_gray.cols, ref_gray.rows);
        return 1;
    }

    Semper::Image ref(ref_gray.cols, ref_gray.rows, ref_gray.ptr<uint8_t>());
    ref.prepare_data(false);
    Semper::Image def(def_gray.cols, def_gray.rows, def_gray.ptr<uint8_t>());
    def.prepare_data(false);

    // DICe custom_app subsets.txt coordinates + published truth.
    constexpr int kSubset = 27;
    constexpr float kUTrue = 0.4f;
    constexpr float kTol = 0.1f;
    const int points[4][2] = {{100, 100}, {200, 200}, {300, 300}, {400, 400}};

    int solved = 0;
    for (const auto& p : points) {
        Semper::SubsetData subset;
        Semper::SubsetPrecomputer::precompute_subset(subset, ref, p[0], p[1], kSubset);
        if (!subset.is_initialized) {
            std::fprintf(stderr, "  (%d,%d): subset init failed\n", p[0], p[1]);
            continue;
        }

        Semper::OptimizationEngine engine;
        Semper::AnalysisResult res = engine.calculate_deformation(
            subset, def, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, Semper::INIT_NO_SIMPLEX);

        std::printf("  (%3d,%3d): status=%d  u=%+.4f  v=%+.4f\n",
                    p[0], p[1], res.status, static_cast<double>(res.u),
                    static_cast<double>(res.v));

        if (res.status != 0) continue;
        if (std::fabs(res.u - kUTrue) > kTol) {
            std::fprintf(stderr, "FAIL: |u-%.1f|=%.4f exceeds %.1f px\n",
                         kUTrue, static_cast<double>(std::fabs(res.u - kUTrue)), kTol);
            return 1;
        }
        ++solved;
    }

    if (solved != 4) {
        std::fprintf(stderr, "FAIL: solved %d/4 subsets\n", solved);
        return 1;
    }

    std::printf("OK — DICe custom_app contract: 4/4 subsets |u-0.4| <= 0.1 px\n");
    return 0;
}
