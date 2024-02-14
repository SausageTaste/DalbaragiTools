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

    template <typename T>
    std::optional<T> read_file(const char* const path) {
        using namespace std::string_literals;

        std::ifstream file{ path, std::ios::ate | std::ios::binary | std::ios::in };

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

}


namespace dal {

    void work_compile(int argc, char* argv[]) {
        argparse::ArgumentParser parser{ "daltools" };

        parser.add_argument("operation")
            .help("Operation name");

        parser.add_argument("-k", "--key")
            .help("Path to a secret key file");

        parser.add_argument("files")
            .help("Input model file paths")
            .remaining();

        parser.parse_args(argc, argv);

        const auto files = parser.get<std::vector<std::string>>("files");

        for (const auto& src_path : files) {
            const auto file_content = ::read_file<std::vector<uint8_t>>(src_path.c_str());
            std::vector<dal::parser::SceneIntermediate> scenes;
            const auto result = dal::parser::parse_json(scenes, file_content->data(), file_content->size());

            for (auto& scene : scenes) {
                dal::parser::flip_uv_vertically(scene);
                dal::parser::clear_collection_info(scene);
                dal::parser::optimize_scene(scene);
            }

            const auto model = dal::parser::convert_to_model_dmd(scenes.at(0));
            dal::parser::binary_buffer_t binary_built;
            const auto key_path = parser.present("--key");
            if (key_path.has_value()) {
                dal::crypto::PublicKeySignature sign_mgr{ dal::crypto::CONTEXT_PARSER };
                const auto [key, attrib] = dal::crypto::load_key<dal::crypto::PublicKeySignature::SecretKey>(key_path->c_str());
                binary_built = dal::parser::build_binary_model(model, &key, &sign_mgr).value();
            }
            else {
                binary_built = dal::parser::build_binary_model(model, nullptr, nullptr).value();
            }

            std::filesystem::path output_path = src_path;
            output_path.replace_extension("dmd");

            std::ofstream file(output_path.u8string().c_str(), std::ios::binary);
            file.write(reinterpret_cast<const char*>(binary_built.data()), binary_built.size());
            file.close();
        }
    }

}
