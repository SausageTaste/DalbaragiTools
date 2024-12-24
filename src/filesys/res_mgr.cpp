#include "daltools/filesys/res_mgr.hpp"

#include <map>
#include <set>
#include <unordered_map>
#include <variant>

#include <spdlog/spdlog.h>

#include "daltools/dmd/parser.h"


namespace {

    namespace fs = dal::fs;


    dal::ResType deduce_res_type(const fs::path& path) {
        static const std::map<dal::ResType, std::set<std::string>> exts{
            { dal::ResType::img,
              { ".ktx", ".png", ".jpg", ".jpeg", ".bmp", ".tga" } },
            { dal::ResType::dmd, { ".dmd" } },
        };

        for (const auto& [type, ext_set] : exts) {
            if (ext_set.find(path.extension().u8string()) != ext_set.end())
                return type;
        }

        return dal::ResType::unknown;
    }


    class ResItem {

    public:
        ResItem(const fs::path& path) : path_(path) {}

        dal::ReqResult udpate(dal::Filesystem& filesys) {
            if (res_data_.index() != 0)
                return dal::ReqResult::ready;

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
                        if (this->try_parse_img())
                            return dal::ReqResult::ready;
                    case dal::ResType::dmd:
                        if (this->try_parse_dmd())
                            return dal::ReqResult::ready;
                    default:
                        break;
                }
            }

            if (this->try_parse_img())
                return dal::ReqResult::ready;
            if (this->try_parse_dmd())
                return dal::ReqResult::ready;

            return dal::ReqResult::not_supported_file;
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

        dal::IImage* get_img() {
            if (res_data_.index() == 1) {
                return std::get<1>(res_data_).get();
            } else {
                return nullptr;
            }
        }

        const dal::IImage* get_img() const {
            if (res_data_.index() == 1) {
                return std::get<1>(res_data_).get();
            } else {
                return nullptr;
            }
        }

        const dal::parser::Model* get_dmd() const {
            if (res_data_.index() == 2) {
                return &std::get<dal::parser::Model>(res_data_);
            } else {
                return nullptr;
            }
        }

    private:
        bool try_parse_img() {
            dal::ImageParseInfo pinfo;
            pinfo.file_path_ = path_.u8string();
            pinfo.data_ = reinterpret_cast<uint8_t*>(raw_data_.data());
            pinfo.size_ = raw_data_.size();
            pinfo.force_rgba_ = false;

            if (auto img = dal::parse_img(pinfo)) {
                res_data_ = std::move(img);
                return true;
            }

            return false;
        }

        bool try_parse_dmd() {
            res_data_ = dal::parser::Model{};
            const auto parse_result = dal::parser::parse_dmd(
                std::get<dal::parser::Model>(res_data_),
                reinterpret_cast<const uint8_t*>(raw_data_.data()),
                raw_data_.size()
            );

            if (parse_result != dal::parser::ModelParseResult::success) {
                res_data_ = std::monostate{};
                return false;
            }

            return true;
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

    private:
        using ReadLock = dal::IResourceManager::ReadLock;
        using WriteLock = dal::IResourceManager::WriteLock;

        template <typename TResType, typename TLockType>
        using ResLckOpt = dal::IResourceManager::ResLckOpt<TResType, TLockType>;

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

        using ImmutableImage = ResLckOpt<const dal::IImage, ReadLock>;
        ImmutableImage get_img(const fs::path& path) override {
            auto item = this->get(path);
            if (!item)
                return std::nullopt;
            auto image = item->get_img();
            if (!image)
                return std::nullopt;

            return std::make_pair(std::cref(*image), ReadLock{});
        }

        using MutableImage = ResLckOpt<dal::IImage, WriteLock>;
        MutableImage get_img_mut(const fs::path& path) override {
            auto item = this->get(path);
            if (!item)
                return std::nullopt;
            auto image = item->get_img();
            if (!image)
                return std::nullopt;

            return std::make_pair(std::ref(*image), WriteLock{});
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
