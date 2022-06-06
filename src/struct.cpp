#include "daltools/struct.h"

#include <stdexcept>


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

}


//
namespace dal::parser {

    using scene_t = dal::parser::SceneIntermediate;


    bool scene_t::Vertex::are_same(const scene_t::Vertex& other) {
        if (this->m_pos != other.m_pos)
            return false;
        if (this->m_normal != other.m_normal)
            return false;
        if (this->uv_coord != other.uv_coord)
            return false;

        if (this->m_joints.size() != other.m_joints.size())
            return false;

        const auto joint_count = this->m_joints.size();
        for (size_t i = 0; i < joint_count; ++i) {
            auto& j0 = this->m_joints[i];
            auto& j1 = other.m_joints[i];

            if (j0.m_index != j1.m_index)
                return false;
            if (j0.m_weight != j1.m_weight)
                return false;
        }

        return true;
     }


    void scene_t::Mesh::add_vertex(const scene_t::Vertex& vertex) {
        const auto vert_size = this->m_vertices.size();
        for (size_t i = 0; i < vert_size; ++i) {
            if (this->m_vertices[i].are_same(vertex)) {
                this->m_indices.push_back(i);
                return;
            }
        }

        this->m_indices.push_back(this->m_vertices.size());
        this->m_vertices.push_back(vertex);
    }

    void scene_t::Mesh::concat(const scene_t::Mesh& other) {
        if (this->m_skeleton_name != other.m_skeleton_name)
            throw std::runtime_error{"Cannot concatenate meshes with different skeletons"};

        for (const auto index : other.m_indices) {
            this->add_vertex(other.m_vertices[index]);
        }
    }


    bool scene_t::Material::operator==(const scene_t::Material& rhs) const {
        if (!this->is_physically_same(rhs))
            return false;
        if (this->m_name != rhs.m_name)
            return false;

        return true;
    }

    bool scene_t::Material::is_physically_same(const scene_t::Material& other) const {
        if (this->m_roughness != other.m_roughness)
            return false;
        if (this->m_metallic != other.m_metallic)
            return false;
        if (this->m_transparency != other.m_transparency)
            return false;

        if (this->m_albedo_map != other.m_albedo_map)
            return false;
        if (this->m_roughness_map != other.m_roughness_map)
            return false;
        if (this->m_metallic_map != other.m_metallic_map)
            return false;
        if (this->m_normal_map != other.m_normal_map)
            return false;

        return true;
    }


    jointID_t scene_t::Skeleton::find_index_by_name(const std::string& name) const {
        for (jointID_t i = 0; i < this->m_joints.size(); ++i) {
            if (this->m_joints[i].m_name == name) {
                return i;
            }
        }
        return NULL_JID;
    }


    void scene_t::AnimJoint::add_position(float time, float x, float y, float z) {
        auto& added = this->m_positions.emplace_back();

        added.first = time;
        added.second = glm::vec3{ x, y, z };
    }

    void scene_t::AnimJoint::add_rotation(float time, float w, float x, float y, float z) {
        auto& added = this->m_rotations.emplace_back();

        added.first = time;
        added.second = glm::quat{ w, x, y, z };
    }

    void scene_t::AnimJoint::add_scale(float time, float x) {
        auto& added = this->m_scales.emplace_back();

        added.first = time;
        added.second = x;
    }

    float scene_t::AnimJoint::get_max_time_point() const {
        float max_value = 0;

        for (auto& x : this->m_positions)
            max_value = std::max(max_value, x.first);
        for (auto& x : this->m_rotations)
            max_value = std::max(max_value, x.first);
        for (auto& x : this->m_scales)
            max_value = std::max(max_value, x.first);

        return max_value;
    }

    bool scene_t::AnimJoint::are_keyframes_empty() const {
        if (!this->m_positions.empty())
            return false;
        if (!this->m_rotations.empty())
            return false;
        if (!this->m_scales.empty())
            return false;

        return true;
    }


    float scene_t::Animation::calc_duration_in_ticks() const {
        float max_value = 0.f;

        for (auto& joint : this->m_joints)
            max_value = std::max(max_value, joint.get_max_time_point());

        return 0.f != max_value ? max_value : 1.f;
    }

    jointID_t scene_t::Animation::find_index_by_name(const std::string& name) const {
        for (jointID_t i = 0; i < this->m_joints.size(); ++i) {
            if (this->m_joints[i].m_name == name) {
                return i;
            }
        }
        return NULL_JID;
    }


    bool scene_t::MeshActor::can_merge_with(const MeshActor& other) const {
        if (this->m_parent_name != other.m_parent_name)
            return false;
        if (this->m_collections != other.m_collections)
            return false;
        if (this->m_transform != other.m_transform)
            return false;
        if (this->m_hidden != other.m_hidden)
            return false;

        return true;
    }

}


// SceneIntermediate
namespace dal::parser {

    scene_t::Mesh* scene_t::find_mesh_by_name(const std::string& name) {
        for (auto& mesh : this->m_meshes) {
            if (mesh.m_name == name) {
                return &mesh;
            }
        }

        return nullptr;
    }

    const scene_t::Mesh* scene_t::find_mesh_by_name(const std::string& name) const {
        for (auto& mesh : this->m_meshes) {
            if (mesh.m_name == name) {
                return &mesh;
            }
        }

        return nullptr;
    }

    const scene_t::Material* scene_t::find_material_by_name(const std::string& name) const {
        for (auto& x : this->m_materials) {
            if (x.m_name == name) {
                return &x;
            }
        }

        return nullptr;
    }

    const scene_t::IActor* scene_t::find_actor_by_name(const std::string& name) const {
        for (auto& x : this->m_mesh_actors)
            if (x.m_name == name)
                return &x;

        for (auto& x : this->m_dlights)
            if (x.m_name == name)
                return &x;

        for (auto& x : this->m_plights)
            if (x.m_name == name)
                return &x;

        for (auto& x : this->m_slights)
            if (x.m_name == name)
                return &x;

        return nullptr;
    }

    glm::mat4 scene_t::make_hierarchy_transform(const scene_t::IActor& actor) const {
        glm::mat4 output = actor.m_transform.make_mat4();
        const IActor* cur_actor = &actor;

        while (!cur_actor->m_parent_name.empty()) {
            const auto& parent_name = cur_actor->m_parent_name;
            cur_actor = this->find_actor_by_name(parent_name);
            if (nullptr == cur_actor)
                throw std::runtime_error{"Failed to find a parent actor \"" + parent_name + '"'};

            output = cur_actor->m_transform.make_mat4() * output;
        }

        return output;
    }

}
