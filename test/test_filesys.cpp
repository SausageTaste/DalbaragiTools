#include <gtest/gtest.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/xchar.h>
#include <sung/general/stringtool.hpp>

#include "daltools/filesys/filesys.hpp"


namespace {

    namespace fs = std::filesystem;

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


    class Walker : public dal::IDirWalker {

    public:
        Walker(dal::Filesystem& filesys) : filesys_{ filesys } {}

        bool on_folder(const fs::path& path, size_t depth) override {
            for (size_t i = 0; i < depth; ++i) fmt::print("  ");
            fmt::print("(Fold) {}\n", path.u8string());
            return true;
        }

        bool on_bundle(const fs::path& path, size_t depth) override {
            for (size_t i = 0; i < depth; ++i) fmt::print("  ");
            fmt::print("(Bndl) {}\n", path.u8string());
            return true;
        }

        void on_file(const fs::path& path, size_t depth) override {
            for (size_t i = 0; i < depth; ++i) fmt::print("  ");
            if (const auto content = filesys_.read_file(path))
                fmt::print(
                    "(File) {} ({})\n",
                    path.u8string(),
                    sung::format_bytes(content->size())
                );
            else
                fmt::print("(File) {} (not found)\n", path.u8string());
        }

    private:
        dal::Filesystem& filesys_;
    };


    TEST(DaltestFilesys, FileSubsysStd) {
        system("chcp 65001");
        const auto test_path = ::find_root_path() / "test";

        dal::Filesystem filesys;
        filesys.add_subsys(
            dal::create_filesubsys_std(":test", test_path.u8string(), filesys)
        );

        ::Walker walker{ filesys };
        filesys.walk(":test", walker);
    }

}  // namespace


int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
