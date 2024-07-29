#include "work_functions.hpp"

#include <fstream>

#include <glob/glob.h>
#include <spdlog/fmt/fmt.h>
#include <argparse/argparse.hpp>


namespace {

    std::filesystem::path make_path(const std::string& str) {
        return std::filesystem::absolute(std::filesystem::path{ str });
    }

    auto read_file(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        std::vector<uint8_t> content;
        content.assign(
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        );
        return content;
    }

}  // namespace


namespace dal {

    void work_bundle(int argc, char* argv[]) {
        argparse::ArgumentParser parser{ "daltools" };
        parser.add_argument("operation").help("Operation name").required();
        parser.add_argument("-o", "--output")
            .help("Output file path")
            .required();
        parser.add_argument("inputs").help("Input paths").remaining();
        parser.parse_args(argc, argv);

        const auto out_str = parser.get<std::vector<std::string>>("--output");
        if (1 != out_str.size()) {
            fmt::print("Output path must be a single string\n");
            return;
        }
        const auto out_path = ::make_path(out_str.back());
        fmt::print(
            "Output: '{}' from '{}'\n", out_path.string(), out_str.back()
        );

        const auto file_paths = glob::glob(
            parser.get<std::vector<std::string>>("inputs")
        );
        for (const auto& x : file_paths) {
            const auto content = ::read_file(x);
            fmt::print(" - {} ({})\n", x.string(), content.size() / 1024.0);
        }
    }

}  // namespace dal
