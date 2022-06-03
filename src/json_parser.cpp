#include "daltools/model_parser.h"

#include <optional>

#include <nlohmann/json.hpp>
#include <libbase64.h>

#include "daltools/byte_tool.h"
#include "daltools/konst.h"
#include "daltools/compression.h"


namespace {

    namespace dalp = dal::parser;
    using json_t = nlohmann::json;
    using scene_t = dal::parser::SceneIntermediate;


    std::vector<uint8_t> decode_base64(const std::string& base64_data) {
        std::vector<uint8_t> output(base64_data.size());
        size_t output_size = output.size();

        const auto result = base64_decode(
            base64_data.c_str(),
            base64_data.size(),
            reinterpret_cast<char*>(output.data()),
            &output_size,
            0
        );

        if (1 != result) {
            output.clear();
        }
        else {
            output.resize(output_size);
        }

        return output;
    }


    class BinaryData {
        std::vector<uint8_t> m_data;

    public:
        auto ptr_at(const size_t index) const {
            return this->m_data.data() + index;
        }

        bool parse_from_json(const json_t& json_data) {
            const size_t raw_size = json_data["raw size"];

            auto base64_decoded = ::decode_base64(json_data["base64"]);
            if (base64_decoded.empty()) {
                return false;
            }

            if (json_data.contains("compressed size")) {
                this->m_data.resize(raw_size);
                const auto result = dal::decompress_zip(this->m_data.data(), this->m_data.size(), base64_decoded.data(), base64_decoded.size());
                if (dal::CompressResult::success != result.m_result) {
                    return false;
                }

                assert(result.m_output_size == this->m_data.size());
            }
            else {
                this->m_data = std::move(base64_decoded);
            }

            return true;
        }

    };


    void parse_vec3(const json_t& json_data, glm::vec3& output) {
        output[0] = json_data[0];
        output[1] = json_data[1];
        output[2] = json_data[2];
    }

    void parse_quat(const json_t& json_data, glm::quat& output) {
        output.w = json_data[0];
        output.x = json_data[1];
        output.y = json_data[2];
        output.z = json_data[3];
    }

    void parse_mat4(const json_t& json_data, glm::mat4& output) {
        const auto mat_ptr = &output[0][0];
        for (int i = 0; i < 16; ++i) {
            mat_ptr[i] = json_data[i];
        }
    }

    glm::mat4 parse_mat4(const json_t& json_data) {
        glm::mat4 output;
        ::parse_mat4(json_data, output);
        return output;
    }

    void parse_transform(const json_t& json_data, dal::parser::Transform& output) {
        ::parse_vec3(json_data["translation"], output.m_pos);
        ::parse_quat(json_data["rotation"], output.m_quat);
        output.m_scale = json_data["scale"];
    }

    template <typename T>
    void parse_list(const json_t& json_data, std::vector<T>& output) {
        for (auto& x : json_data) {
            output.push_back(x);
        }
    }

    void parse_actor(const json_t& json_data, scene_t::IActor& actor) {
        actor.m_name = json_data["name"];
        actor.m_parent_name = json_data["parent name"];
        ::parse_list(json_data["collections"], actor.m_collections);
        ::parse_transform(json_data["transform"], actor.m_transform);
        actor.m_hidden = json_data["hidden"];
    }

    void parse_mesh(const json_t& json_data, scene_t::Mesh& output, const ::BinaryData& binary_data) {
        output.m_name = json_data["name"];
        output.m_skeleton_name = json_data["skeleton name"];

        size_t vertex_count = json_data["vertex count"];
        output.m_vertices.resize(vertex_count);

        assert(!dal::parser::is_big_endian());
        static_assert(sizeof(float) * 2 == sizeof(glm::vec2));
        static_assert(sizeof(float) * 3 == sizeof(glm::vec3));

        {
            const size_t bin_pos = json_data["vertices binary data"]["position"];
            const size_t bin_size = json_data["vertices binary data"]["size"];

            for (size_t i = 0; i < vertex_count; ++i) {
                const auto ptr = binary_data.ptr_at(bin_pos + i * sizeof(glm::vec3));
                output.m_vertices[i].m_pos = *reinterpret_cast<const glm::vec3*>(ptr);
            }
        }

        {
            const size_t bin_pos = json_data["uv coordinates binary data"]["position"];
            const size_t bin_size = json_data["uv coordinates binary data"]["size"];

            for (size_t i = 0; i < vertex_count; ++i) {
                const auto ptr = binary_data.ptr_at(bin_pos + i * sizeof(glm::vec2));
                output.m_vertices[i].uv_coord = *reinterpret_cast<const glm::vec2*>(ptr);
            }
        }

        {
            const size_t bin_pos = json_data["normals binary data"]["position"];
            const size_t bin_size = json_data["normals binary data"]["size"];

            for (size_t i = 0; i < vertex_count; ++i) {
                const auto ptr = binary_data.ptr_at(bin_pos + i * sizeof(glm::vec3));
                output.m_vertices[i].m_normal = glm::normalize(*reinterpret_cast<const glm::vec3*>(ptr));
            }
        }

        {
            const size_t bin_pos = json_data["joints binary data"]["position"];
            const size_t bin_size = json_data["joints binary data"]["size"];

            auto ptr = binary_data.ptr_at(bin_pos);

            for (size_t i = 0; i < vertex_count; ++i) {
                auto& vertex = output.m_vertices[i];
                const auto joint_count = dalp::make_int32(ptr); ptr += 4;

                for (size_t j = 0; j < joint_count; ++j) {
                    auto& joint = vertex.m_joints.emplace_back();
                    joint.m_index = dalp::make_int32(ptr); ptr += 4;
                    joint.m_weight = dalp::make_float32(ptr); ptr += 4;
                }
            }

            assert(ptr == binary_data.ptr_at(bin_pos + bin_size));
        }

        {
            output.m_indices.resize(output.m_vertices.size());
            for (size_t i = 0; i < output.m_vertices.size(); ++i) {
                output.m_indices[i] = i;
            }
        }
    }

