#include "daltools/struct.h"


namespace dal::parser {

    bool Vertex::is_equal(const Vertex& other) const {
        return (
            this->m_position == other.m_position &&
            this->m_uv_coords == other.m_uv_coords &&
            this->m_normal == other.m_normal
        );
    }

    bool VertexJoint::is_equal(const VertexJoint& other) const {
        return (
            this->m_position == other.m_position &&
            this->m_uv_coords == other.m_uv_coords &&
            this->m_normal == other.m_normal &&
            this->m_joint_weights == other.m_joint_weights &&
            this->m_joint_indices == other.m_joint_indices
        );
    }

    bool Material::operator==(const Material& other) const {
        return (
            this->m_albedo_map    == other.m_albedo_map &&
            this->m_roughness_map == other.m_roughness_map &&
            this->m_metallic_map  == other.m_metallic_map &&
            this->m_normal_map    == other.m_normal_map &&
            this->m_emision_map   == other.m_emision_map &&
            this->m_roughness     == other.m_roughness &&
            this->m_metallic      == other.m_metallic
        );
    }


    void Mesh_Straight::concat(const Mesh_Straight& other) {
        this->m_vertices.insert(m_vertices.end(), other.m_vertices.begin(), other.m_vertices.end());
        this->m_texcoords.insert(m_texcoords.end(), other.m_texcoords.begin(), other.m_texcoords.end());
        this->m_normals.insert(m_normals.end(), other.m_normals.begin(), other.m_normals.end());
    }

    void Mesh_StraightJoint::concat(const Mesh_StraightJoint& other) {
        this->Mesh_Straight::concat(other);

        this->m_boneWeights.insert(m_boneWeights.end(), other.m_boneWeights.begin(), other.m_boneWeights.end());
        this->m_boneIndex.insert(m_boneIndex.end(), other.m_boneIndex.begin(), other.m_boneIndex.end());
    }


    jointID_t Skeleton::find_by_name(const std::string& name) const {
        for (jointID_t i = 0; i < this->m_joints.size(); ++i) {
            if (this->m_joints[i].m_name == name) {
                return i;
            }
        }

        return -1;
    }


    void AnimJoint::add_translate(float time, float x, float y, float z) {
        auto& added = this->m_translates.emplace_back();

        added.first = time;
        added.second = glm::vec3{ x, y, z };
    }

    void AnimJoint::add_rotation(float time, float w, float x, float y, float z) {
        auto& added = this->m_rotations.emplace_back();

        added.first = time;
        added.second = glm::quat{ w, x, y, z };
    }

    void AnimJoint::add_scale(float time, float x) {
        auto& added = this->m_scales.emplace_back();

        added.first = time;
        added.second = x;
    }

    bool AnimJoint::is_identity_transform() const {
        if (!this->m_translates.empty())
            return false;
        else if (!this->m_rotations.empty())
            return false;
        else if (!this->m_scales.empty())
            return false;
        else
            return true;
    }

}


// Animation
namespace dal::parser {

    std::optional<size_t> Animation::find_by_name(const char* const name) const {
        for (size_t i = 0; i < this->m_joints.size(); ++i) {
            if (this->m_joints[i].m_name == name) {
                return i;
            }
        }

        return std::nullopt;
    }

}


// SceneIntermediate
namespace dal::parser {

    std::optional<SceneIntermediate::Mesh> SceneIntermediate::find_mesh_by_name(const std::string& name) const {
        for (auto& mesh : this->m_meshes) {
            if (mesh.m_name == name) {
                return mesh;
            }
        }

        return std::nullopt;
    }

    std::optional<SceneIntermediate::Material> SceneIntermediate::find_material_by_name(const std::string& name) const {
        for (auto& x : this->m_materials) {
            if (x.m_name == name) {
                return x;
            }
        }

        return std::nullopt;
    }

}