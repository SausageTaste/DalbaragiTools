#include <gtest/gtest.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/xchar.h>
#include <sung/general/stringtool.hpp>

#include "daltools/filesys/filesys.hpp"


namespace {

    std::filesystem::path find_root_path() {
        namespace fs = std::filesystem;
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


    TEST(DaltestFilesys, FileSubsysStd) {
        system("chcp 65001");
        const auto test_path = ::find_root_path() / "test";

        dal::Filesystem filesys;
        filesys.add_subsys(
            dal::create_filesubsys_std(":test", test_path.u8string())
        );

        for (auto folder_path : filesys.list_folders(":test")) {
            fmt::print("* {}\n", folder_path.u8string());

            for (auto file_path : filesys.list_files(folder_path)) {
                const auto content = filesys.read_file(file_path);
                ASSERT_TRUE(content.has_value());
                fmt::print(
                    "  - {} ({})\n",
                    file_path.u8string(),
                    sung::format_bytes(content->size())
                );
            }
        }
    }

}  // namespace


int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
