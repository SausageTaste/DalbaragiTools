#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>

#include "daltools/model_parser.h"
#include "daltools/modifier.h"
#include "daltools/byte_tool.h"
#include "daltools/model_exporter.h"


#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define CHECK_TRUTH(condition) { if (!(condition)) std::cout << "(" << (condition) << ") " TOSTRING(condition) << std::endl; }

namespace dalp = dal::parser;


namespace {

    constexpr unsigned NANOSEC_PER_SEC = 1000000000;

    double get_cur_sec(void) {
        return static_cast<double>(std::chrono::steady_clock::now().time_since_epoch().count()) / static_cast<double>(NANOSEC_PER_SEC);
    }

    std::vector<std::string> get_all_dir_within_folder(std::string folder) {
        std::vector<std::string> names;

        for (const auto & entry : std::filesystem::directory_iterator(folder)) {
            names.push_back(entry.path().filename().string());
        }

        return names;
    }

    std::string find_root_path() {
        std::string current_dir = ".";

        for (int i = 0; i < 10; ++i) {
            for (const auto& x : ::get_all_dir_within_folder(current_dir)) {
                if ( x == ".git" ) {
                    return current_dir;
                }
            }

            current_dir += "/..";
        }

        throw std::runtime_error{ "failed to find root folder." };
    }


    std::vector<uint8_t> read_file(const char* const path) {
        std::ifstream file{ path, std::ios::ate | std::ios::binary | std::ios::in };

        if ( !file.is_open() ) {
            throw std::runtime_error(std::string{"failed to open file: "} + path);
        }

        const auto fileSize = static_cast<size_t>(file.tellg());
        std::vector<uint8_t> buffer;
        buffer.resize(fileSize);

        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        file.close();

        return buffer;
    }

    double compare_binary_buffers(const std::vector<uint8_t>& one, const std::vector<uint8_t>& two) {
        if (one.size() != two.size()) {
            return 0.0;
        }

        size_t same_count = 0;

        for (size_t i = 0; i < one.size(); ++i) {
            if (one[i] == two[i]) {
                ++same_count;
            }
        }

        return static_cast<double>(same_count) / static_cast<double>(one.size());
    }

