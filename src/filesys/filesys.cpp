#include "daltools/filesys/filesys.hpp"

#include <fstream>


namespace {

    namespace fs = std::filesystem;


    class FileSubsysStd : public dal::IFileSubsys {

    public:
        FileSubsysStd(const std::string& prefix, const fs::path& root)
            : prefix_{ prefix }, root_{ root } {}

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

        bool read_file(const fs::path& i_path, bindata_t& out) override {
            const auto raw_path = this->make_raw_path(i_path);
            if (!raw_path.has_value())
                return false;

            std::ifstream file(*raw_path, std::ios::binary);
            out.assign(
                std::istreambuf_iterator<char>(file),
                std::istreambuf_iterator<char>()
            );
            return true;
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
    };

};  // namespace


namespace dal {

    std::unique_ptr<IFileSubsys> create_filesubsys_std(
        const std::string& prefix, const fs::path& root
    ) {
        return std::make_unique<FileSubsysStd>(prefix, root);
    }

}  // namespace dal


namespace dal {

    class Filesystem::Impl {

    public:
        std::vector<std::unique_ptr<IFileSubsys>> subsys_;
    };


    Filesystem::Filesystem() : pimpl_(std::make_unique<Impl>()) {}
    Filesystem::~Filesystem() {}

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
