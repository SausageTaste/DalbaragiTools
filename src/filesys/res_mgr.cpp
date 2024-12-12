#include "daltools/filesys/res_mgr.hpp"

#include <unordered_map>
#include <variant>

#include <spdlog/spdlog.h>

#include "daltools/dmd/parser.h"


namespace {

    namespace fs = dal::fs;


    dal::ResType deduce_res_type(const fs::path& path) {
        const auto ext = path.extension().u8string();
        if (ext == ".ktx" || ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
            ext == ".bmp" || ext == ".tga") {
            return dal::ResType::img;
        } else if (ext == ".dmd") {
            return dal::ResType::dmd;
        } else {
            return dal::ResType::unknown;
        }
    }


    class ResItem {

    public:
        ResItem(const fs::path& path) : path_(path) {}

        dal::ReqResult udpate(dal::Filesystem& filesys) {
            if (raw_data_.empty()) {
                filesys.read_file(path_, raw_data_);
                if (raw_data_.empty())
                    return dal::ReqResult::unknown_error;
                else
                    return dal::ReqResult::loading;
            }

            if (res_data_.index() == 0) {
                switch (::deduce_res_type(path_)) {
                    case dal::ResType::img:
                        this->try_parse_img();
                        return dal::ReqResult::loading;
                    case dal::ResType::dmd:
                        this->try_parse_dmd();
                        return dal::ReqResult::loading;
                    default:
                        return dal::ReqResult::not_supported_file;
                }
            }

            return dal::ReqResult::ready;
        }

        dal::ResType res_type() {
            switch (res_data_.index()) {
                case 1:
                    return dal::ResType::img;
                case 2:
                    return dal::ResType::dmd;
                default:
                    return dal::ResType::unknown;
            }
        }

        const dal::IImage* get_img() {
            if (res_data_.index() == 1) {
                return std::get<1>(res_data_).get();
            } else {
                return nullptr;
            }
        }

        const dal::parser::Model* get_dmd() {
            if (res_data_.index() == 2) {
                return &std::get<dal::parser::Model>(res_data_);
            } else {
                return nullptr;
            }
        }

    private:
        void try_parse_img() {
            dal::ImageParseInfo pinfo;
            pinfo.file_path_ = path_.u8string();
            pinfo.data_ = reinterpret_cast<uint8_t*>(raw_data_.data());
            pinfo.size_ = raw_data_.size();
            pinfo.force_rgba_ = false;

            if (auto img = dal::parse_img(pinfo)) {
                res_data_ = std::move(img);
            }
        }

        void try_parse_dmd() {
            res_data_ = dal::parser::Model{};
            const auto parse_result = dal::parser::parse_dmd(
                std::get<dal::parser::Model>(res_data_),
                reinterpret_cast<const uint8_t*>(raw_data_.data()),
                raw_data_.size()
            );

            if (parse_result != dal::parser::ModelParseResult::success) {
                res_data_ = std::monostate{};
            }
        }

        fs::path path_;
        std::vector<std::byte> raw_data_;
        std::variant<
            std::monostate,
            std::shared_ptr<dal::IImage>,
            dal::parser::Model>
            res_data_;
    };


    class ResourceManager : public dal::IResourceManager {

    public:
        ResourceManager(std::shared_ptr<dal::Filesystem>& filesys)
            : filesys_(filesys) {}

        dal::ReqResult request(const fs::path& respath) override {
            auto& item = this->get_or_create(respath);
            return item.udpate(*filesys_);
        }

        dal::ResType query_res_type(const fs::path& path) override {
            if (auto item = this->get(path))
                return item->res_type();

            return dal::ResType::unknown;
        }

        const dal::IImage* get_img(const fs::path& path) override {
            if (auto item = this->get(path))
                return item->get_img();

            return nullptr;
        }

        const dal::parser::Model* get_dmd(const fs::path& path) override {
            if (auto item = this->get(path))
                return item->get_dmd();

            return nullptr;
        }

    private:
        ResItem& get_or_create(const fs::path& path) {
            const auto path_str = path.u8string();
            auto it = items_.find(path_str);
            if (it != items_.end()) {
                return it->second;
            } else {
                items_.emplace(path_str, ResItem{ path });
                return items_.find(path_str)->second;
            }
        }

        ResItem* get(const fs::path& path) {
            const auto it = items_.find(path.u8string());
            if (it != items_.end()) {
                return &it->second;
            } else {
                return nullptr;
            }
        }

        std::shared_ptr<dal::Filesystem> filesys_;
        std::unordered_map<std::string, ResItem> items_;
    };

}  // namespace


namespace dal {

    std::unique_ptr<IResourceManager> create_resmgr(
        std::shared_ptr<Filesystem>& filesys
    ) {
        return std::make_unique<ResourceManager>(filesys);
    }

}  // namespace dal
