#include "daltools/filesys/filesys.hpp"

#include <spdlog/fmt/fmt.h>
#include <fstream>
#include <sung/general/bytes.hpp>

#include "daltools/bundle/bundle.hpp"
#include "daltools/common/compression.h"


namespace {

    namespace fs = std::filesystem;


    bool walk_bundle(
        const fs::path& file_path,
        dal::IDirWalker& walker,
        dal::IFileSubsys& subsys,
        size_t depth
    ) {
        dal::BundleHeader header;
        {
            const auto res = subsys.read_file(
                file_path,
                reinterpret_cast<uint8_t*>(&header),
                sizeof(dal::BundleHeader)
            );
            if (res != sizeof(dal::BundleHeader))
                return false;
            if (!header.is_magic_valid())
                return false;
        }

        std::vector<uint8_t> file_content;
        {
            const auto items_buf_size = sizeof(dal::BundleHeader) +
                                        header.items_size_z();
            file_content.resize(items_buf_size);
            const auto res = subsys.read_file(
                file_path, file_content.data(), file_content.size()
            );
            if (res != items_buf_size)
                return false;
        }

        const auto item_block = dal::decomp_bro(
            file_content.data() + header.items_offset(),
            header.items_size_z(),
            header.items_size()
        );
        if (!item_block.has_value())
            return false;

        std::vector<std::string> item_names;
        {
            sung::BytesReader items_reader{ item_block->data(),
                                            item_block->size() };
            for (uint64_t i = 0; i < header.items_count(); ++i) {
                const auto name = items_reader.read_nt_str();
                items_reader.advance(sizeof(uint64_t) * 2);
                item_names.push_back(name);
            }
            if (!items_reader.is_eof())
                return false;
        }

        walker.on_bundle(file_path, depth);
        for (const auto& name : item_names) {
            walker.on_file(file_path / name, depth + 1);
        }

        return true;
    }

}  // namespace


namespace {

    class FileSubsysStd : public dal::IFileSubsys {

    public:
        FileSubsysStd(
            const std::string& prefix,
            const fs::path& root,
            dal::BundleRepository& bundles
        )
            : prefix_{ prefix }, root_{ root }, bundles_{ bundles } {}

        bool is_file(const fs::path& i_path) override {
            const auto raw_path = this->make_raw_path(i_path);
            if (!raw_path.has_value())
                return false;

            if (fs::is_regular_file(raw_path.value()))
                return true;

            return false;
        }

        std::vector<fs::path> list_files(const fs::path& i_path) override {
            std::vector<fs::path> output;

            const auto raw_path = this->make_raw_path(i_path);
            if (!raw_path.has_value())
                return output;

            for (const auto& entry : fs::directory_iterator(*raw_path)) {
                if (entry.is_regular_file())
                    output.push_back(this->make_i_path(entry.path()));
            }

            return output;
        }

        std::vector<fs::path> list_folders(const fs::path& i_path) override {
            std::vector<fs::path> output;

            const auto raw_path = this->make_raw_path(i_path);
            if (!raw_path.has_value())
                return output;

            for (const auto& entry : fs::directory_iterator(*raw_path)) {
                if (entry.is_directory())
                    output.push_back(this->make_i_path(entry.path()));
            }

            return output;
        }

        size_t read_file(const fs::path& path, uint8_t* buf, size_t buf_size)
            override {
            const auto raw_path = this->make_raw_path(path);
            if (!raw_path.has_value())
                return false;

            std::ifstream file(*raw_path, std::ios::binary);
            file.read(reinterpret_cast<char*>(buf), buf_size);
            return file.gcount();
        }

        bool read_file(const fs::path& i_path, bindata_t& out) override {
            const auto raw_path = this->make_raw_path(i_path);
            if (!raw_path.has_value())
                return false;

            std::ifstream file(*raw_path, std::ios::binary);
            if (file.is_open()) {
                out.assign(
                    std::istreambuf_iterator<char>(file),
                    std::istreambuf_iterator<char>()
                );
                return true;
            }

            const auto parent_path = raw_path->parent_path();

            {
                const auto bundle_file_data = bundles_.get_file_data(
                    parent_path.u8string(), raw_path->filename().u8string()
                );
                if (nullptr != bundle_file_data.first) {
                    out.assign(
                        bundle_file_data.first,
                        bundle_file_data.first + bundle_file_data.second
                    );
                    return true;
                }
            }

            std::vector<uint8_t> file_content;
            {
                file.open(parent_path, std::ios::binary);
                if (!file.is_open())
                    return false;

                file_content.assign(
                    std::istreambuf_iterator<char>(file),
                    std::istreambuf_iterator<char>()
                );
            }

            if (!bundles_.notify(parent_path.u8string(), file_content))
                return false;

            {
                const auto bundle_file_data = bundles_.get_file_data(
                    parent_path.u8string(), raw_path->filename().u8string()
                );
                if (nullptr != bundle_file_data.first) {
                    out.assign(
                        bundle_file_data.first,
                        bundle_file_data.first + bundle_file_data.second
                    );
                    return true;
                }
            }

            return false;
        }

