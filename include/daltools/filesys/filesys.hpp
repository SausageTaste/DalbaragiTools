#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include "daltools/bundle/repo.hpp"


namespace dal {

    namespace fs = std::filesystem;


    class IFileSubsys {

    public:
        using bindata_t = std::vector<uint8_t>;

        virtual ~IFileSubsys() = default;

        virtual bool is_file(const fs::path& path) = 0;

        virtual std::vector<fs::path> list_files(const fs::path& path) = 0;
        virtual std::vector<fs::path> list_folders(const fs::path& path) = 0;

        virtual size_t read_file(
            const fs::path& path, uint8_t* buf, size_t buf_size
        ) = 0;
        virtual bool read_file(const fs::path& path, bindata_t& out) = 0;
    };


    class IDirWalker {

    public:
        virtual ~IDirWalker() = default;

        virtual bool on_folder(const fs::path& path, size_t depth) = 0;
        virtual bool on_bundle(const fs::path& path, size_t depth) = 0;
        virtual void on_file(const fs::path& path, size_t depth) = 0;
    };


    class Filesystem {

    public:
        Filesystem();
        ~Filesystem();

        BundleRepository& bundle_repo();
        void add_subsys(std::unique_ptr<IFileSubsys> subsys);

        bool is_file(const fs::path& path);

        std::vector<fs::path> list_files(const fs::path& path);
        std::vector<fs::path> list_folders(const fs::path& path);

        void walk(const fs::path& root, IDirWalker& visitor);

        bool read_file(const fs::path& path, std::vector<uint8_t>& out);
        std::optional<std::vector<uint8_t>> read_file(const fs::path& path);

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };


    std::unique_ptr<IFileSubsys> create_filesubsys_std(
        const std::string& prefix,
        const fs::path& root,
        Filesystem& filesys
    );

}  // namespace dal
