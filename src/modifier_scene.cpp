#include "daltools/modifier.h"


namespace {

    namespace dalp = dal::parser;
    using scene_t = dalp::SceneIntermediate;

}


// apply_root_transform
namespace {

    void apply_transform(const glm::mat4& m, glm::vec3& v) {
        v = m * glm::vec4{ v, 1 };
    }

    void apply_transform(const glm::mat3& m, glm::quat& q) {
        static_assert(0 * sizeof(float) == offsetof(glm::quat, x));
        static_assert(1 * sizeof(float) == offsetof(glm::quat, y));
        static_assert(2 * sizeof(float) == offsetof(glm::quat, z));
        static_assert(3 * sizeof(float) == offsetof(glm::quat, w));
        static_assert(3 * sizeof(float) == sizeof(glm::vec3));

        auto& quat_rotation_part = *reinterpret_cast<glm::vec3*>(&q.x);
        quat_rotation_part = m * quat_rotation_part;
    }

}


// reduce_indexed_vertices
namespace {

    bool are_vertices_same(const scene_t::Vertex& one, const scene_t::Vertex& two) {
        if (one.m_pos != two.m_pos)
            return false;
        if (one.m_normal != two.m_normal)
            return false;
        if (one.uv_coord != two.uv_coord)
            return false;

        return true;
    }

    class IndexedMeshBuilder {

    public:
        std::vector<scene_t::Vertex> m_vertices;
        std::vector<size_t> m_indices;

    public:
        void add(const scene_t::Vertex& vertex) {
            const auto vert_size = this->m_vertices.size();

            for (size_t i = 0; i < vert_size; ++i) {
                if (::are_vertices_same(vertex, this->m_vertices[i])) {
                    this->m_indices.push_back(i);
                    return;
                }
            }

            this->m_indices.push_back(this->m_vertices.size());
            this->m_vertices.push_back(vertex);
        }

        void swap(scene_t::Mesh& mesh) {
            std::swap(mesh.m_vertices, this->m_vertices);
            std::swap(mesh.m_indices, this->m_indices);
        }

    };

}


// convert_to_model_dmd
namespace {

    void convert_material(dalp::Material& dst, const dalp::SceneIntermediate::Material& src) {
        dst.m_roughness = src.m_roughness;
        dst.m_metallic = src.m_metallic;
        dst.alpha_blend = src.m_transparency;
        dst.m_albedo_map = src.m_albedo_map;
        dst.m_roughness_map = src.m_roughness_map;
        dst.m_metallic_map = src.m_metallic_map;
        dst.m_normal_map = src.m_normal_map;
    }

    void convert_skeleton(dalp::Skeleton& dst, const dalp::SceneIntermediate::Skeleton& src) {
        for (auto& src_joint : src.m_joints) {
            auto& dst_joint = dst.m_joints.emplace_back();

            dst_joint.m_name = src_joint.m_name;
            dst_joint.m_parent_index = dst.find_by_name(src_joint.m_parent_name);
            dst_joint.m_joint_type = src_joint.m_joint_type;
            dst_joint.m_offset_mat = src_joint.m_offset_mat;
        }
    }

    void convert_animation(dalp::Animation& dst, const dalp::SceneIntermediate::Animation& src) {
        dst.m_name = src.m_name;
        dst.m_ticks_par_sec = src.m_ticks_per_sec;
        dst.m_duration_tick = src.calc_duration_in_ticks();

        for (auto& src_joint : src.m_joints) {
            auto& dst_joint = dst.m_joints.emplace_back();
            dst_joint.m_name = src_joint.m_name;
            dst_joint.m_translates = src_joint.m_positions;
            dst_joint.m_rotations = src_joint.m_rotations;
            dst_joint.m_scales = src_joint.m_scales;
            dst_joint.m_transform = glm::mat4{1};
        }
    }

}


namespace dal::parser {

