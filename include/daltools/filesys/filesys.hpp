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

        void add_subsys(std::unique_ptr<IFileSubsys> subsys);

        bool is_file(const fs::path& path);

        bool read_file(const fs::path& path, std::vector<uint8_t>& out);
        std::optional<std::vector<uint8_t>> read_file(const fs::path& path);

    private:
        class Impl;
        std::unique_ptr<Impl> pimpl_;
    };


    std::unique_ptr<IFileSubsys> create_filesubsys_std(
        const std::string& prefix, const fs::path& root
    );

}  // namespace dal
