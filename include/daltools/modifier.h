#pragma once

#include "daltools/struct.h"


namespace dal::parser {

    Mesh_Indexed convert_to_indexed(const Mesh_Straight& input);

    Mesh_IndexedJoint convert_to_indexed(const Mesh_StraightJoint& input);


    std::vector<RenderUnit<Mesh_Straight     >> merge_by_material(const std::vector<RenderUnit<Mesh_Straight     >>& units);
    std::vector<RenderUnit<Mesh_StraightJoint>> merge_by_material(const std::vector<RenderUnit<Mesh_StraightJoint>>& units);
    std::vector<RenderUnit<Mesh_Indexed      >> merge_by_material(const std::vector<RenderUnit<Mesh_Indexed      >>& units);
    std::vector<RenderUnit<Mesh_IndexedJoint >> merge_by_material(const std::vector<RenderUnit<Mesh_IndexedJoint >>& units);

    enum class JointReductionResult{ success, fail, needless };

    JointReductionResult reduce_joints(dal::parser::Model& model);


    void apply_root_transform(SceneIntermediate& scene);

    void reduce_indexed_vertices(SceneIntermediate& scene);

    void remove_duplicate_materials(SceneIntermediate& scene);

    void merge_by_material(SceneIntermediate& scene);


    Model convert_to_model_dmd(const SceneIntermediate& scene);

}