    void parse_material(const json_t& json_mesh, scene_t::Material& output_material) {
        output_material.m_name = json_mesh["name"];
        output_material.m_roughness = json_mesh["roughness"];
        output_material.m_metallic = json_mesh["metallic"];
        output_material.m_transparency = json_mesh["transparency"];
        output_material.m_albedo_map = json_mesh["albedo map"];
        output_material.m_roughness_map = json_mesh["roughness map"];
        output_material.m_metallic_map = json_mesh["metallic map"];
        output_material.m_normal_map = json_mesh["normal map"];
    }

    void parse_skeleton_joint(const json_t& json_data, scene_t::SkelJoint& output) {
        output.m_name = json_data["name"];
        output.m_parent_name = json_data["parent name"];
        output.m_joint_type = static_cast<dalp::JointType>(static_cast<int>(json_data["joint type"]));
        ::parse_mat4(json_data["offset matrix"], output.m_offset_mat);
    }

    void parse_skeleton(const json_t& json_data, scene_t::Skeleton& output) {
        output.m_name = json_data["name"];

        for (auto& x : json_data["joints"]) {
            ::parse_skeleton_joint(x, output.m_joints.emplace_back());
        }
    }

    void parse_anim_joint(const json_t& json_data, scene_t::AnimJoint& output) {
        output.m_name = json_data["name"];

        for (auto& x : json_data["positions"]) {
            const auto& value = x["value"];
            output.add_position(x["time point"], value[0], value[1], value[2]);
        }

        for (auto& x : json_data["rotations"]) {
            const auto& value = x["value"];
            output.add_rotation(x["time point"], value[0], value[1], value[2], value[3]);
        }

        for (auto& x : json_data["scales"]) {
            const auto& value = x["value"];
            output.add_scale(x["time point"], value);
        }
    }

    void parse_animation(const json_t& json_data, scene_t::Animation& output) {
        output.m_name = json_data["name"];
        output.m_ticks_per_sec = json_data["ticks per seconds"];

        for (auto& x : json_data["joints"]) {
            ::parse_anim_joint(x, output.m_joints.emplace_back());
        }
    }

    void parse_mesh_actor(const json_t& json_data, scene_t::MeshActor& output) {
        ::parse_actor(json_data, output);

        for (auto& x : json_data["render pairs"]) {
            auto& pair = output.m_render_pairs.emplace_back();
            pair.m_mesh_name = x["mesh name"];
            pair.m_material_name = x["material name"];
        }
    }

    void parse_ilight(const json_t& json_data, scene_t::ILight& output) {
        ::parse_vec3(json_data["color"], output.m_color);
        output.m_intensity = json_data["intensity"];
        output.m_has_shadow = json_data["has shadow"];
    }

    void parse_dlight(const json_t& json_data, scene_t::DirectionalLight& output) {
        ::parse_actor(json_data, output);
        ::parse_ilight(json_data, output);
    }

    void parse_plight(const json_t& json_data, scene_t::PointLight& output) {
        output.m_max_distance = json_data["max distance"];
        output.m_half_intense_distance = json_data["half intense distance"];

        ::parse_actor(json_data, output);
        ::parse_ilight(json_data, output);
    }

    void parse_slight(const json_t& json_data, scene_t::Spotlight& output) {
        ::parse_plight(json_data, output);

        output.m_spot_degree = json_data["spot degree"];
        output.m_spot_blend = json_data["spot blend"];
    }

    void parse_scene(const json_t& json_data, scene_t& scene, const ::BinaryData& binary_data) {
        scene.m_name = json_data["name"];
        scene.m_root_transform = ::parse_mat4(json_data["root transform"]);

        for (auto& x : json_data["meshes"]) {
            ::parse_mesh(x, scene.m_meshes.emplace_back(), binary_data);
        }

        for (auto& x : json_data["materials"]) {
            ::parse_material(x, scene.m_materials.emplace_back());
        }

        for (auto& x : json_data["skeletons"]) {
            ::parse_skeleton(x, scene.m_skeletons.emplace_back());
        }

        for (auto& x : json_data["animations"]) {
            ::parse_animation(x, scene.m_animations.emplace_back());
        }

        for (auto& x : json_data["mesh actors"]) {
            ::parse_mesh_actor(x, scene.m_mesh_actors.emplace_back());
        }

        for (auto& x : json_data["directional lights"]) {
            ::parse_dlight(x, scene.m_dlights.emplace_back());
        }

        for (auto& x : json_data["point lights"]) {
            ::parse_plight(x, scene.m_plights.emplace_back());
        }

        for (auto& x : json_data["spotlights"]) {
            ::parse_slight(x, scene.m_slights.emplace_back());
        }
    }

}


namespace dal::parser {

    JsonParseResult parse_json(std::vector<SceneIntermediate>& scenes, const uint8_t* const file_content, const size_t content_size) {
        const auto json_data = nlohmann::json::parse(file_content, file_content + content_size);

        ::BinaryData binary_data;
        binary_data.parse_from_json(json_data["binary data"]);

        for (auto& x : json_data["scenes"]) {
            ::parse_scene(x, scenes.emplace_back(), binary_data);
        }

        return JsonParseResult::success;
    }

}
