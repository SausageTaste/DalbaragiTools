#pragma once

#include <optional>

#include <ktx.h>
#include <glm/glm.hpp>

#include "daltools/img/img.hpp"


namespace dal {

    class KtxImage : public IImage {

    public:
        KtxImage() = default;
        KtxImage(const uint8_t* data, size_t size);

        KtxImage(const KtxImage&) = delete;
        KtxImage& operator=(const KtxImage&) = delete;
        KtxImage(KtxImage&&) = delete;
        KtxImage& operator=(KtxImage&&) = delete;

        ~KtxImage() { this->destroy(); }

        void destroy() override;
        bool is_ready() const override;

        uint32_t base_width() const;
        uint32_t base_height() const;

        bool need_transcoding() const;
        bool transcode(ktx_transcode_fmt_e fmt, ktx_transcode_flags flags = 0);

        std::optional<glm::tvec4<uint8_t>> get_base_pixel(
            uint32_t x, uint32_t y
        ) const;

        ktxTexture& ktx();
        const ktxTexture& ktx() const;
        const ktxTexture1* ktx1() const;
        const ktxTexture2* ktx2() const;

    private:
        // Non-const
        ktxTexture1* ktx1_nc();
        ktxTexture2* ktx2_nc();

        ktxTexture* texture_ = nullptr;
    };

}  // namespace dal
