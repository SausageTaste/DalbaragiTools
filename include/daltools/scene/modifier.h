#pragma once

#include <filesystem>

#include "daltools/scene/struct.h"


namespace dal::parser {

    Mesh_Indexed convert_to_indexed(const Mesh_Straight& input);

    Mesh_IndexedJoint convert_to_indexed(const Mesh_StraightJoint& input);


    std::vector<RenderUnit<Mesh_Straight>> merge_by_material(
        const std::vector<RenderUnit<Mesh_Straight>>& units
    );
    std::vector<RenderUnit<Mesh_StraightJoint>> merge_by_material(
        const std::vector<RenderUnit<Mesh_StraightJoint>>& units
    );
    std::vector<RenderUnit<Mesh_Indexed>> merge_by_material(
        const std::vector<RenderUnit<Mesh_Indexed>>& units
    );
    std::vector<RenderUnit<Mesh_IndexedJoint>> merge_by_material(
        const std::vector<RenderUnit<Mesh_IndexedJoint>>& units
    );

    enum class JointReductionResult { success, fail, needless };

    JointReductionResult reduce_joints(dal::parser::Model& model);


    // Optimize

    void apply_root_transform(SceneIntermediate& scene);

    void reduce_indexed_vertices(SceneIntermediate& scene);

    void remove_duplicate_materials(SceneIntermediate& scene);

    void reduce_joints(SceneIntermediate& scene);

    void merge_redundant_mesh_actors(SceneIntermediate& scene);

    void remove_empty_meshes(SceneIntermediate& scene);

    void split_by_transparency(
        SceneIntermediate& scene, const std::filesystem::path& path
    );

    inline void optimize_scene(
        SceneIntermediate& scene, const std::filesystem::path& path
    ) {
        reduce_indexed_vertices(scene);
        remove_duplicate_materials(scene);
        merge_redundant_mesh_actors(scene);
        split_by_transparency(scene, path);
        remove_empty_meshes(scene);
        reduce_joints(scene);
        apply_root_transform(scene);
    }

    // Modify

    void flip_uv_vertically(SceneIntermediate& scene);

    void clear_collection_info(SceneIntermediate& scene);

    // Convert

    Model convert_to_model_dmd(const SceneIntermediate& scene);

}  // namespace dal::parser
