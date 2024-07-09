#include "work_functions.hpp"

#include <filesystem>
#include <fstream>

#include <fmt/format.h>
#include <argparse/argparse.hpp>

#include "daltools/common/crypto.h"
#include "daltools/common/konst.h"
#include "daltools/dmd/exporter.h"
#include "daltools/dmd/parser.h"
#include "daltools/json/parser.h"
#include "daltools/scene/modifier.h"


namespace {

    std::optional<std::vector<uint8_t>> read_file(
        const std::filesystem::path& path
    ) {
        std::ifstream file{ path,
                            std::ios::ate | std::ios::binary | std::ios::in };

        if (!file.is_open())
            return std::nullopt;

        const auto file_size = static_cast<size_t>(file.tellg());
        std::vector<uint8_t> buffer;
        buffer.resize(file_size);

        file.seekg(0);
        file.read((char*)buffer.data(), buffer.size());
        file.close();

        return buffer;
    }


    void do_file(const std::filesystem::path& src_path) {
        using namespace dal::crypto;
        using namespace dal::parser;
        using SecretKey = PublicKeySignature::SecretKey;

        const auto json_data = ::read_file(src_path);

        auto bin_path = src_path;
        bin_path.replace_extension("bin");

        std::vector<SceneIntermediate> scenes;
        JsonParseResult result;
        if (const auto bin_data = ::read_file(bin_path)) {
            result = parse_json_bin(
                scenes,
                json_data->data(),
                json_data->size(),
                bin_data->data(),
                bin_data->size()
            );
        } else {
            result = parse_json(scenes, json_data->data(), json_data->size());
        }

        for (auto& scene : scenes) {
            flip_uv_vertically(scene);
            clear_collection_info(scene);
            optimize_scene(scene);
        }

        const auto model = convert_to_model_dmd(scenes.at(0));
        const auto bin_built = build_binary_model(model);

        std::filesystem::path output_path = src_path;
        output_path.replace_extension("dmd");

        std::ofstream file(output_path.u8string().c_str(), std::ios::binary);
        file.write((const char*)bin_built->data(), bin_built->size());
        file.close();
    }

}  // namespace


namespace dal {

    void work_compile(int argc, char* argv[]) {
        argparse::ArgumentParser parser{ "daltools" };
        parser.add_argument("operation").help("Operation name");
        parser.add_argument("files").help("Input model file paths").remaining();
        parser.parse_args(argc, argv);

        const auto files = parser.get<std::vector<std::string>>("files");
        for (const auto& src_path_str : files) {
            const std::filesystem::path src_path{ src_path_str };
            ::do_file(src_path);
        }
    }

}  // namespace dal
