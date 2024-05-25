#include <filesystem>
#include <fstream>
#include <optional>

#include <fmt/format.h>

#include "daltools/model_exporter.h"
#include "daltools/model_parser.h"
#include "daltools/modifier.h"


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


    bool test_one_json(const fs::path& json_path) {
        const auto start_time = ::time_sec();

        const auto file_content = ::read_file<std::vector<uint8_t>>(json_path);
        std::vector<dal::parser::SceneIntermediate> scenes;
        const auto result = dal::parser::parse_json(
            scenes, file_content->data(), file_content->size()
        );
        assert(scenes.size() == 1);

        for (auto& scene : scenes) {
            dal::parser::flip_uv_vertically(scene);
            dal::parser::clear_collection_info(scene);
            dal::parser::optimize_scene(scene);
        }

        const auto model1 = dal::parser::convert_to_model_dmd(scenes.at(0));
        const auto binary1 =
            dal::parser::build_binary_model(model1, nullptr, nullptr).value();

        const auto model2 =
            dal::parser::parse_dmd(binary1.data(), binary1.size()).value();
        const auto binary2 =
            dal::parser::build_binary_model(model2, nullptr, nullptr).value();

        const auto similarity = compare_binary_buffers(binary1, binary2);
        if (1.0 != similarity) {
            fmt::print(
                "Binary results are different: {} ({})\n",
                json_path.u8string(),
                similarity
            );
            return false;
        }

        const auto end_time = ::time_sec();
        const auto elapsed_time = end_time - start_time;
        fmt::print(
            "Test passed: {} ({} ms)\n", json_path.u8string(), elapsed_time
        );
        return true;
    }

}  // namespace


int main() {
    const auto root_path = ::find_root_path();
    const auto json_path = root_path / "test" / "json";

    for (const auto& entry : fs::directory_iterator(json_path)) {
        if (entry.path().extension() == ".json") {
            if (!::test_one_json(entry.path())) {
                return 1;
            }
        }
    }
    return 0;
}
