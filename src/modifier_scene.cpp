#include "daltools/modifier.h"

#include <stdexcept>
#include <unordered_map>


namespace {

    namespace dalp = dal::parser;
    using scene_t = dalp::SceneIntermediate;


    template <typename T>
    class EasyMap {

    private:
        std::unordered_map<T, T> m_map;

    public:
        void add(const T& key, const T& value) {
            if (this->m_map.end() != this->m_map.find(key)) {
                throw std::runtime_error{"Trying to add a 'key' which already exists"};
            }

            this->m_map[key] = value;
        }

        auto& get(const std::string& key) const {
            const auto found = this->m_map.find(key);
            if (this->m_map.end() != found) {
                return found->second;
            }
            else {
                return key;
            }
        }

    };


    class StringReplaceMap {

    private:
        ::EasyMap<std::string> m_map;

    public:
        void add(const std::string& from_name, const std::string& to_name) {
            if (from_name == to_name) {
                throw std::runtime_error{"from_name and to_name are identical, maybe ill-formed data"};
            }

            this->m_map.add(from_name, to_name);
        }

        auto& get(const std::string& from_name) const {
            return this->m_map.get(from_name);
        }

    };

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


// remove_duplicate_materials
namespace {

    const scene_t::Material* find_material_with_same_physical_properties(const scene_t::Material& material, const std::vector<scene_t::Material>& list) {
        for (auto& x : list) {
            if (x.is_physically_same(material)) {
                return &x;
            }
        }
        return nullptr;
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
            scene_t::Mesh builder;
            for (auto& index : mesh.m_indices)
                builder.add_vertex(mesh.m_vertices[index]);

            std::swap(mesh.m_vertices, builder.m_vertices);
            std::swap(mesh.m_indices, builder.m_indices);
        }
    }

    void remove_duplicate_materials(SceneIntermediate& scene) {
        ::StringReplaceMap replace_map;

        {
            std::vector<::scene_t::Material> new_materials;

            for (auto& mat : scene.m_materials) {
                const auto found = ::find_material_with_same_physical_properties(mat, new_materials);
                if (nullptr != found) {
                    assert(mat.m_name != found->m_name);
                    replace_map.add(mat.m_name, found->m_name);
                }
                else {
                    new_materials.push_back(mat);
                }
            }

            std::swap(scene.m_materials, new_materials);
        }

        for (auto& mesh_actor : scene.m_mesh_actors) {
            for (auto& render_pair : mesh_actor.m_render_pairs) {
                render_pair.m_material_name = replace_map.get(render_pair.m_material_name);
            }
        }
    }


    Model convert_to_model_dmd(const SceneIntermediate& scene) {
        Model output;

        for (auto& src_mesh_actor : scene.m_mesh_actors) {
            for (auto& pair : src_mesh_actor.m_render_pairs) {
                const auto src_mesh = scene.find_mesh_by_name(pair.m_mesh_name);
                if (nullptr == src_mesh) {
                    continue;
                }

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

                    const auto src_material = scene.find_material_by_name(pair.m_material_name);
                    if (nullptr != src_material)
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

                    const auto src_material = scene.find_material_by_name(pair.m_material_name);
                    if (nullptr != src_material)
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
