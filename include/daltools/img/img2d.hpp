#pragma once

#include <vector>

#include "daltools/img/img.hpp"


namespace dal {

    template <typename T>
    T clamp(T val, T min, T max) {
        if (val < min) {
            return min;
        } else if (val > max) {
            return max;
        } else {
            return val;
        }
    }


    class IImage2D : public IImage {

    public:
        virtual const uint8_t* data() const = 0;
        virtual uint32_t data_size() const = 0;

        virtual uint32_t width() const = 0;
        virtual uint32_t height() const = 0;
        virtual uint32_t channels() const = 0;
        virtual uint32_t value_type_size() const = 0;
    };


    template <typename T>
    class TImage2D : public IImage2D {

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

        const uint8_t* data() const override {
            return reinterpret_cast<const uint8_t*>(data_.data());
        }

        uint32_t data_size() const override {
            return static_cast<uint32_t>(data_.size() * sizeof(T));
        }

        uint32_t width() const override { return width_; }
        uint32_t height() const override { return height_; }
        uint32_t channels() const override { return channels_; }
        uint32_t value_type_size() const override { return sizeof(T); }

        const T* get_texel_at_clamp(int x, int y) const {
            x = clamp<int>(x, 0, width_ - 1);
            y = clamp<int>(y, 0, height_ - 1);
            const auto index = (x + width_ * y) * channels_;
            return data_.data() + index;
        }

    private:
        std::vector<T> data_;
        uint32_t width_ = 0;
        uint32_t height_ = 0;
        uint32_t channels_ = 0;
    };

}  // namespace dal
