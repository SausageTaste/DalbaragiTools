#include "work_functions.hpp"

#include <fstream>
#include <unordered_set>

#include <glob/glob.h>
#include <spdlog/spdlog.h>
#include <argparse/argparse.hpp>
#include <sung/general/bytes.hpp>
#include <sung/general/stringtool.hpp>

#include "daltools/bundle/bundle.hpp"
#include "daltools/common/compression.h"


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

    void save_file(
        const std::filesystem::path& path, const std::vector<uint8_t>& content
    ) {
        std::ofstream file(path, std::ios::binary);
        file.write(
            reinterpret_cast<const char*>(content.data()), content.size()
        );
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
            spdlog::error("Output path must be a single string");
            return;
        }
        const auto out_path = ::make_path(out_str.back());

        sung::BytesBuilder items_block, data_block;
        std::unordered_set<std::string> added_names;
        const auto file_paths = glob::glob(
            parser.get<std::vector<std::string>>("inputs")
        );
        for (const auto& x : file_paths) {
            const auto name = x.filename().u8string();
            if (added_names.find(name) != added_names.end()) {
                spdlog::error("Name collision: '{}'", name);
                return;
            }
            added_names.insert(name);

            const auto content = ::read_file(x);

            const auto [offset, size] = data_block.add_arr(content);
            items_block.add_nt_str(name.c_str());
            items_block.add_uint64(offset);
            items_block.add_uint64(size);

            spdlog::info("Added '{}' ({})", name, sung::format_bytes(size));
        }

        sung::BytesBuilder combined;
        combined.enlarge(sizeof(dal::BundleHeader));

        const auto items_bro = compress_bro(items_block.vector());
        const auto items_info = combined.add_arr(items_bro.value());
        spdlog::info(
            "Item info: count={}, size={}, size_z={}, ratio={:.2f}",
            added_names.size(),
            sung::format_bytes(items_block.size()),
            sung::format_bytes(items_info.second),
            static_cast<double>(items_info.second) / items_block.size()
        );

        const auto data_bro = compress_bro(data_block.vector());
        const auto data_info = combined.add_arr(data_bro.value());
        spdlog::info(
            "Data info: size={}, size_z={}, ratio={:.2f}",
            sung::format_bytes(data_block.size()),
            sung::format_bytes(data_info.second),
            static_cast<double>(data_info.second) / data_block.size()
        );

        auto& header = *reinterpret_cast<BundleHeader*>(combined.data());
        header.init();
        header.set_items_info(
            items_info.first,
            items_block.size(),
            items_info.second,
            added_names.size()
        );
        header.set_data_info(
            data_info.first, data_block.size(), data_info.second
        );

        ::save_file(out_path, combined.vector());
        spdlog::info(
            "Output: '{}' ({})",
            out_path.u8string(),
            sung::format_bytes(combined.size())
        );

        return;
    }

}  // namespace dal
