#include <filesystem>
#include <fstream>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/fmt/fmt.h>

#include "daltools/img/backend/ktx.hpp"
#include "daltools/img/backend/stb.hpp"


namespace {

    auto read_file(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        std::vector<uint8_t> content;
        content.assign(
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        );
        return content;
    }

    std::string format_bytes(size_t bytes) {
        if (bytes < 1024) {
            return fmt::format("{} B", bytes);
        } else if (bytes < 1024 * 1024) {
            return fmt::format("{:.1f} KiB", bytes / 1024.0);
        } else if (bytes < 1024 * 1024 * 1024) {
            return fmt::format("{:.1f} MiB", bytes / (1024.0 * 1024.0));
        } else {
            return fmt::format("{:.1f} GiB", bytes / (1024.0 * 1024.0 * 1024.0));
        }
    }


    TEST(DaltestKtx, SimpleTest) {
        const auto path = "C:/Users/AORUS/Desktop/refine_output/src";

        std::vector<std::unique_ptr<dal::IImage>> images;

        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            const auto img_data = ::read_file(entry.path());
            dal::ImageParseInfo parse_info;
            parse_info.data_ = img_data.data();
            parse_info.size_ = img_data.size();
            parse_info.force_rgba_ = false;
            parse_info.file_path_ = entry.path().string();
            images.push_back(dal::parse_img(parse_info));
        }

        for (const auto& img : images) {
            ASSERT_TRUE(img->is_ready());

            if (auto ktx_img = dynamic_cast<dal::KtxImage*>(img.get())) {
                fmt::print(
                    "KtxImage: dim={}x{}, compressed={}, genMipmaps={}, dataSize={}\n",
                    ktx_img->texture_->baseWidth,
                    ktx_img->texture_->baseHeight,
                    ktx_img->texture_->isCompressed,
                    ktx_img->texture_->generateMipmaps,
                    ::format_bytes(ktx_img->texture_->dataSize)
                );
            } else if (auto img2d = dynamic_cast<dal::IImage2D*>(img.get())) {
                fmt::print(
                    "IImage2D: dim={}x{}, ch={}, typeSize={}, dataSize={}\n",
                    img2d->width(),
                    img2d->height(),
                    img2d->channels(),
                    img2d->value_type_size(),
                    ::format_bytes(img2d->data_size())
                );
            }
        }

        return;
    }

}  // namespace


int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
