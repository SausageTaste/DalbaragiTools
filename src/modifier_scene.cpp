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
        void add(const std::string& froname_, const std::string& to_name) {
            if (froname_ == to_name) {
                throw std::runtime_error{"froname_ and to_name are identical, maybe ill-formed data"};
            }

            this->m_map.add(froname_, to_name);
        }

        auto& get(const std::string& froname_) const {
            return this->m_map.get(froname_);
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
        ::apply_transform(m4, t.pos_);
        ::apply_transform(m3, t.quat_);
        ::rotate_scale_factors(m3, t.scale_);
    }

}


// convert_to_model_dmd
namespace {

    template <typename T>
    dalp::RenderUnit<T>& find_or_create_render_unit_by_material(std::vector<dalp::RenderUnit<T>>& units, const scene_t::Material& criterion) {
        for (auto& unit : units) {
            if (unit.material_.is_physically_same(criterion)) {
                return unit;
            }
        }
        return units.emplace_back();
    }

    void convert_meshes(dalp::Model& output, const dalp::SceneIntermediate& scene) {
        for (auto& src_mesh_actor : scene.mesh_actors_) {
            const auto actor_mat4 = scene.make_hierarchy_transform(src_mesh_actor);
            const auto actor_mat3 = glm::mat3{actor_mat4};

            for (auto& pair : src_mesh_actor.render_pairs_) {
                const auto src_mesh = scene.find_mesh_by_name(pair.mesh_name_);
                if (nullptr == src_mesh) {
                    throw std::runtime_error{""};
                }
                const auto src_material = scene.find_material_by_name(pair.material_name_);
                if (nullptr == src_material) {
                    std::cout << "Failed to find a material: " << pair.material_name_ << '\n';
                    continue;
                }

                if (src_mesh->skeleton_name_.empty()) {
                    auto& dst_pair = ::find_or_create_render_unit_by_material(output.units_indexed_, *src_material);

                    dst_pair.name_ = src_mesh->name_;
                    dst_pair.material_ = *src_material;

                    for (auto src_index : src_mesh->indices_) {
                        auto& src_vert = src_mesh->vertices_[src_index];

                        dalp::Vertex vertex;
                        vertex.pos_ = actor_mat4 * glm::vec4{src_vert.pos_, 1};
                        vertex.uv_ = src_vert.uv_;
                        vertex.normal_ = actor_mat3 * src_vert.normal_;
                        dst_pair.mesh_.add_vertex(vertex);
                    }
                }
                else {
                    auto& dst_pair = ::find_or_create_render_unit_by_material(output.units_indexed_joint_, *src_material);

                    dst_pair.name_ = src_mesh->name_;
                    dst_pair.material_ = *src_material;

                    for (auto src_index : src_mesh->indices_) {
                        auto& src_vert = src_mesh->vertices_[src_index];

                        dalp::VertexJoint vertex;
                        vertex.pos_ = actor_mat4 * glm::vec4{src_vert.pos_, 1};
                        vertex.uv_ = src_vert.uv_;
                        vertex.normal_ = actor_mat3 * src_vert.normal_;

                        const int valid_joint_count = std::min<int>(4, src_vert.joints_.size());
                        for (int i = 0; i < valid_joint_count; ++i) {
                            vertex.joint_indices_[i] = src_vert.joints_[i].index_;
                            vertex.joint_weights_[i] = src_vert.joints_[i].weight_;
                        }
                        for (int i = valid_joint_count; i < dalp::NUM_JOINTS_PER_VERTEX; ++i) {
                            vertex.joint_indices_[i] = dalp::NULL_JID;
                        }

                        dst_pair.mesh_.add_vertex(vertex);
                    }
                }
            }
        }
    }