    private:
        std::optional<fs::path> make_raw_path(const fs::path& i_path) const {
            const auto prefix_str = prefix_.u8string();
            const auto interf_path_str = i_path.u8string();
            if (interf_path_str.find(prefix_str) != 0) {
                return std::nullopt;
            } else {
                const auto suffix = interf_path_str.substr(prefix_str.size());
                const auto out = fs::u8path(root_.u8string() + '/' + suffix);
                return fs::absolute(out);
            }
        }

        fs::path make_i_path(const fs::path& raw_path) const {
            const auto rel_path = fs::relative(raw_path, root_);
            return prefix_ / rel_path;
        }

        fs::path root_;
        fs::path prefix_;
        dal::BundleRepository& bundles_;
    };

}  // namespace


namespace dal {

    std::unique_ptr<IFileSubsys> create_filesubsys_std(
        const std::string& prefix, const fs::path& root, Filesystem& filesys
    ) {
        return std::make_unique<FileSubsysStd>(
            prefix, root, filesys.bundle_repo()
        );
    }

}  // namespace dal


namespace dal {

    class Filesystem::Impl {

    public:
        std::vector<fs::path> list_files(const fs::path& path) {
            std::vector<fs::path> output;

            for (const auto& subsys : subsys_) {
                const auto files = subsys->list_files(path);
                output.insert(output.end(), files.begin(), files.end());
            }

            return output;
        }

        std::vector<fs::path> list_folders(const fs::path& path) {
            std::vector<fs::path> output;

            for (const auto& subsys : subsys_) {
                const auto folders = subsys->list_folders(path);
                output.insert(output.end(), folders.begin(), folders.end());
            }

            return output;
        }

        void walk(const fs::path& fol_path, IDirWalker& walker, size_t depth) {
            const auto res = walker.on_folder(fol_path, depth);
            if (!res)
                return;

            for (const auto& subsys : subsys_) {
                for (const auto& file_path : subsys->list_files(fol_path)) {
                    if (!::walk_bundle(file_path, walker, *subsys, depth + 1))
                        walker.on_file(file_path, depth + 1);
                }
            }

            for (const auto& subsys : subsys_) {
                for (auto& folder_path : subsys->list_folders(fol_path)) {
                    this->walk(folder_path, walker, depth + 1);
                }
            }
        }

        BundleRepository bundles_;
        std::vector<std::unique_ptr<IFileSubsys>> subsys_;
    };


    Filesystem::Filesystem() : pimpl_(std::make_unique<Impl>()) {}
    Filesystem::~Filesystem() {}

    BundleRepository& Filesystem::bundle_repo() { return pimpl_->bundles_; }

    void Filesystem::add_subsys(std::unique_ptr<IFileSubsys> subsys) {
        pimpl_->subsys_.push_back(std::move(subsys));
    }

    bool Filesystem::is_file(const fs::path& path) {
        for (const auto& subsys : this->pimpl_->subsys_) {
            if (subsys->is_file(path)) {
                return true;
            }
        }

        return false;
    }

    std::vector<fs::path> Filesystem::list_files(const fs::path& path) {
        std::vector<fs::path> output;

        for (const auto& subsys : pimpl_->subsys_) {
            const auto files = subsys->list_files(path);
            output.insert(output.end(), files.begin(), files.end());
        }

        return output;
    }

    std::vector<fs::path> Filesystem::list_folders(const fs::path& path) {
        std::vector<fs::path> output;

        for (const auto& subsys : pimpl_->subsys_) {
            const auto folders = subsys->list_folders(path);
            output.insert(output.end(), folders.begin(), folders.end());
        }

        return output;
    }

    void Filesystem::walk(const fs::path& root, IDirWalker& visitor) {
        pimpl_->walk(root, visitor, 0);
    }

    bool Filesystem::read_file(
        const fs::path& path, std::vector<uint8_t>& out
    ) {
        for (const auto& subsys : this->pimpl_->subsys_) {
            if (subsys->read_file(path, out)) {
                return true;
            }
        }

        return false;
    }

    std::optional<std::vector<uint8_t>> Filesystem::read_file(
        const fs::path& path
    ) {
        std::vector<uint8_t> out;
        if (this->read_file(path, out))
            return out;
        else
            return std::nullopt;
    }

}  // namespace dal
