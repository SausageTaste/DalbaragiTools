#pragma once

#include <vector>

#include <glm/vec4.hpp>
#include <sung/general/mamath.hpp>

#include "daltools/img/img.hpp"


namespace dal {

    class IImage2D : public IImage {

    public:
        virtual uint32_t width() const = 0;
        virtual uint32_t height() const = 0;

        /**
         * Channels per pixel.
         *  - 1 = R
         *  - 2 = RG
         *  - 3 = RGB
         *  - 4 = RGBA
         */
        virtual uint32_t channels() const = 0;

        /**
         * sizeof(T)
         *  - 1 = `uint8_t`
         *  - 4 = `float`
         */
        virtual uint32_t value_type_size() const = 0;
    };


    template <typename T>
    class TImage2D : public IImage2D {

    public:
        // `x` is in range [0, width), `y` is in range [0, height)
        virtual const T* texel_ptr(int x, int y) const = 0;
    };

}  // namespace dal
