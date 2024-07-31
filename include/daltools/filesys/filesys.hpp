#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>


namespace dal {

    namespace fs = std::filesystem;


    class IFileSubsys {

    public:
        using bindata_t = std::vector<uint8_t>;

        virtual ~IFileSubsys() = default;

        virtual bool is_file(const fs::path& path) = 0;

        virtual std::vector<fs::path> list_files(const fs::path& path) = 0;
        virtual std::vector<fs::path> list_folders(const fs::path& path) = 0;

        virtual bool read_file(const fs::path& path, bindata_t& out) = 0;
    };


    std::unique_ptr<IFileSubsys> create_filesubsys_std(
        const std::string& prefix, const fs::path& root
    );


    class Filesystem {

    public:
        Filesystem();
        ~Filesystem();

        void add_subsys(std::unique_ptr<IFileSubsys> subsys);

        bool is_file(const fs::path& path);

        std::vector<fs::path> list_files(const fs::path& path);
        std::vector<fs::path> list_folders(const fs::path& path);

        bool read_file(const fs::path& path, std::vector<uint8_t>& out);
        std::optional<std::vector<uint8_t>> read_file(const fs::path& path);

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };

}  // namespace dal
