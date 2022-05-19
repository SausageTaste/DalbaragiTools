#include <fstream>
#include <iostream>
#include <filesystem>

#include <daltools/model_parser.h>
#include <daltools/modifier.h>
#include <daltools/model_exporter.h>


namespace {

    std::vector<std::string> get_all_dir_within_folder(std::string folder) {
        std::vector<std::string> names;

        for (const auto& entry : std::filesystem::directory_iterator(folder)) {
            names.push_back(entry.path().filename().string());
        }

        return names;
    }

    std::string find_root_path() {
        std::string current_dir = ".";

        for (int i = 0; i < 10; ++i) {
            for (const auto& x : ::get_all_dir_within_folder(current_dir)) {
                if (x == ".git") {
                    return current_dir;
                }
            }

            current_dir += "/..";
        }

        throw std::runtime_error{ "failed to find root folder." };
    }

    std::vector<uint8_t> read_file(const char* const path) {
        std::ifstream file{ path, std::ios::ate | std::ios::binary | std::ios::in };

        if (!file.is_open()) {
            throw std::runtime_error(std::string{ "failed to open file: " } + path);
        }

        const auto fileSize = static_cast<size_t>(file.tellg());
        std::vector<uint8_t> buffer;
        buffer.resize(fileSize);

        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        file.close();

        return buffer;
    }

}


namespace {

    void test_a_json(const char* const file_path) {
        const auto file_content = ::read_file(file_path);
        std::vector<dal::parser::SceneIntermediate> scenes;
        const auto result = dal::parser::parse_json(scenes, file_content.data(), file_content.size());

        const auto model = dal::parser::convert_to_model_dmd(scenes[0]);
        const auto binary_built = dal::parser::build_binary_model(model, nullptr, nullptr);

        std::ofstream file(::find_root_path() + "/shit.dmd", std::ios::binary);
        file.write(reinterpret_cast<const char*>(binary_built->data()), binary_built->size());
        file.close();

        return;
    }

}


int main() {
    for (auto entry : std::filesystem::directory_iterator(::find_root_path() + "/test")) {
        if (entry.path().extension().string() == ".json") {
            ::test_a_json(entry.path().string().c_str());
        }
    }

	return 0;
}
