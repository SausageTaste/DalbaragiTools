#include "daltools/img/backend/stb.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>


namespace {

    template <typename T>
    class TDataImage2D : public dal::TImage2D<T> {

    public:
        bool init(
            const T* data, uint32_t width, uint32_t height, uint32_t channels
        ) {
            if (nullptr == data)
                return false;

            const auto data_size = width * height * channels;
            this->data_.assign(data, data + data_size);

            this->width_ = width;
            this->height_ = height;
            this->channels_ = channels;
            return true;
        }

        void destroy() override { data_.clear(); }

        bool is_ready() const override {
            if (data_.empty())
                return false;
            if (0 == width_ || 0 == height_ || 0 == channels_)
                return false;
            return true;
        }

        uint32_t width() const override { return width_; }
        uint32_t height() const override { return height_; }
        uint32_t channels() const override { return channels_; }
        uint32_t value_type_size() const override { return sizeof(T); }

        const T* texel_ptr(int x, int y) const override {
            x = sung::clamp<int>(x, 0, width_ - 1);
            y = sung::clamp<int>(y, 0, height_ - 1);
            const auto index = (x + width_ * y) * channels_;
            return data_.data() + index;
        }

        const uint8_t* data() const {
            return reinterpret_cast<const uint8_t*>(data_.data());
        }

        uint32_t data_size() const {
            return static_cast<uint32_t>(data_.size() * sizeof(T));
        }

    private:
        std::vector<T> data_;
        uint32_t width_ = 0;
        uint32_t height_ = 0;
        uint32_t channels_ = 0;
    };

}  // namespace


namespace dal {

    std::unique_ptr<IImage2D> parse_img_stb(
        const uint8_t* data, size_t data_size, bool force_rgba
    ) {
        int width, height, channels;
        const auto req_comp = force_rgba ? STBI_rgb_alpha : STBI_default;

        if (stbi_is_hdr_from_memory(data, static_cast<int>(data_size))) {
            const auto pixel_data = stbi_loadf_from_memory(
                data,
                static_cast<int>(data_size),
                &width,
                &height,
                &channels,
                req_comp
            );
            if (nullptr == pixel_data) {
                return nullptr;
            }

            auto image = std::make_unique<TDataImage2D<float>>();
            image->init(pixel_data, width, height, force_rgba ? 4 : channels);
            stbi_image_free(pixel_data);
            return image;
        } else {
            const auto pixel_data = stbi_load_from_memory(
                data,
                static_cast<int>(data_size),
                &width,
                &height,
                &channels,
                req_comp
            );
            if (nullptr == pixel_data) {
                return nullptr;
            }

            auto image = std::make_unique<TDataImage2D<uint8_t>>();
            image->init(pixel_data, width, height, force_rgba ? 4 : channels);
            stbi_image_free(pixel_data);
            return image;
        }
    }

}  // namespace dal
