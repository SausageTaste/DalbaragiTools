#include "daltools/model_parser.h"

#include <nlohmann/json.hpp>

#include "daltools/byte_tool.h"
#include "daltools/konst.h"
#include "daltools/compression.h"


namespace {

    using json_t = nlohmann::json;
    using scene_t = dal::parser::SceneIntermediate;


    void parse_mesh(const json_t& json_mesh, scene_t::Mesh& output_mesh) {

    }

    void parse_material(const json_t& json_mesh, scene_t::Material& output_material) {

    }

    void parse_mesh_actor(const json_t& json_mesh, scene_t::MeshActor& output_mesh_actor) {

    }

    void parse_dlight(const json_t& json_mesh, scene_t::DirectionalLight& output_dlight) {

    }

    void parse_plight(const json_t& json_mesh, scene_t::PointLight& output_plight) {

    }

    void parse_slight(const json_t& json_mesh, scene_t::Spotlight& output_slight) {

    }

    void parse_scene(const json_t& json_scene, scene_t& scene) {
        scene.m_name = json_scene["name"];

        for (auto& x : json_scene["meshes"]) {
            ::parse_mesh(x, scene.m_meshes.emplace_back());
        }

        for (auto& x : json_scene["materials"]) {
            ::parse_material(x, scene.m_materials.emplace_back());
        }

        for (auto& x : json_scene["mesh actors"]) {
            ::parse_mesh_actor(x, scene.m_mesh_actors.emplace_back());
        }

        for (auto& x : json_scene["directional lights"]) {
            ::parse_dlight(x, scene.m_dlights.emplace_back());
        }

        for (auto& x : json_scene["point lights"]) {
            ::parse_plight(x, scene.m_plights.emplace_back());
        }

        for (auto& x : json_scene["spotlights"]) {
            ::parse_slight(x, scene.m_slights.emplace_back());
        }
    }

}


namespace dal::parser {

    JsonParseResult parse_json(std::vector<SceneIntermediate>& scenes, const uint8_t* const file_content, const size_t content_size) {
        const auto json_data = nlohmann::json::parse(file_content, file_content + content_size);

        for (auto& x : json_data["scenes"]) {
            ::parse_scene(x, scenes.emplace_back());
        }

        return JsonParseResult::success;
    }

}
