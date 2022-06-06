#include "daltools/modifier.h"

#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>


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

    template <typename T>
    T combine_abs_value_and_sign(const T only_abs_value, const T only_sign) {
        const auto ZERO = static_cast<T>(0);

        if (only_abs_value < ZERO) {
            if (only_sign < ZERO) {
                return only_abs_value;
            }
            else {
                return -only_abs_value;
            }
        }
        else {
            if (only_sign < ZERO) {
                return -only_abs_value;
            }
            else {
                return only_abs_value;
            }
        }
    }

    void rotate_scale_factors(const glm::mat3& m, glm::vec3& scales) {
        const auto rotated = m * scales;
        scales = glm::vec3{
            ::combine_abs_value_and_sign(rotated[0], scales[0]),
            ::combine_abs_value_and_sign(rotated[1], scales[1]),
            ::combine_abs_value_and_sign(rotated[2], scales[2])
        };
    }

    void apply_transform(const glm::mat4& m4, const glm::mat3& m3, scene_t::Transform& t) {
        ::apply_transform(m4, t.m_pos);
        ::apply_transform(m3, t.m_quat);
        ::rotate_scale_factors(m3, t.m_scale);
    }

}


// convert_to_model_dmd
namespace {

    template <typename T>
    dalp::RenderUnit<T>& find_or_create_render_unit_by_material(std::vector<dalp::RenderUnit<T>>& units, const scene_t::Material& criterion) {
        for (auto& unit : units) {
            if (unit.m_material.is_physically_same(criterion)) {
                return unit;
            }
        }
        return units.emplace_back();
    }

    void convert_meshes(dalp::Model& output, const dalp::SceneIntermediate& scene) {
        for (auto& src_mesh_actor : scene.m_mesh_actors) {
            const auto actor_mat4 = scene.make_hierarchy_transform(src_mesh_actor);
            const auto actor_mat3 = glm::mat3{actor_mat4};

            for (auto& pair : src_mesh_actor.m_render_pairs) {
                const auto src_mesh = scene.find_mesh_by_name(pair.m_mesh_name);
                if (nullptr == src_mesh) {
                    throw std::runtime_error{""};
                }
                const auto src_material = scene.find_material_by_name(pair.m_material_name);
                if (nullptr == src_material) {
                    std::cout << "Failed to find a material: " << pair.m_material_name << '\n';
                    continue;
                }

                if (src_mesh->m_skeleton_name.empty()) {
                    auto& dst_pair = ::find_or_create_render_unit_by_material(output.m_units_indexed, *src_material);

                    dst_pair.m_name = src_mesh->m_name;
                    dst_pair.m_material = *src_material;

                    for (auto src_index : src_mesh->m_indices) {
                        auto& src_vert = src_mesh->m_vertices[src_index];

                        dalp::Vertex vertex;
                        vertex.m_position = actor_mat4 * glm::vec4{src_vert.m_pos, 1};
                        vertex.m_uv_coords = src_vert.uv_coord;
                        vertex.m_normal = actor_mat3 * src_vert.m_normal;
                        dst_pair.m_mesh.add_vertex(vertex);
                    }
                }
                else {
                    auto& dst_pair = ::find_or_create_render_unit_by_material(output.m_units_indexed_joint, *src_material);

                    dst_pair.m_name = src_mesh->m_name;
                    dst_pair.m_material = *src_material;

                    for (auto src_index : src_mesh->m_indices) {
                        auto& src_vert = src_mesh->m_vertices[src_index];

                        dalp::VertexJoint vertex;
                        vertex.m_position = actor_mat4 * glm::vec4{src_vert.m_pos, 1};
                        vertex.m_uv_coords = src_vert.uv_coord;
                        vertex.m_normal = actor_mat3 * src_vert.m_normal;

                        const int valid_joint_count = std::min<int>(4, src_vert.m_joints.size());
                        for (int i = 0; i < valid_joint_count; ++i) {
                            vertex.m_joint_indices[i] = src_vert.m_joints[i].m_index;
                            vertex.m_joint_weights[i] = src_vert.m_joints[i].m_weight;
                        }
                        for (int i = valid_joint_count; i < dalp::NUM_JOINTS_PER_VERTEX; ++i) {
                            vertex.m_joint_indices[i] = dalp::NULL_JID;
                        }

                        dst_pair.m_mesh.add_vertex(vertex);
                    }
                }
            }
        }
    }

