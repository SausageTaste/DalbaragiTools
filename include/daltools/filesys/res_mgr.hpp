#pragma once

#include "daltools/filesys/filesys.hpp"
#include "daltools/img/img2d.hpp"
#include "daltools/scene/struct.h"


namespace dal {

    enum class ResType {
        img,
        dmd,
        unknown,
    };

    enum class ReqResult {
        ready,
        loading,
        not_supported_file,
        unknown_error,
    };


    class IResourceManager {

    public:
        using PTex = std::shared_ptr<dal::IImage2D>;

        virtual ~IResourceManager() = default;
        virtual ReqResult request(const fs::path& path) = 0;
        virtual ResType query_res_type(const fs::path& path) = 0;

        virtual const dal::IImage* get_img(const fs::path& path) = 0;
        virtual const dal::parser::Model* get_dmd(const fs::path& path) = 0;
    };


    std::unique_ptr<IResourceManager> create_resmgr(
        std::shared_ptr<Filesystem>& filesys
    );

}  // namespace dal
