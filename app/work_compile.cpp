#include "work_functions.hpp"

#include <filesystem>
#include <fstream>

#include <argparse/argparse.hpp>

#include "daltools/crypto.h"
#include "daltools/konst.h"
#include "daltools/model_exporter.h"
#include "daltools/model_parser.h"
#include "daltools/modifier.h"


namespace {

    std::optional<std::vector<uint8_t>> read_file(const std::string& path) {
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


    void do_file(
        const std::string& src_path, const std::optional<std::string> key_path
    ) {
        using namespace dal::crypto;
        using namespace dal::parser;
        using SecretKey = PublicKeySignature::SecretKey;

        const auto file_content = ::read_file(src_path);
        std::vector<SceneIntermediate> scenes;
        const auto result = parse_json(
            scenes, file_content->data(), file_content->size()
        );

        for (auto& scene : scenes) {
            flip_uv_vertically(scene);
            clear_collection_info(scene);
            optimize_scene(scene);
        }

        const auto model = convert_to_model_dmd(scenes.at(0));
        std::optional<binary_buffer_t> bin_built;

        if (key_path.has_value()) {
            PublicKeySignature sign_mgr{ CONTEXT_PARSER };
            const auto [key, attrib] = load_key<SecretKey>(key_path->c_str());
            bin_built = build_binary_model(model, &key, &sign_mgr);
        } else {
            bin_built = build_binary_model(model, nullptr, nullptr);
        }

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
        parser.add_argument("-k", "--key").help("Path to a secret key file");
        parser.add_argument("files").help("Input model file paths").remaining();
        parser.parse_args(argc, argv);

        const auto files = parser.get<std::vector<std::string>>("files");
        for (const auto& src_path : files)
            ::do_file(src_path, parser.present("--key"));
    }

}  // namespace dal