    void convert_skeleton(dalp::Skeleton& dst, const dalp::SceneIntermediate::Skeleton& src) {
        const auto root_mat = src.m_root_transform.make_mat4();

        for (auto& src_joint : src.m_joints) {
            auto& dst_joint = dst.m_joints.emplace_back();

            dst_joint.m_name = src_joint.m_name;
            dst_joint.m_parent_index = dst.find_by_name(src_joint.m_parent_name);
            dst_joint.m_joint_type = src_joint.m_joint_type;
            dst_joint.m_offset_mat = src_joint.m_offset_mat;
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


// reduce_joints
namespace {

    using str_set_t = std::unordered_set<std::string>;

    ::str_set_t make_set_union(const ::str_set_t& a, const ::str_set_t& b) {
        ::str_set_t output;
        output.insert(a.begin(), a.end());
        output.insert(b.begin(), b.end());
        return output;
    }

    ::str_set_t make_set_intersection(const ::str_set_t& a, const ::str_set_t& b) {
        ::str_set_t output;

        auto& smaller_set = a.size() < b.size() ? a : b;
        auto& larger_set = a.size() < b.size() ? b : a;

        for (auto iter = smaller_set.begin(); iter != smaller_set.end(); ++iter) {
            if (larger_set.end() != larger_set.find(*iter)) {
                output.insert(*iter);
            }
        }

        return output;
    }

    ::str_set_t make_set_difference(const ::str_set_t& a, const ::str_set_t& b) {
        ::str_set_t output;

        for (auto iter = a.begin(); iter != a.end(); ++iter) {
            if (b.end() == b.find(*iter)) {
                output.insert(*iter);
            }
        }

        return output;
    }


    class JointParentNameManager {

    private:
        struct JointParentName {
            std::string m_name, m_parent_name;
        };

    public:
        const std::string NO_PARENT_NAME = "{%{%-1%}%}";

    private:
        std::vector<JointParentName> m_data;
        std::unordered_map<std::string, std::string> m_replace_map;

    public:
        void fill_joints(const scene_t::Skeleton& skeleton) {
            this->m_data.resize(skeleton.m_joints.size());

            for (size_t i = 0; i < skeleton.m_joints.size(); ++i) {
                auto& joint = skeleton.m_joints[i];

                this->m_data[i].m_name = joint.m_name;
                if (joint.has_parent())
                    this->m_data[i].m_parent_name = joint.m_parent_name;
                else
                    this->m_data[i].m_parent_name = this->NO_PARENT_NAME;

                this->m_replace_map[joint.m_name] = joint.m_name;
            }
        }

        void remove_joint(const std::string& name) {
            const auto found_index = this->find_by_name(name);
            if (-1 == found_index)
                return;

            const auto parent_of_victim = this->m_data[found_index].m_parent_name;
            this->m_data.erase(this->m_data.begin() + found_index);

            for (auto& joint : this->m_data) {
                if (joint.m_parent_name == name) {
                    joint.m_parent_name = parent_of_victim;
                }
            }

            for (auto& iter : this->m_replace_map) {
                if (iter.second == name) {
                    iter.second = parent_of_victim;
                }
            }
        }

        void remove_except(const ::str_set_t& survivor_names) {
            const auto names_to_remove = ::make_set_difference(this->make_names_set(), survivor_names);

            for (auto& name : names_to_remove) {
                this->remove_joint(name);
            }
        }

        ::str_set_t make_names_set() const {
            ::str_set_t output;

            for (auto& joint : this->m_data) {
                output.insert(joint.m_name);
            }

            return output;
        }

        const std::string& get_replaced_name(const std::string& name) const {
            if (name == this->NO_PARENT_NAME) {
                return this->NO_PARENT_NAME;
            }
            else {
                return this->m_replace_map.find(name)->second;
            }
        }

    private:
        dalp::jointID_t find_by_name(const std::string& name) {
            if (this->NO_PARENT_NAME == name)
                return -1;

            for (dalp::jointID_t i = 0; i < this->m_data.size(); ++i) {
                if (this->m_data[i].m_name == name) {
                    return i;
                }
            }

            return -1;
        }

    };


    bool are_skel_anim_compatible(const scene_t::Skeleton& skeleton, const scene_t::Animation& anim) {
        for (auto& joint : skeleton.m_joints) {
            if (dal::parser::NULL_JID != anim.find_index_by_name(joint.m_name)) {
                return true;
            }
        }
        return false;
    }

    std::vector<const scene_t::Animation*> make_compatible_anim_list(const scene_t::Skeleton& skeleton, const std::vector<scene_t::Animation>& animations) {
        std::vector<const scene_t::Animation*> output;
        for (auto& anim : animations) {
            if (::are_skel_anim_compatible(skeleton, anim)) {
                output.push_back(&anim);
            }
        }
        return output;
    }

    ::str_set_t get_vital_joint_names(const scene_t::Skeleton& skeleton) {
        // Super parents' children are all vital
        ::str_set_t output, super_parents;

        for (auto& joint : skeleton.m_joints) {
            if (joint.is_root()) {
                output.insert(joint.m_name);
            }
            else if (dal::parser::JointType::hair_root == joint.m_joint_type || dal::parser::JointType::skirt_root == joint.m_joint_type) {
                super_parents.insert(joint.m_name);
                output.insert(joint.m_name);
            }
            else {
                if (super_parents.end() != super_parents.find(joint.m_parent_name)) {
                    super_parents.insert(joint.m_name);
                    output.insert(joint.m_name);
                }
            }
        }

        return output;
    }

    ::str_set_t get_joint_names_with_non_identity_transform(const scene_t::Skeleton& skeleton, const std::vector<scene_t::Animation>& animations) {
        ::str_set_t output;

        if (skeleton.m_joints.empty())
            return output;

        // Root nodes
        for (auto& joint : skeleton.m_joints) {
            if (joint.is_root()) {
                output.insert(joint.m_name);
            }
        }

        // Nodes with keyframes
        for (auto& anim : animations) {
            for (auto& joint : anim.m_joints) {
                if (!joint.are_keyframes_empty()) {
                    output.insert(joint.m_name);
                }
            }
        }

        return output;
    }

    scene_t::Skeleton make_new_skeleton(const scene_t::Skeleton& src_skeleton, const ::JointParentNameManager& jname_manager) {
        scene_t::Skeleton output;
        output.m_name = src_skeleton.m_name;
        const auto survivor_joints = jname_manager.make_names_set();

        for (auto& src_joint : src_skeleton.m_joints) {
            if (survivor_joints.end() == survivor_joints.find(src_joint.m_name))
                continue;

            const std::string& parent_name = src_joint.has_parent() ? src_joint.m_parent_name : jname_manager.NO_PARENT_NAME;
            const auto& parent_replace_name = jname_manager.get_replaced_name(parent_name);
            output.m_joints.push_back(src_joint);
        }

        for (auto& joint : output.m_joints) {
            if (joint.is_root())
                continue;

            const auto& original_parent_name = joint.m_parent_name;
            const auto& new_parent_name = jname_manager.get_replaced_name(original_parent_name);
            if (jname_manager.NO_PARENT_NAME != new_parent_name) {
                joint.m_parent_name = new_parent_name;
            }
            else {
                joint.m_parent_name.clear();
            }
        }

        return output;
    }

    std::unordered_map<dalp::jointID_t, dalp::jointID_t> make_index_replace_map(
        const scene_t::Skeleton& from_skeleton,
        const scene_t::Skeleton& to_skeleton,
        const JointParentNameManager& jname_manager
    ) {
        std::unordered_map<dalp::jointID_t, dalp::jointID_t> output;
        output[-1] = -1;

        for (size_t i = 0; i < from_skeleton.m_joints.size(); ++i) {
            const auto& from_name = from_skeleton.m_joints[i].m_name;
            const auto& to_name = jname_manager.get_replaced_name(from_name);
            const auto to_index = to_skeleton.find_index_by_name(to_name);
            assert(dalp::NULL_JID != to_index);
            output[i] = to_index;
        }

        return output;
    }

    std::optional<scene_t::Skeleton> reduce_joints(
        const scene_t::Skeleton& skeleton,
        const std::vector<scene_t::Animation>& animations,
        std::vector<scene_t::Mesh>& meshes
    ) {
        const auto compatible_animations = ::make_compatible_anim_list(skeleton, animations);
        if (compatible_animations.empty())
            return std::nullopt;

        const auto needed_joint_names = ::make_set_union(
            ::get_joint_names_with_non_identity_transform(skeleton, animations),
            ::get_vital_joint_names(skeleton)
        );

        ::JointParentNameManager joint_parent_names;
        joint_parent_names.fill_joints(skeleton);
        joint_parent_names.remove_except(needed_joint_names);

        const auto new_skeleton = ::make_new_skeleton(skeleton, joint_parent_names);
        const auto index_replace_map = ::make_index_replace_map(skeleton, new_skeleton, joint_parent_names);

        for (auto& mesh : meshes) {
            for (auto& vertex : mesh.m_vertices) {
                for (auto& joint : vertex.m_joints) {
                    const auto new_index = index_replace_map.find(joint.m_index)->second;
                    assert(-1 <= new_index && new_index < static_cast<int64_t>(new_skeleton.m_joints.size()));
                    joint.m_index = new_index;
                }
            }
        }

        return new_skeleton;
    }

}


namespace dal::parser {

    // Optimize

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
            ::apply_transform(root_m4, root_m3, mesh_actor.m_transform);
        }

        for (auto& light : scene.m_dlights) {
            ::apply_transform(root_m4, root_m3, light.m_transform);
        }

        for (auto& light : scene.m_plights) {
            ::apply_transform(root_m4, root_m3, light.m_transform);
        }

        for (auto& light : scene.m_slights) {
            ::apply_transform(root_m4, root_m3, light.m_transform);
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

    void reduce_joints(SceneIntermediate& scene) {
        for (auto& skel : scene.m_skeletons) {
            const auto result = ::reduce_joints(skel, scene.m_animations, scene.m_meshes);
            if (result.has_value()) {
                skel = result.value();
            }
        }
    }

    void merge_redundant_mesh_actors(SceneIntermediate& scene) {
        const auto mesh_actor_count = scene.m_mesh_actors.size();

        for (size_t i = 1; i < mesh_actor_count; ++i) {
            auto& prey_actor = scene.m_mesh_actors[i];

            for (size_t j = 0; j < i; ++j) {
                auto& dst_actor = scene.m_mesh_actors[j];
                if (dst_actor.m_render_pairs.empty())
                    continue;

                if (dst_actor.can_merge_with(prey_actor)) {
                    dst_actor.m_render_pairs.insert(
                        dst_actor.m_render_pairs.end(),
                        prey_actor.m_render_pairs.begin(),
                        prey_actor.m_render_pairs.end()
                    );
                    prey_actor.m_render_pairs.clear();
                }
            }
        }
    }

    // Modify

    void flip_uv_vertically(SceneIntermediate& scene) {
        for (auto& mesh : scene.m_meshes) {
            for (auto& vertex : mesh.m_vertices) {
                vertex.uv_coord.y = 1.f - vertex.uv_coord.y;
            }
        }
    }

    void clear_collection_info(SceneIntermediate& scene) {
        for (auto& actor : scene.m_mesh_actors)
            actor.m_collections.clear();
        for (auto& actor : scene.m_dlights)
            actor.m_collections.clear();
        for (auto& actor : scene.m_plights)
            actor.m_collections.clear();
        for (auto& actor : scene.m_slights)
            actor.m_collections.clear();
    }

    // Convert

    Model convert_to_model_dmd(const SceneIntermediate& scene) {
        Model output;

        ::convert_meshes(output, scene);

        if (scene.m_skeletons.size() > 1) {
            throw std::runtime_error{"Multiple skeletons are not supported"};
        }
        else if (scene.m_skeletons.size() == 1) {
            ::convert_skeleton(output.m_skeleton, scene.m_skeletons.back());
        }

        for (auto& src_anim : scene.m_animations) {
            output.m_animations.push_back(src_anim);
        }

        return output;
    }

}