    void compare_models(const dal::parser::Model& one, const dal::parser::Model& two) {
        CHECK_TRUTH(one.m_aabb.m_max == two.m_aabb.m_max);
        CHECK_TRUTH(one.m_aabb.m_min == two.m_aabb.m_min);

        CHECK_TRUTH(one.m_skeleton.m_joints.size() == two.m_skeleton.m_joints.size());
        for (size_t i = 0; i < std::min(one.m_skeleton.m_joints.size(), two.m_skeleton.m_joints.size()); ++i) {
            CHECK_TRUTH(one.m_skeleton.m_joints[i].m_name == two.m_skeleton.m_joints[i].m_name);
            CHECK_TRUTH(one.m_skeleton.m_joints[i].m_joint_type == two.m_skeleton.m_joints[i].m_joint_type);
            CHECK_TRUTH(one.m_skeleton.m_joints[i].m_offset_mat == two.m_skeleton.m_joints[i].m_offset_mat);
            CHECK_TRUTH(one.m_skeleton.m_joints[i].m_parent_index == two.m_skeleton.m_joints[i].m_parent_index);
        }

        CHECK_TRUTH(one.m_animations.size() == two.m_animations.size());
        for (size_t i = 0; i < std::min(one.m_animations.size(), two.m_animations.size()); ++i) {
            CHECK_TRUTH(one.m_animations[i].m_name == two.m_animations[i].m_name);
            CHECK_TRUTH(one.m_animations[i].m_duration_tick == two.m_animations[i].m_duration_tick);
            CHECK_TRUTH(one.m_animations[i].m_ticks_par_sec == two.m_animations[i].m_ticks_par_sec);

            CHECK_TRUTH(one.m_animations[i].m_joints.size() == two.m_animations[i].m_joints.size());
            for (size_t j = 0; j < std::min(one.m_animations[i].m_joints.size(), two.m_animations[i].m_joints.size()); ++j) {
                CHECK_TRUTH(one.m_animations[i].m_joints[j].m_name == two.m_animations[i].m_joints[j].m_name);
                CHECK_TRUTH(one.m_animations[i].m_joints[j].m_transform == two.m_animations[i].m_joints[j].m_transform);
                CHECK_TRUTH(one.m_animations[i].m_joints[j].m_translates == two.m_animations[i].m_joints[j].m_translates);
                CHECK_TRUTH(one.m_animations[i].m_joints[j].m_rotations == two.m_animations[i].m_joints[j].m_rotations);
                CHECK_TRUTH(one.m_animations[i].m_joints[j].m_scales == two.m_animations[i].m_joints[j].m_scales);
            }
        }

        CHECK_TRUTH(one.m_units_straight.size() == two.m_units_straight.size());
        for (size_t i = 0; i < std::min(one.m_units_straight.size(), two.m_units_straight.size()); ++i) {
            CHECK_TRUTH(one.m_units_straight[i].m_name == two.m_units_straight[i].m_name);
            CHECK_TRUTH(one.m_units_straight[i].m_material == two.m_units_straight[i].m_material);
            CHECK_TRUTH(one.m_units_straight[i].m_mesh.m_vertices == two.m_units_straight[i].m_mesh.m_vertices);
            CHECK_TRUTH(one.m_units_straight[i].m_mesh.m_texcoords == two.m_units_straight[i].m_mesh.m_texcoords);
            CHECK_TRUTH(one.m_units_straight[i].m_mesh.m_normals == two.m_units_straight[i].m_mesh.m_normals);
        }

        CHECK_TRUTH(one.m_units_straight_joint.size() == two.m_units_straight_joint.size());
        for (size_t i = 0; i < std::min(one.m_units_straight_joint.size(), two.m_units_straight_joint.size()); ++i) {
            CHECK_TRUTH(one.m_units_straight_joint[i].m_name == two.m_units_straight_joint[i].m_name);
            CHECK_TRUTH(one.m_units_straight_joint[i].m_material == two.m_units_straight_joint[i].m_material);
            CHECK_TRUTH(one.m_units_straight_joint[i].m_mesh.m_vertices == two.m_units_straight_joint[i].m_mesh.m_vertices);
            CHECK_TRUTH(one.m_units_straight_joint[i].m_mesh.m_texcoords == two.m_units_straight_joint[i].m_mesh.m_texcoords);
            CHECK_TRUTH(one.m_units_straight_joint[i].m_mesh.m_normals == two.m_units_straight_joint[i].m_mesh.m_normals);
        }

        CHECK_TRUTH(one.m_units_indexed.size() == two.m_units_indexed.size());
        CHECK_TRUTH(one.m_units_indexed_joint.size() == two.m_units_indexed_joint.size());
    }

}


namespace {

