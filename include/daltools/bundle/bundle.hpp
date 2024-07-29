#pragma once

#include <string>


namespace dal {

    class BundleHeader {

    public:
        BundleHeader();

        uint64_t version() const noexcept { return version_; }
        uint64_t num_files() const noexcept { return num_files_; }
        uint64_t file_list_size() const noexcept { return file_list_size_; }
        std::string created_datetime() const noexcept {
            return std::string{ created_datetime_, created_datetime_ + 32 };
        }

        void set_num_files(const uint64_t num) noexcept { num_files_ = num; }

    private:
        char magic_[8];
        uint64_t version_;
        uint64_t num_files_;
        uint64_t file_list_size_;
        char created_datetime_[32];
    };

}  // namespace dal
