#include "daltools/img/backend/ktx.hpp"


namespace dal {

    KtxImage::KtxImage(const uint8_t* data, size_t size) {
        const auto res = ktxTexture_CreateFromMemory(data, size, 0, &texture_);

        if (KTX_SUCCESS != res) {
            this->destroy();
            return;
        }
    }

    KtxImage::KtxImage(KtxImage&& rhs) { std::swap(texture_, rhs.texture_); }

    KtxImage& KtxImage::operator=(KtxImage&& rhs) {
        std::swap(texture_, rhs.texture_);
        return *this;
    }

    void KtxImage::destroy() {
        if (nullptr != texture_) {
            ktxTexture_Destroy(texture_);
            texture_ = nullptr;
        }
    }

    bool KtxImage::is_ready() const { return nullptr != texture_; }

}  // namespace dal
