#include "daltools/model_parser.h"

#include <optional>

#include <nlohmann/json.hpp>
#include <libbase64.h>

#include "daltools/byte_tool.h"
#include "daltools/konst.h"
#include "daltools/compression.h"


namespace {

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
            return &this->m_data[index];
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

    void parse_vertex_buffer(const json_t& json_vbuf, scene_t::VertexBuffer& vertex_buf) {

    }

    void parse_mesh(const json_t& json_mesh, scene_t::Mesh& output_mesh) {
        output_mesh.m_name = json_mesh["name"];
        
        for (auto& x : json_mesh["vertices"]) {
            ::parse_vertex_buffer(x, output_mesh.m_vertices.emplace_back());
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

    void parse_mesh_actor(const json_t& json_data, scene_t::MeshActor& output) {
        output.m_mesh_name = json_data["mesh name"];
        ::parse_actor(json_data, output);
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

    void parse_scene(const json_t& json_data, scene_t& scene) {
        scene.m_name = json_data["name"];

        for (auto& x : json_data["meshes"]) {
            ::parse_mesh(x, scene.m_meshes.emplace_back());
        }

        for (auto& x : json_data["materials"]) {
            ::parse_material(x, scene.m_materials.emplace_back());
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
            ::parse_scene(x, scenes.emplace_back());
        }

        return JsonParseResult::success;
    }

}
