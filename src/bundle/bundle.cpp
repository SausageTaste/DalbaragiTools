#include "daltools/bundle/bundle.hpp"

#include <sung/general/time.hpp>


namespace dal {

    BundleHeader::BundleHeader()
        : magic_{ 0 }
        , version_{ 1 }
        , num_files_{ 0 }
        , file_list_size_{ 0 }
        , created_datetime_{ 0 } {
        static_assert(sizeof(char) == 1);
        static_assert(offsetof(BundleHeader, magic_) == 0);
        static_assert(offsetof(BundleHeader, version_) == 8);

        std::memcpy(this->magic_, "DALBUNDLE", 8);

        const auto now = sung::get_time_iso_utc();
        std::memcpy(
            created_datetime_,
            now.c_str(),
            std::min(now.size(), sizeof(this->created_datetime_))
        );
    }

}  // namespace dal