    void test_a_model(const char* const model_path) {
        std::cout << "< " << model_path << " >" << std::endl;

        const auto zipped = ::read_file(model_path);
        const auto unzipped = dal::parser::unzip_dmd(zipped.data(), zipped.size());
        const auto model = dal::parser::parse_dmd(unzipped->data(), unzipped->size());

        std::cout << "    * Loaded and parsed" << std::endl;
        std::cout << "        render units straight:       " << model->m_units_straight.size() << std::endl;
        std::cout << "        render units straight joint: " << model->m_units_straight_joint.size() << std::endl;
        std::cout << "        render units indexed:        " << model->m_units_indexed.size() << std::endl;
        std::cout << "        render units indexed joint:  " << model->m_units_indexed_joint.size() << std::endl;
        std::cout << "        joints: " << model->m_skeleton.m_joints.size() << std::endl;
        std::cout << "        animations: " << model->m_animations.size() << std::endl;
        std::cout << "        signature: " << model->m_signature_hex << std::endl;

        {
            const auto binary = dal::parser::build_binary_model(*model, nullptr, nullptr);
            const auto zipped_second = dalp::zip_binary_model(binary->data(), binary->size());
            const auto unzipped_second = dalp::unzip_dmd(zipped_second->data(), zipped_second->size());
            const auto model_second = dal::parser::parse_dmd(unzipped_second->data(), unzipped_second->size());

            std::cout << "    * Second model parsed" << std::endl;
            std::cout << "        render units straight:       " << model_second->m_units_straight.size() << std::endl;
            std::cout << "        render units straight joint: " << model_second->m_units_straight_joint.size() << std::endl;
            std::cout << "        render units indexed:        " << model_second->m_units_indexed.size() << std::endl;
            std::cout << "        render units indexed joint:  " << model_second->m_units_indexed_joint.size() << std::endl;
            std::cout << "        joints: " << model_second->m_skeleton.m_joints.size() << std::endl;
            std::cout << "        animations: " << model_second->m_animations.size() << std::endl;
            std::cout << "        signature: " << model_second->m_signature_hex << std::endl;

            std::cout << "    * Built binary" << std::endl;
            std::cout << "        original zipped   binary size: " << zipped.size() << std::endl;
            std::cout << "        original unzipped binary size: " << unzipped->size() << std::endl;
            std::cout << "        built    zipped   binary size: " << zipped_second->size() << std::endl;
            std::cout << "        built    unzipped binary size: " << unzipped_second->size() << std::endl;
            std::cout << "        compare: " << ::compare_binary_buffers(*unzipped_second, *unzipped) << std::endl;

            ::compare_models(*model, *model_second);
        }

        {
            const auto merged_0 = dalp::merge_by_material(model->m_units_straight);
            const auto merged_1 = dalp::merge_by_material(model->m_units_straight_joint);
            const auto merged_2 = dalp::merge_by_material(model->m_units_indexed);
            const auto merged_3 = dalp::merge_by_material(model->m_units_indexed_joint);

            const auto before = model->m_units_straight.size() + model->m_units_straight_joint.size() + model->m_units_indexed.size() + model->m_units_indexed_joint.size();
            const auto after = merged_0.size() + merged_1.size() + merged_2.size() + merged_3.size();

            std::cout << "    * Merging by material" << std::endl;
            std::cout << "        before: " << before << std::endl;
            std::cout << "        after : " << after << std::endl;
        }

        {
            std::cout << "    * Reducing joints" << std::endl;
            auto model_cpy = model.value();
            std::cout << "        result: " << (dal::parser::JointReductionResult::fail != dalp::reduce_joints(model_cpy)) << std::endl;
        }

        constexpr double TEST_DURATION = 5.0;

        {

            std::cout << "    * Measuring import performance" << std::endl;

            const auto start_time = ::get_cur_sec();
            size_t process_count = 0;

            while (::get_cur_sec() - start_time < TEST_DURATION) {
                const auto model = dal::parser::parse_dmd(unzipped->data(), unzipped->size());
                ++process_count;
            }

            std::cout << "        processed " << static_cast<double>(process_count) / TEST_DURATION << " times per sec\n";
        }

        {
            std::cout << "    * Measuring export performance" << std::endl;

            const auto start_time = ::get_cur_sec();
            size_t process_count = 0;

            while (::get_cur_sec() - start_time < TEST_DURATION) {
                const auto binary = dal::parser::build_binary_model(*model, nullptr, nullptr);
                ++process_count;
            }

            std::cout << "        processed " << static_cast<double>(process_count) / TEST_DURATION << " times per sec\n";
        }
    }

    void test_a_model(const std::string& model_path) {
        ::test_a_model(model_path.c_str());
    }

    void create_indexed_model(const char* const dst_path, const char* const src_path) {
        std::cout << "Convert " << src_path << " to indexed model to " << dst_path;

        const auto model_data = ::read_file(dst_path);
        const auto unzipped = dal::parser::unzip_dmd(model_data.data(), model_data.size());
        auto model = dal::parser::parse_dmd(unzipped->data(), unzipped->size());

        for (const auto& unit : model->m_units_straight) {
            dalp::RenderUnit<dalp::Mesh_Indexed> new_unit;
            new_unit.m_name = unit.m_name;
            new_unit.m_material = unit.m_material;
            new_unit.m_mesh = dal::parser::convert_to_indexed(unit.m_mesh);
            model->m_units_indexed.push_back(new_unit);
        }
        model->m_units_straight.clear();

        for (const auto& unit : model->m_units_straight_joint) {
            dalp::RenderUnit<dalp::Mesh_IndexedJoint> new_unit;
            new_unit.m_name = unit.m_name;
            new_unit.m_material = unit.m_material;
            new_unit.m_mesh = dal::parser::convert_to_indexed(unit.m_mesh);
            model->m_units_indexed_joint.push_back(new_unit);
        }
        model->m_units_straight_joint.clear();

        const auto binary_built = dalp::build_binary_model(*model, nullptr, nullptr);
        const auto zipped = dalp::zip_binary_model(binary_built->data(), binary_built->size());

        std::ofstream file(src_path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(zipped->data()), zipped->size());
        file.close();

        std::cout << " -> Done" << std::endl;
    }

    void create_indexed_model(const std::string& dst_path, const std::string& src_path) {
        ::create_indexed_model(dst_path.c_str(), src_path.c_str());
    }

}


int main() {
    for (auto entry : std::filesystem::directory_iterator(::find_root_path() + "/test")) {
        if (entry.path().extension().string() == ".dmd") {
            std::cout << std::endl;
            ::test_a_model(entry.path().string());
        }
    }
}
