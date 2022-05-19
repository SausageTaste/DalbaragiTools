#pragma once

#include <map>
#include <vector>
#include <string>
#include <optional>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>


namespace dal::parser {

    using jointID_t = int32_t;

    constexpr jointID_t NULL_JID = -1;


    struct AABB3 {
        glm::vec3 m_min, m_max;
    };


    class Transform {

    public:
        glm::vec3 m_pos;
        glm::quat m_quat;
        float m_scale = 1.f;

    public:
        glm::mat4 Transform::make_mat4() const {
            const auto identity = glm::mat4{ 1 };
            const auto scale_mat = glm::scale(identity, glm::vec3{ this->m_scale, this->m_scale , this->m_scale });
            const auto translate_mat = glm::translate(identity, this->m_pos);

            return translate_mat * glm::mat4_cast(this->m_quat) * scale_mat;
        }

    };
    

    struct Vertex {
        glm::vec3 m_position;
        glm::vec3 m_normal;
        glm::vec2 m_uv_coords;

        bool operator==(const Vertex& other) const {
            return this->is_equal(other);
        }

        bool is_equal(const Vertex& other) const;
    };

    struct VertexJoint {
        glm::ivec4 m_joint_indices;
        glm::vec4 m_joint_weights;
        glm::vec3 m_position;
        glm::vec3 m_normal;
        glm::vec2 m_uv_coords;

        bool operator==(const VertexJoint& other) const {
            return this->is_equal(other);
        }

        bool is_equal(const VertexJoint& other) const;
    };

    static_assert(sizeof(VertexJoint::m_joint_indices) == sizeof(VertexJoint::m_joint_weights));
    constexpr int NUM_JOINTS_PER_VERTEX = sizeof(VertexJoint::m_joint_indices) / sizeof(float);


    struct Material {
        std::string m_albedo_map;
        std::string m_roughness_map;
        std::string m_metallic_map;
        std::string m_normal_map;
        std::string m_emision_map;
        float m_roughness = 0.5;
        float m_metallic = 1;
        bool alpha_blend = false;

        bool operator==(const Material& other) const;
    };


    struct Mesh_Straight {
        std::vector<float> m_vertices, m_texcoords, m_normals;

        void concat(const Mesh_Straight& other);
    };

    struct Mesh_StraightJoint : public Mesh_Straight {
        std::vector<float> m_boneWeights;
        std::vector<jointID_t> m_boneIndex;

        void concat(const Mesh_StraightJoint& other);
    };

    template <typename _Vertex>
    struct TMesh_Indexed {
        std::vector<_Vertex> m_vertices;
        std::vector<uint32_t> m_indices;

        using VERT_TYPE = _Vertex;

        void add_vertex(const _Vertex& vert) {
            for (size_t i = 0; i < this->m_vertices.size(); ++i) {
                if (vert == this->m_vertices[i]) {
                    this->m_indices.push_back(i);
                    return;
                }
            }

            this->m_indices.push_back(this->m_vertices.size());
            this->m_vertices.push_back(vert);
        }

        void concat(const TMesh_Indexed<_Vertex>& other) {
            for (const auto index : other.m_indices) {
                this->add_vertex(other.m_vertices[index]);
            }
        }
    };

    using Mesh_Indexed = TMesh_Indexed<Vertex>;

    using Mesh_IndexedJoint = TMesh_Indexed<VertexJoint>;


    template <typename _Mesh>
    struct RenderUnit {
        std::string m_name;
        _Mesh m_mesh;
        Material m_material;
    };

    enum class JointType {
        basic        = 0,
        hair_root    = 1,
        skirt_root   = 2,
    };

    struct SkelJoint {
        std::string m_name;
        jointID_t m_parent_index;
        JointType m_joint_type;
        glm::mat4 m_offset_mat;
    };

    struct Skeleton {
        std::vector<SkelJoint> m_joints;

        // Returns -1 if not found
        jointID_t find_by_name(const std::string& name) const;
    };

    struct AnimJoint {
        std::string m_name;
        glm::mat4 m_transform;  // i dont remember what this was...
        std::vector<std::pair<float, glm::vec3>> m_translates;
        std::vector<std::pair<float, glm::quat>> m_rotations;
        std::vector<std::pair<float, float>> m_scales;

        void add_translate(float time, float x, float y, float z);
        void add_rotation(float time, float w, float x, float y, float z);
        void add_scale(float time, float x);

        bool is_identity_transform() const;
    };


    class Animation {

    public:
        std::string m_name;
        std::vector<AnimJoint> m_joints;
        float m_duration_tick;
        float m_ticks_par_sec;

    public:
        std::optional<size_t> find_by_name(const char* const name) const;

        std::optional<size_t> find_by_name(const std::string& name) const {
            return this->find_by_name(name.c_str());
        }

    };


    struct Model {
        std::vector<RenderUnit<      Mesh_Straight >> m_units_straight;
        std::vector<RenderUnit< Mesh_StraightJoint >> m_units_straight_joint;
        std::vector<RenderUnit<       Mesh_Indexed >> m_units_indexed;
        std::vector<RenderUnit<  Mesh_IndexedJoint >> m_units_indexed_joint;

        std::vector<Animation> m_animations;
        Skeleton m_skeleton;
        AABB3 m_aabb;

        std::string m_signature_hex;
    };


    struct SceneIntermediate {

    public:
        struct IActor {
            std::string m_name;
            std::string m_parent_name;
            std::vector<std::string> m_collections;
            Transform m_transform;
            bool m_hidden = false;
        };


        struct VertexJointPair {
            jointID_t m_index = NULL_JID;
            float m_weight = 0.f;
        };


        struct Vertex {
            glm::vec3 m_pos;
            glm::vec2 uv_coord;
            glm::vec3 m_normal;
            std::vector<VertexJointPair> m_joints;
        };


        struct Mesh {
            std::string m_name;
            std::string m_skeleton_name;
            std::vector<Vertex> m_vertices;
        };


        struct Material {
            std::string m_name;

            float m_roughness = 0.5;
            float m_metallic = 0.0;
            bool m_transparency = false;

            std::string m_albedo_map;
            std::string m_roughness_map;
            std::string m_metallic_map;
            std::string m_normal_map;
        };


        struct RenderPair {
            std::string m_mesh_name;
            std::string m_material_name;
        };


        struct MeshActor : public IActor {
            std::vector<RenderPair> m_render_pairs;
        };


        struct ILight {
            glm::vec3 m_color;
            float m_intensity = 1000.f;
            bool m_has_shadow = false;
        };


        struct DirectionalLight : public IActor, public ILight {

        };


        struct PointLight : public IActor, public ILight {
            float m_max_distance = 0.f;
            float m_half_intense_distance = 0.f;
        };


        struct Spotlight : public PointLight {
            float m_spot_degree = 0.f;
            float m_spot_blend = 0.f;
        };


    public:
        std::string m_name;

        std::vector<Mesh> m_meshes;
        std::vector<Material> m_materials;

        std::vector<MeshActor> m_mesh_actors;
        std::vector<DirectionalLight> m_dlights;
        std::vector<PointLight> m_plights;
        std::vector<Spotlight> m_slights;

    public:
        std::optional<Mesh> find_mesh_by_name(const std::string& name) const;

        std::optional<Material> find_material_by_name(const std::string& name) const;

    };

}
