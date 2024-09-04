#include <filesystem>
#include <fstream>
#include <optional>

#include <gtest/gtest.h>
#include <spdlog/fmt/fmt.h>
#include <sung/general/time.hpp>

#include "daltools/common/util.h"
#include "daltools/dmd/exporter.h"
#include "daltools/dmd/parser.h"
#include "daltools/json/parser.h"
#include "daltools/scene/modifier.h"


namespace fs = std::filesystem;


namespace {

    double time_sec() {
        const auto now = std::chrono::system_clock::now();
        return static_cast<double>(now.time_since_epoch().count()) / 1e9;
    }


    fs::path find_root_path() {
        constexpr int DEPTH = 10;

        fs::path current_dir = ".";

        for (int i = 0; i < DEPTH; ++i) {
            for (const auto& entry : fs::directory_iterator(current_dir)) {
                if (entry.path().filename().u8string() == ".git") {
                    return fs::absolute(current_dir);
                }
            }

            current_dir /= "..";
        }

        throw std::runtime_error{ "Failed to find root folder" };
    }

    std::vector<fs::path> get_json_paths() {
        std::vector<fs::path> output;

        const auto root_path = ::find_root_path();
        const auto json_path = root_path / "test" / "json";

        for (const auto& entry : fs::directory_iterator(json_path)) {
            if (entry.path().extension() == ".json") {
                output.push_back(entry.path());
            }
        }

        return output;
    }


    template <typename T>
    std::optional<T> read_file(const fs::path& path) {
        std::ifstream file{ path,
                            std::ios::ate | std::ios::binary | std::ios::in };

        if (!file.is_open())
            return std::nullopt;

        const auto file_size = static_cast<size_t>(file.tellg());
        T buffer;
        buffer.resize(file_size);

        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        file.close();

        return buffer;
    }

    double compare_binary_buffers(
        const std::vector<uint8_t>& lhs, const std::vector<uint8_t>& rhs
    ) {
        if (lhs.size() != rhs.size()) {
            return 0.0;
        }

        size_t same_count = 0;
        for (size_t i = 0; i < lhs.size(); ++i) {
            if (lhs[i] == rhs[i]) {
                ++same_count;
            }
        }

        return static_cast<double>(same_count) /
               static_cast<double>(lhs.size());
    }


    TEST(Json, RealFileTest) {
        for (const auto json_path : ::get_json_paths()) {
            const auto start_time = ::time_sec();
            const auto file_content = ::read_file<std::vector<uint8_t>>(
                json_path
            );
            ASSERT_TRUE(file_content.has_value())
                << "Failed to read file: " << json_path.u8string();

            fmt::print("Testing: {}\n", json_path.u8string());
            sung::MonotonicRealtimeTimer timer;

            // Replace extension from .json to .bin
            auto bin_path = json_path;
            bin_path.replace_extension(".bin");
            const auto bin_file_content = ::read_file<std::vector<uint8_t>>(
                bin_path
            );
            fmt::print(" - Read file ({:.2f})\n", timer.check_get_elapsed());

            std::vector<dal::parser::SceneIntermediate> scenes;
            dal::parser::JsonParseResult result;
            if (bin_file_content) {
                result = dal::parser::parse_json_bin(
                    scenes, *file_content, *bin_file_content
                );
            } else {
                result = dal::parser::parse_json(scenes, *file_content);
            }
            ASSERT_EQ(scenes.size(), 1);
            fmt::print(" - Json parsed ({:.2f})\n", timer.check_get_elapsed());

            for (auto& scene : scenes) {
                dal::parser::flip_uv_vertically(scene);
                dal::parser::clear_collection_info(scene);
                dal::parser::optimize_scene(scene, json_path);
            }
            fmt::print(" - Optimzied ({:.2f})\n", timer.check_get_elapsed());

            const auto model1 = dal::parser::convert_to_model_dmd(scenes.at(0));
            fmt::print(" - Model built ({:.2f})\n", timer.check_get_elapsed());

            const auto binary1 = dal::parser::build_binary_model(
                model1, dal::CompressMethod::brotli
            );
            ASSERT_TRUE(binary1.has_value());
            fmt::print(" - DMD built ({:.2f})\n", timer.check_get_elapsed());

            const auto model2 = dal::parser::parse_dmd(*binary1);
            ASSERT_TRUE(model2.has_value());
            fmt::print(" - DMD parsed ({:.2f})\n", timer.check_get_elapsed());

            const auto binary2 = dal::parser::build_binary_model(
                *model2, dal::CompressMethod::brotli
            );
            ASSERT_TRUE(binary2.has_value());
            fmt::print(" - DMD again ({:.2f})\n", timer.check_get_elapsed());

            const auto similarity = compare_binary_buffers(*binary1, *binary2);
            ASSERT_DOUBLE_EQ(1.0, similarity) << fmt::format(
                "Binary results are different: {} ({})",
                json_path.u8string(),
                similarity
            );

            const auto end_time = ::time_sec();
            const auto elapsed_time = end_time - start_time;
            fmt::print(
                "Test passed: {} ({} ms)\n", json_path.u8string(), elapsed_time
            );
        }
    }

}  // namespace


int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
