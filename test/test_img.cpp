#include <filesystem>
#include <fstream>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/fmt/fmt.h>

#include "daltools/common/util.h"
#include "daltools/img/backend/ktx.hpp"
#include "daltools/img/backend/stb.hpp"


namespace {

    std::string format_bytes(size_t bytes) {
        if (bytes < 1024) {
            return fmt::format("{} B", bytes);
        } else if (bytes < 1024 * 1024) {
            return fmt::format("{:.1f} KiB", bytes / 1024.0);
        } else if (bytes < 1024 * 1024 * 1024) {
            return fmt::format("{:.1f} MiB", bytes / (1024.0 * 1024.0));
        } else {
            return fmt::format(
                "{:.1f} GiB", bytes / (1024.0 * 1024.0 * 1024.0)
            );
        }
    }


    TEST(DaltestKtx, SimpleTest) {
        const auto root_path = dal::find_git_repo_root(dal::fs::current_path());
        ASSERT_TRUE(root_path.has_value());
        const auto img_path = root_path.value() / "test" / "img";

        std::vector<std::unique_ptr<dal::IImage>> images;

        for (const auto& entry : dal::fs::directory_iterator(img_path)) {
            const auto img_data = dal::read_file(entry.path());
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
                if (auto ktx1 = ktx_img->ktx1()) {
                    fmt::print(
                        "KTX 1: dim={}x{}x{}, levels={}, compressed={}, "
                        "dataSize={}\n",
                        ktx1->baseWidth,
                        ktx1->baseHeight,
                        ktx1->baseDepth,
                        ktx1->numLevels,
                        ktx1->isCompressed,
                        ::format_bytes(ktx1->dataSize)
                    );
                } else if (auto ktx2 = ktx_img->ktx2()) {
                    auto& tex = ktx_img->ktx();
                    auto& te2 = *ktx2;

                    if (ktx_img->need_transcoding()) {
                        ASSERT_TRUE(ktx_img->transcode(KTX_TTF_RGBA32));
                    }

                    for (uint32_t y = 0 ; y < ktx_img->base_height(); ++y) {
                        for (uint32_t x = 0 ; x < ktx_img->base_width(); ++x) {
                            const auto pixel = ktx_img->get_base_pixel(x, y);
                            fmt::print(
                                "Pixel at ({}, {}): rgba=({}, {}, {}, {})\n",
                                x, y,
                                pixel->r, pixel->g, pixel->b, pixel->a
                            );
                        }
                    }

                    fmt::print(
                        "KTX 2: dim={}x{}x{}, levels={}, compressed={}, "
                        "dataSize={}\n",
                        tex.baseWidth,
                        tex.baseHeight,
                        tex.baseDepth,
                        tex.numLevels,
                        tex.isCompressed,
                        ::format_bytes(tex.dataSize)
                    );
                    continue;
                }
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
