#pragma once

#include <ktx.h>

#include <daltools/img/img.hpp>


namespace dal {

    class KtxImage : public IImage {

    public:
        KtxImage() = default;
        KtxImage(const uint8_t* data, size_t size);

        KtxImage(const KtxImage&) = delete;
        KtxImage& operator=(const KtxImage&) = delete;
        KtxImage(KtxImage&&);
        KtxImage& operator=(KtxImage&&);

        ~KtxImage() { this->destroy(); }

        void destroy();
        bool is_ready() const;

        ktxTexture* texture_ = nullptr;
    };

}  // namespace dal