    void apply_root_transform(SceneIntermediate& scene) {
        const auto& root_m4 = scene.m_root_transform;
        const auto root_m4_inv = glm::inverse(root_m4);
        const auto root_m3 = glm::mat3{ root_m4 };

        for (auto& mesh : scene.m_meshes) {
            for (auto& vertex : mesh.m_vertices) {
                ::apply_transform(root_m4, vertex.m_pos);
                vertex.m_normal = root_m3 * vertex.m_normal;
            }
        }

        for (auto& skeleton : scene.m_skeletons) {
            for (auto& joint : skeleton.m_joints) {
                joint.m_offset_mat = root_m4 * joint.m_offset_mat * root_m4_inv;
            }
        }

        for (auto& animation : scene.m_animations) {
            for (auto& joint : animation.m_joints) {
                for (auto& x : joint.m_positions) {
                    ::apply_transform(root_m4, x.second);
                }

                for (auto& x : joint.m_rotations) {
                    ::apply_transform(root_m3, x.second);
                }
            }
        }

        for (auto& mesh_actor : scene.m_mesh_actors) {
            ::apply_transform(root_m4, mesh_actor.m_transform.m_pos);
            ::apply_transform(root_m3, mesh_actor.m_transform.m_quat);
        }

        for (auto& light : scene.m_dlights) {
            ::apply_transform(root_m4, light.m_transform.m_pos);
            ::apply_transform(root_m3, light.m_transform.m_quat);
        }

        for (auto& light : scene.m_plights) {
            ::apply_transform(root_m4, light.m_transform.m_pos);
            ::apply_transform(root_m3, light.m_transform.m_quat);
        }

        for (auto& light : scene.m_slights) {
            ::apply_transform(root_m4, light.m_transform.m_pos);
            ::apply_transform(root_m3, light.m_transform.m_quat);
        }

        scene.m_root_transform = glm::mat4{1};
    }

    void reduce_indexed_vertices(SceneIntermediate& scene) {
        for (auto& mesh : scene.m_meshes) {
            IndexedMeshBuilder builder;
            for (auto& index : mesh.m_indices) {
                builder.add(mesh.m_vertices[index]);
            }
            builder.swap(mesh);
        }
    }


    Model convert_to_model_dmd(const SceneIntermediate& scene) {
        Model output;

        for (auto& src_mesh_actor : scene.m_mesh_actors) {
            for (auto& pair : src_mesh_actor.m_render_pairs) {
                auto& src_mesh = scene.find_mesh_by_name(pair.m_mesh_name);

                if (src_mesh->m_skeleton_name.empty()) {
                    auto& dst_pair = output.m_units_indexed.emplace_back();
                    dst_pair.m_name = src_mesh->m_name;

                    for (auto& src_vert : src_mesh->m_vertices) {
                        auto& dst_vert = dst_pair.m_mesh.m_vertices.emplace_back();
                        dst_vert.m_position = src_vert.m_pos;
                        dst_vert.m_uv_coords = src_vert.uv_coord;
                        dst_vert.m_normal = src_vert.m_normal;
                    }

                    for (auto src_index : src_mesh->m_indices) {
                        dst_pair.m_mesh.m_indices.push_back(src_index);
                    }

                    auto& src_material = scene.find_material_by_name(pair.m_material_name);
                    if (src_material.has_value())
                        ::convert_material(dst_pair.m_material, *src_material);
                }
                else {
                    auto& dst_pair = output.m_units_indexed_joint.emplace_back();
                    dst_pair.m_name = src_mesh->m_name;

                    for (auto& src_vert : src_mesh->m_vertices) {
                        auto& dst_vert = dst_pair.m_mesh.m_vertices.emplace_back();
                        dst_vert.m_position = src_vert.m_pos;
                        dst_vert.m_uv_coords = src_vert.uv_coord;
                        dst_vert.m_normal = src_vert.m_normal;

                        const int valid_joint_count = std::min<int>(4, src_vert.m_joints.size());
                        for (int i = 0; i < valid_joint_count; ++i) {
                            dst_vert.m_joint_indices[i] = src_vert.m_joints[i].m_index;
                            dst_vert.m_joint_weights[i] = src_vert.m_joints[i].m_weight;
                        }
                        for (int i = valid_joint_count; i < 4; ++i) {
                            dst_vert.m_joint_indices[i] = -1;
                        }
                    }

                    for (auto src_index : src_mesh->m_indices) {
                        dst_pair.m_mesh.m_indices.push_back(src_index);
                    }

                    auto& src_material = scene.find_material_by_name(pair.m_material_name);
                    if (src_material.has_value())
                        ::convert_material(dst_pair.m_material, *src_material);
                }
            }
        }

        if (scene.m_skeletons.size() > 1) {
            throw std::runtime_error{"Multiple skeletons are not supported"};
        }
        else if (scene.m_skeletons.size() == 1) {
            ::convert_skeleton(output.m_skeleton, scene.m_skeletons.back());
        }

        for (auto& src_anim : scene.m_animations) {
            ::convert_animation(output.m_animations.emplace_back(), src_anim);
        }

        return output;
    }

}
