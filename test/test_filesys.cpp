#include <gtest/gtest.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/xchar.h>
#include <sung/general/stringtool.hpp>

#include "daltools/filesys/filesys.hpp"
#include "daltools/filesys/res_mgr.hpp"
#include "daltools/img/backend/ktx.hpp"


namespace {

    namespace fs = dal::fs;

    std::optional<fs::path> find_root_path() {
        return dal::find_parent_path_that_has(".git");
    }


    TEST(DaltestFilesys, FileSubsysStd) {
        const auto root_path = ::find_root_path();
        ASSERT_TRUE(root_path.has_value());
        const auto test_path = root_path.value() / "test";

        auto filesys = std::make_shared<dal::Filesystem>();
        filesys->add_subsys(dal::create_filesubsys_std(":test", test_path));
        ASSERT_TRUE(filesys->is_file(":test/json/sponza.json"));

        sung::HDataCentral datacen = sung::create_data_central();
        auto res_mgr = dal::create_resmgr(filesys, datacen);

        while (res_mgr->request(":test/json/sponza.json") ==
               dal::ReqResult::loading) {
        }
        ASSERT_EQ(
            res_mgr->request(":test/json/sponza.json"),
            dal::ReqResult::not_supported_file
        );

        auto img_path = ":test/img/stripes.ktx";
        while (res_mgr->request(img_path) == dal::ReqResult::loading) {
        }
        ASSERT_EQ(res_mgr->request(img_path), dal::ReqResult::ready);
        ASSERT_EQ(res_mgr->get_dmd(img_path), nullptr);
        auto img = res_mgr->get_img(img_path);
        ASSERT_NE(img, nullptr);
        ASSERT_NE(dynamic_cast<const dal::KtxImage*>(img), nullptr);
    }

}  // namespace


int main(int argc, char** argv) {
    system("chcp 65001");
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