    void convert_skeleton(dalp::Skeleton& dst, const dalp::SceneIntermediate::Skeleton& src) {
        const auto root_mat = src.root_transform_.make_mat4();

        for (auto& src_joint : src.joints_) {
            auto& dst_joint = dst.joints_.emplace_back();

            dst_joint.name_ = src_joint.name_;
            dst_joint.parent_index_ = dst.find_by_name(src_joint.parent_name_);
            dst_joint.joint_type_ = src_joint.joint_type_;
            dst_joint.offset_mat_ = src_joint.offset_mat_;
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
            std::string name_, parent_name_;
        };

    public:
        const std::string NO_PARENT_NAME = "{%{%-1%}%}";

    private:
        std::vector<JointParentName> m_data;
        std::unordered_map<std::string, std::string> m_replace_map;

    public:
        void fill_joints(const scene_t::Skeleton& skeleton) {
            this->m_data.resize(skeleton.joints_.size());

            for (size_t i = 0; i < skeleton.joints_.size(); ++i) {
                auto& joint = skeleton.joints_[i];

                this->m_data[i].name_ = joint.name_;
                if (joint.has_parent())
                    this->m_data[i].parent_name_ = joint.parent_name_;
                else
                    this->m_data[i].parent_name_ = this->NO_PARENT_NAME;

                this->m_replace_map[joint.name_] = joint.name_;
            }
        }

        void remove_joint(const std::string& name) {
            const auto found_index = this->find_by_name(name);
            if (-1 == found_index)
                return;

            const auto parent_of_victim = this->m_data[found_index].parent_name_;
            this->m_data.erase(this->m_data.begin() + found_index);

            for (auto& joint : this->m_data) {
                if (joint.parent_name_ == name) {
                    joint.parent_name_ = parent_of_victim;
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
                output.insert(joint.name_);
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
                if (this->m_data[i].name_ == name) {
                    return i;
                }
            }

            return -1;
        }

    };


    bool are_skel_anim_compatible(const scene_t::Skeleton& skeleton, const scene_t::Animation& anim) {
        for (auto& joint : skeleton.joints_) {
            if (dal::parser::NULL_JID != anim.find_index_by_name(joint.name_)) {
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

        for (auto& joint : skeleton.joints_) {
            if (joint.is_root()) {
                output.insert(joint.name_);
            }
            else if (dal::parser::JointType::hair_root == joint.joint_type_ || dal::parser::JointType::skirt_root == joint.joint_type_) {
                super_parents.insert(joint.name_);
                output.insert(joint.name_);
            }
            else {
                if (super_parents.end() != super_parents.find(joint.parent_name_)) {
                    super_parents.insert(joint.name_);
                    output.insert(joint.name_);
                }
            }
        }

        return output;
    }

    ::str_set_t get_joint_names_with_non_identity_transform(const scene_t::Skeleton& skeleton, const std::vector<scene_t::Animation>& animations) {
        ::str_set_t output;

        if (skeleton.joints_.empty())
            return output;

        // Root nodes
        for (auto& joint : skeleton.joints_) {
            if (joint.is_root()) {
                output.insert(joint.name_);
            }
        }

        // Nodes with keyframes
        for (auto& anim : animations) {
            for (auto& joint : anim.joints_) {
                if (!joint.are_keyframes_empty()) {
                    output.insert(joint.name_);
                }
            }
        }

        return output;
    }

    scene_t::Skeleton make_new_skeleton(const scene_t::Skeleton& src_skeleton, const ::JointParentNameManager& jname_manager) {
        scene_t::Skeleton output;
        output.name_ = src_skeleton.name_;
        output.root_transform_ = src_skeleton.root_transform_;
        const auto survivor_joints = jname_manager.make_names_set();

        for (auto& src_joint : src_skeleton.joints_) {
            if (survivor_joints.end() == survivor_joints.find(src_joint.name_))
                continue;

            const std::string& parent_name = src_joint.has_parent() ? src_joint.parent_name_ : jname_manager.NO_PARENT_NAME;
            const auto& parent_replace_name = jname_manager.get_replaced_name(parent_name);
            output.joints_.push_back(src_joint);
        }

        for (auto& joint : output.joints_) {
            if (joint.is_root())
                continue;

            const auto& original_parent_name = joint.parent_name_;
            const auto& new_parent_name = jname_manager.get_replaced_name(original_parent_name);
            if (jname_manager.NO_PARENT_NAME != new_parent_name) {
                joint.parent_name_ = new_parent_name;
            }
            else {
                joint.parent_name_.clear();
            }
        }

        return output;
    }

    std::unordered_map<dalp::jointID_t, dalp::jointID_t> make_index_replace_map(
        const scene_t::Skeleton& froskeleton_,
        const scene_t::Skeleton& to_skeleton,
        const JointParentNameManager& jname_manager
    ) {
        std::unordered_map<dalp::jointID_t, dalp::jointID_t> output;
        output[-1] = -1;

        for (size_t i = 0; i < froskeleton_.joints_.size(); ++i) {
            const auto& froname_ = froskeleton_.joints_[i].name_;
            const auto& to_name = jname_manager.get_replaced_name(froname_);
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
            for (auto& vertex : mesh.vertices_) {
                for (auto& joint : vertex.joints_) {
                    const auto new_index = index_replace_map.find(joint.index_)->second;
                    assert(-1 <= new_index && new_index < static_cast<int64_t>(new_skeleton.joints_.size()));
                    joint.index_ = new_index;
                }
            }
        }

        return new_skeleton;
    }

}


namespace dal::parser {

    // Optimize

    void apply_root_transform(SceneIntermediate& scene) {
        const auto& root_m4 = scene.root_transform_;
        const auto root_m4_inv = glm::inverse(root_m4);
        const auto root_m3 = glm::mat3{ root_m4 };

        for (auto& mesh : scene.meshes_) {
            for (auto& vertex : mesh.vertices_) {
                ::apply_transform(root_m4, vertex.pos_);
                vertex.normal_ = root_m3 * vertex.normal_;
            }
        }

        for (auto& skeleton : scene.skeletons_) {
            for (auto& joint : skeleton.joints_) {
                joint.offset_mat_ = root_m4 * joint.offset_mat_ * root_m4_inv;
            }
        }

        for (auto& animation : scene.animations_) {
            for (auto& joint : animation.joints_) {
                for (auto& x : joint.translations_) {
                    ::apply_transform(root_m4, x.second);
                }

                for (auto& x : joint.rotations_) {
                    ::apply_transform(root_m3, x.second);
                }
            }
        }

        for (auto& mesh_actor : scene.mesh_actors_) {
            ::apply_transform(root_m4, root_m3, mesh_actor.transform_);
        }

        for (auto& light : scene.dlights_) {
            ::apply_transform(root_m4, root_m3, light.transform_);
        }

        for (auto& light : scene.plights_) {
            ::apply_transform(root_m4, root_m3, light.transform_);
        }

        for (auto& light : scene.slights_) {
            ::apply_transform(root_m4, root_m3, light.transform_);
        }

        scene.root_transform_ = glm::mat4{1};
    }

    void reduce_indexed_vertices(SceneIntermediate& scene) {
        for (auto& mesh : scene.meshes_) {
            scene_t::Mesh builder;
            for (auto& index : mesh.indices_)
                builder.add_vertex(mesh.vertices_[index]);

            std::swap(mesh.vertices_, builder.vertices_);
            std::swap(mesh.indices_, builder.indices_);
        }
    }

    void remove_duplicate_materials(SceneIntermediate& scene) {
        ::StringReplaceMap replace_map;

        {
            std::vector<::scene_t::Material> new_materials;

            for (auto& mat : scene.materials_) {
                const auto found = ::find_material_with_same_physical_properties(mat, new_materials);
                if (nullptr != found) {
                    assert(mat.name_ != found->name_);
                    replace_map.add(mat.name_, found->name_);
                }
                else {
                    new_materials.push_back(mat);
                }
            }

            std::swap(scene.materials_, new_materials);
        }

        for (auto& mesh_actor : scene.mesh_actors_) {
            for (auto& render_pair : mesh_actor.render_pairs_) {
                render_pair.material_name_ = replace_map.get(render_pair.material_name_);
            }
        }
    }

    void reduce_joints(SceneIntermediate& scene) {
        for (auto& skel : scene.skeletons_) {
            const auto result = ::reduce_joints(skel, scene.animations_, scene.meshes_);
            if (result.has_value()) {
                skel = result.value();
            }
        }
    }

    void merge_redundant_mesh_actors(SceneIntermediate& scene) {
        const auto mesh_actor_count = scene.mesh_actors_.size();

        for (size_t i = 1; i < mesh_actor_count; ++i) {
            auto& prey_actor = scene.mesh_actors_[i];

            for (size_t j = 0; j < i; ++j) {
                auto& dst_actor = scene.mesh_actors_[j];
                if (dst_actor.render_pairs_.empty())
                    continue;

                if (dst_actor.can_merge_with(prey_actor)) {
                    dst_actor.render_pairs_.insert(
                        dst_actor.render_pairs_.end(),
                        prey_actor.render_pairs_.begin(),
                        prey_actor.render_pairs_.end()
                    );
                    prey_actor.render_pairs_.clear();
                }
            }
        }
    }

    // Modify

    void flip_uv_vertically(SceneIntermediate& scene) {
        for (auto& mesh : scene.meshes_) {
            for (auto& vertex : mesh.vertices_) {
                vertex.uv_.y = 1.f - vertex.uv_.y;
            }
        }
    }

    void clear_collection_info(SceneIntermediate& scene) {
        for (auto& actor : scene.mesh_actors_)
            actor.collections_.clear();
        for (auto& actor : scene.dlights_)
            actor.collections_.clear();
        for (auto& actor : scene.plights_)
            actor.collections_.clear();
        for (auto& actor : scene.slights_)
            actor.collections_.clear();
    }

    // Convert

    Model convert_to_model_dmd(const SceneIntermediate& scene) {
        Model output;

        ::convert_meshes(output, scene);

        if (scene.skeletons_.size() > 1) {
            throw std::runtime_error{"Multiple skeletons are not supported"};
        }
        else if (scene.skeletons_.size() == 1) {
            ::convert_skeleton(output.skeleton_, scene.skeletons_.back());
        }

        for (auto& src_anim : scene.animations_) {
            output.animations_.push_back(src_anim);
        }

        return output;
    }

}
