#ifndef SEMPER_IMAGE_HPP
#define SEMPER_IMAGE_HPP

#include <semper/types.hpp>
#include <vector>
#include <cstdint>

namespace Semper {

    class Image {
    public:
        int_t width, height;
        std::vector<scalar_t> intensities;
        std::vector<scalar_t> grad_x;
        std::vector<scalar_t> grad_y;

        Image(int_t w, int_t h, const uint8_t* raw_pixels);

        // 🚀 ADDED: Accept the UI toggle flag directly
        void prepare_data(bool apply_dice_blur);
        // The new 6x6 DICe Parity Interpolator
        scalar_t interpolate_keys_fourth(scalar_t x, scalar_t y) const;
        // Fast, branchless Keys 4th Order Bicubic
        scalar_t interpolate_bicubic(scalar_t x, scalar_t y) const;
        // The final DICe Parity Fallback
        scalar_t interpolate_bilinear(scalar_t x, scalar_t y) const;

        // Batch-of-4 interior-only variants: same per-point arithmetic, same
        // operation order, as interpolate_keys_fourth/interpolate_bicubic —
        // computed for 4 independent query points together so the compiler
        // can pipeline/vectorize across the 4 lanes instead of the caller
        // making 4 separate calls. No lane reads or writes another lane's
        // data, so this changes nothing about any single point's summation.
        //
        // PRECONDITION (caller's responsibility, not checked here): every one
        // of the 4 points must already be within the interior zone that
        // never triggers bilinear demotion (x/y comfortably inside
        // [~3.5, dim-4.5] — see optimization_engine.cpp's px/py guard, which
        // is strictly tighter than interpolate_keys_fourth's own [2.5, dim-3.5]
        // demotion boundary and therefore already guarantees this before
        // these are ever called). Calling with a point outside that zone is
        // undefined (it skips the demotion check these batch variants don't
        // implement) — do not call this from a context that hasn't already
        // enforced an equivalent-or-tighter guard.
        void interpolate_keys_fourth_x4(const scalar_t x[4], const scalar_t y[4], scalar_t out[4]) const;
        void interpolate_bicubic_x4(const scalar_t x[4], const scalar_t y[4], scalar_t out[4]) const;
    };

}
#endif