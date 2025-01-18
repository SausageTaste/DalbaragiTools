#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>


#if false
namespace std {

    template <>
    struct hash<std::filesystem::path> {
        size_t operator()(const std::filesystem::path& path) const {
            return std::hash<std::string>{}(path.u8string());
        }
    };

}  // namespace std
#endif


namespace dal {

    namespace fs = std::filesystem;
    using path = fs::path;


    std::optional<fs::path> find_parent_path_that_has(
        const fs::path& path, const std::string& item_name_ext
    );
    std::optional<fs::path> find_parent_path_that_has(
        const std::string& item_name_ext
    );


    class IFileSubsys {

    public:
        virtual ~IFileSubsys() = default;

        virtual bool is_file(const fs::path& path) = 0;

        virtual bool read_file(
            const fs::path& path, std::vector<uint8_t>& out
        ) = 0;

        virtual bool read_file(
            const fs::path& path, std::vector<std::byte>& out
        ) = 0;
    };


    class Filesystem {

    public:
        Filesystem();
        ~Filesystem();

        void add_subsys(std::unique_ptr<IFileSubsys> subsys);

        bool is_file(const fs::path& path);

        bool read_file(const fs::path& path, std::vector<uint8_t>& out);
        bool read_file(const fs::path& path, std::vector<std::byte>& out);
        std::optional<std::vector<uint8_t>> read_file(const fs::path& path);

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };
    using HFilesys = std::shared_ptr<Filesystem>;


    std::unique_ptr<IFileSubsys> create_filesubsys_std(
        const std::string& prefix, const fs::path& root
    );

}  // namespace dal
