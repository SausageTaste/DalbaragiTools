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


namespace fs = std::filesystem;


namespace {

    fs::path make_path(const std::string& str) {
        return fs::absolute(fs::path{ str });
    }

    auto read_file(const fs::path& path) {
        std::ifstream file(path, std::ios::binary);
        std::vector<uint8_t> content;
        content.assign(
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        );
        return content;
    }

    void save_file(const fs::path& path, const void* data, size_t size) {
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(data), size);
    }

    void save_file(const fs::path& path, const std::vector<uint8_t>& content) {
        ::save_file(path, content.data(), content.size());
    }

    fs::path select_not_colliding_folder_name(
        const fs::path& loc, const std::string& base_name
    ) {
        const auto folder_path = loc / fs::u8path(base_name);
        if (!fs::exists(folder_path)) {
            return fs::absolute(folder_path);
        }

        for (int i = 0; i < 1000; ++i) {
            const auto name = fmt::format("{}_{:0>3}", base_name, i);
            const auto folder_path = loc / fs::u8path(name);
            if (!fs::exists(folder_path)) {
                return fs::absolute(folder_path);
            }
        }

        throw std::runtime_error("Failed to find a folder name");
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

    void work_bundle_view(int argc, char* argv[]) {
        argparse::ArgumentParser parser{ "daltools" };
        parser.add_argument("operation").help("Operation name").required();
        parser.add_argument("inputs").help("Input paths").remaining();
        parser.parse_args(argc, argv);

        const auto file_paths = glob::glob(
            parser.get<std::vector<std::string>>("inputs")
        );
        for (const auto& x : file_paths) {
            std::ifstream file(x, std::ifstream ::binary);

            // Header block
            dal::BundleHeader header;
            {
                std::array<char, sizeof(BundleHeader)> headbuf;
                file.read(headbuf.data(), headbuf.size());
                std::streamsize res = file.gcount();

                if (res != headbuf.size()) {
                    spdlog::error(
                        "File is too small to be a Dal Bundle file: '{}'",
                        x.u8string()
                    );
                    continue;
                }

                header = *reinterpret_cast<const BundleHeader*>(headbuf.data());
                if (!header.is_magic_valid()) {
                    spdlog::error(
                        "Magic number is invalid: '{}'", x.u8string()
                    );
                    continue;
                }

                fmt::print("Bundle: '{}'\n", x.u8string());
                fmt::print("  * Created: '{}'\n", header.created_datetime());
                fmt::print("  * Version: {}\n", header.version());
                fmt::print(
                    "  * Items: size={}, size_z={}, count={}\n",
                    sung::format_bytes(header.items_size()),
                    sung::format_bytes(header.items_size_z()),
                    header.items_count()
                );
                fmt::print(
                    "  * Data: size={}, size_z={}\n",
                    sung::format_bytes(header.data_size()),
                    sung::format_bytes(header.data_size_z())
                );
            }

            // Items block
            std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> item;
            {
                file.seekg(header.items_offset());
                std::vector<uint8_t> items_buf(header.items_size_z());
                file.read((char*)items_buf.data(), items_buf.size());
                if (file.gcount() != items_buf.size()) {
                    spdlog::error(
                        "Failed to read items block: '{}'", x.u8string()
                    );
                    continue;
                }

                const auto item_block = decomp_bro(
                    items_buf, header.items_size()
                );
                if (!item_block.has_value()) {
                    spdlog::error(
                        "Failed to decompress items block: '{}'", x.u8string()
                    );
                    continue;
                }

                sung::BytesReader items_reader{ item_block->data(),
                                                item_block->size() };
                for (uint64_t i = 0; i < header.items_count(); ++i) {
                    const auto name = items_reader.read_nt_str();
                    const auto offset = items_reader.read_uint64().value();
                    const auto size = items_reader.read_uint64().value();
                    item[name] = { offset, size };

                    fmt::print(
                        "    - '{}' ({})\n", name, sung::format_bytes(size)
                    );
                }

                if (!items_reader.is_eof()) {
                    spdlog::warn(
                        "Items block and item count mismatch: '{}'",
                        x.u8string()
                    );
                }
            }

            // Data block
            {
                file.seekg(header.data_offset());
                std::vector<uint8_t> data_buf(header.data_size_z());
                file.read((char*)data_buf.data(), data_buf.size());
                if (file.gcount() != data_buf.size()) {
                    spdlog::error(
                        "Failed to read data block: '{}'", x.u8string()
                    );
                    continue;
                }

                const auto data_block = decomp_bro(
                    data_buf, header.data_size()
                );
                if (!data_block.has_value()) {
                    spdlog::error(
                        "Failed to decompress data block: '{}'", x.u8string()
                    );
                    continue;
                }

                for (auto& x : item) {
                    const auto end_pos = x.second.first + x.second.second;
                    if (end_pos > data_block->size()) {
                        spdlog::error("Data block overflow: '{}'", x.first);
                        continue;
                    }
                }
            }

            continue;
        }
    }

    void work_extract(int argc, char* argv[]) {
        argparse::ArgumentParser parser{ "daltools" };
        parser.add_argument("operation").help("Operation name").required();
        parser.add_argument("inputs").help("Input paths").remaining();
        parser.parse_args(argc, argv);

        const auto inputs = parser.get<std::vector<std::string>>("inputs");
        const auto file_paths = glob::glob(inputs);

        for (const auto& x : file_paths) {
            fmt::print("\n*** Extracting: '{}'\n", fs::absolute(x).u8string());

            std::ifstream file(x, std::ifstream ::binary);

            // Header block
            dal::BundleHeader header;
            {
                std::array<char, sizeof(BundleHeader)> headbuf;
                file.read(headbuf.data(), headbuf.size());
                std::streamsize res = file.gcount();

                if (res != headbuf.size()) {
                    fmt::print("Invalid Dalbaragi Bundle file\n");
                    continue;
                }

                header = *reinterpret_cast<const BundleHeader*>(headbuf.data());
                if (!header.is_magic_valid()) {
                    fmt::print("Invalid Magic number\n");
                    continue;
                }
            }

            // Items block
            std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> item;
            {
                file.seekg(header.items_offset());
                std::vector<uint8_t> items_buf(header.items_size_z());
                file.read((char*)items_buf.data(), items_buf.size());
                if (file.gcount() != items_buf.size()) {
                    fmt::print("Failed to read items block\n");
                    continue;
                }

                const auto item_block = decomp_bro(
                    items_buf, header.items_size()
                );
                if (!item_block.has_value()) {
                    fmt::print("Failed to decompress items block\n");
                    continue;
                }

                sung::BytesReader items_reader{ item_block->data(),
                                                item_block->size() };
                for (uint64_t i = 0; i < header.items_count(); ++i) {
                    const auto name = items_reader.read_nt_str();
                    const auto offset = items_reader.read_uint64().value();
                    const auto size = items_reader.read_uint64().value();
                    item[name] = { offset, size };
                }

                if (!items_reader.is_eof()) {
                    fmt::print("Items block and item count mismatch\n");
                }
            }

            const auto out_dir = ::select_not_colliding_folder_name(
                x.parent_path(), x.stem().u8string()
            );
            std::filesystem::create_directory(out_dir);
            fmt::print("Output folder: '{}'\n", out_dir.u8string());

            // Data block
            {
                file.seekg(header.data_offset());
                std::vector<uint8_t> data_buf(header.data_size_z());
                file.read((char*)data_buf.data(), data_buf.size());
                if (file.gcount() != data_buf.size()) {
                    fmt::print("Failed to read data block\n");
                    continue;
                }

                const auto data_block = decomp_bro(
                    data_buf, header.data_size()
                );
                if (!data_block.has_value()) {
                    fmt::print("Failed to decompress data block\n");
                    continue;
                }

                size_t count = 0;
                for (auto& [name, offset_size] : item) {
                    const auto [offset, size] = offset_size;
                    const auto begin = data_block->data() + offset;

                    const auto out_path = out_dir / fs::u8path(name);
                    ::save_file(out_path, begin, size);
                    ++count;
                }
                fmt::print("Extracted {} files\n", count);
            }
        }
    }

}  // namespace dal
