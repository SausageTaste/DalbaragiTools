#include "daltools/model_exporter.h"

#include <zlib.h>

#include "daltools/byte_tool.h"
#include "daltools/konst.h"


namespace dalp = dal::parser;


namespace {

    class BinaryBuildBuffer {

    private:
        dalp::binary_buffer_t m_data;

    public:
        BinaryBuildBuffer& operator+=(const BinaryBuildBuffer& other) {
            this->m_data.insert(this->m_data.end(), other.m_data.begin(), other.m_data.end());
            return *this;
        }

        void append_bool8(const bool v) {
            this->m_data.push_back(v ? 1 : 0);
        }

        void append_int32(const int32_t v) {
            this->append_4_bytes(v);
        }

        void append_int32_array(const int32_t* const arr, const size_t arr_size) {
            this->append_4_bytes_array(arr, arr_size);
        }

        void append_float32(const float v) {
            this->append_4_bytes(v);
        }

        void append_float32_array(const float* const arr, const size_t arr_size) {
            this->append_4_bytes_array(arr, arr_size);
        }

        void append_float32_vector(const std::vector<float>& v) {
            this->append_4_bytes_array(v.data(), v.size());
        }

        void append_char(const char c) {
            this->m_data.push_back(c);
        }

        void append_str(const char* const str, const size_t str_size) {
            this->m_data.insert(this->m_data.end(), str, str + str_size);
            this->m_data.push_back(0);
        }

        void append_str(const std::string& str) {
            this->append_str(str.data(), str.size());
        }

        void append_mat4(const glm::mat4& mat) {
            float fbuf[16];

            for (uint32_t row = 0; row < 4; ++row) {
                for (uint32_t col = 0; col < 4; ++col) {
                    fbuf[4 * row + col] = mat[col][row];
                }
            }

            this->append_float32_array(fbuf, 16);
        }

        void append_raw_array(const uint8_t* const arr, const size_t arr_size) {
            this->m_data.insert(this->m_data.end(), arr, arr + arr_size);
        }

        //

        auto data() const {
            return this->m_data.data();
        }

        auto size() const {
            return this->m_data.size();
        }

        void reserve(const size_t reserve_size) {
            this->m_data.reserve(reserve_size);
        }

        auto&& move() {
            return std::move(this->m_data);
        }

    private:
        template <typename T>
        void append_4_bytes(const T v) {
            static_assert(4 == sizeof(T));

            const auto src_loc = reinterpret_cast<const uint8_t*>(&v);

            if (dalp::is_big_endian()) {
                this->m_data.push_back(src_loc[3]);
                this->m_data.push_back(src_loc[2]);
                this->m_data.push_back(src_loc[1]);
                this->m_data.push_back(src_loc[0]);
            }
            else {
                this->m_data.insert(this->m_data.end(), src_loc, src_loc + 4);
            }
        }

        template <typename T>
        void append_4_bytes_array(const T* const arr, const size_t size) {
            static_assert(4 == sizeof(T));

            if (dalp::is_big_endian()) {
                for (size_t i = 0; i < size; ++i) {
                    this->append_4_bytes(arr[i]);
                }
            }
            else {
                const auto src_loc = reinterpret_cast<const uint8_t*>(arr);
                this->m_data.insert(this->m_data.end(), src_loc, src_loc + 4 * size);
            }
        }

    };

}


namespace {

    size_t compress_zip(uint8_t* const dst, const size_t dst_size, const uint8_t* const src, const size_t src_size) {
        uLongf dst_len = dst_size;

        if (Z_OK == compress(dst, &dst_len, src, src_size)) {
            return dst_len;
        }
        else {
            return 0;
        }
    }

    std::optional<dalp::binary_buffer_t> compress_dal_model(const uint8_t* const src, const size_t src_size) {
        dalp::binary_buffer_t output(src_size + 64);

        const auto src_size_int32 = static_cast<int32_t>(src_size);
        const auto data_offset = dalp::MAGIC_NUMBER_SIZE + sizeof(int32_t);

        static_assert(4 == sizeof(int32_t));

        memcpy(output.data(), dalp::MAGIC_NUMBERS_DAL_MODEL, dalp::MAGIC_NUMBER_SIZE);
        memcpy(output.data() + dalp::MAGIC_NUMBER_SIZE, &src_size_int32, sizeof(int32_t));

        const auto result_size = ::compress_zip(output.data() + data_offset, output.size() - data_offset, src, src_size);
        if (0 == result_size) {
            return std::nullopt;
        }
        else {
            output.resize(result_size + data_offset);
            return output;
        }
    }

    void append_bin_aabb(::BinaryBuildBuffer& output, const dalp::AABB3& aabb) {
        output.append_float32(aabb.m_min.x);
        output.append_float32(aabb.m_min.y);
        output.append_float32(aabb.m_min.z);
        output.append_float32(aabb.m_max.x);
        output.append_float32(aabb.m_max.y);
        output.append_float32(aabb.m_max.z);
    }

}


// Build animations
namespace {

    ::BinaryBuildBuffer build_bin_skeleton(const dalp::Skeleton& skeleton) {
        ::BinaryBuildBuffer output;

        output.append_int32(skeleton.m_joints.size());

        for (size_t i = 0; i < skeleton.m_joints.size(); ++i) {
            const auto& joint = skeleton.m_joints[i];

            output.append_str(joint.m_name.data(), joint.m_name.size());
            output.append_int32(joint.m_parent_index);

            switch (joint.m_joint_type) {
                case dalp::JointType::basic:
                    output.append_int32(0);
                    break;
                case dalp::JointType::hair_root:
                    output.append_int32(1);
                    break;
                case dalp::JointType::skirt_root:
                    output.append_int32(2);
                    break;
                default:
                    assert(false);
            }

            output.append_mat4(joint.m_offset_mat);
        }

        return output;
    }

    ::BinaryBuildBuffer _build_bin_joint_keyframes(const dalp::AnimJoint& joint) {
        ::BinaryBuildBuffer output;

        output.append_str(joint.m_name);
        output.append_mat4(joint.m_transform);

        output.append_int32(joint.m_translates.size());
        for (auto& trans : joint.m_translates) {
            output.append_float32(trans.first);
            output.append_float32(trans.second.x);
            output.append_float32(trans.second.y);
            output.append_float32(trans.second.z);
        }

        output.append_int32(joint.m_rotations.size());
        for (auto& rot : joint.m_rotations) {
            output.append_float32(rot.first);
            output.append_float32(rot.second.x);
            output.append_float32(rot.second.y);
            output.append_float32(rot.second.z);
            output.append_float32(rot.second.w);
        }

        output.append_int32(joint.m_scales.size());
        for (auto& scale : joint.m_scales) {
            output.append_float32(scale.first);
            output.append_float32(scale.second);
        }

        return output;
    }

    ::BinaryBuildBuffer build_bin_animation(const std::vector<dalp::Animation>& animations) {
        ::BinaryBuildBuffer output;

        output.append_int32(animations.size());

        for (size_t i = 0; i < animations.size(); ++i) {
            auto& anim = animations[i];

            output.append_str(anim.m_name);
            output.append_float32(anim.m_duration_tick);
            output.append_float32(anim.m_ticks_par_sec);

            output.append_int32(anim.m_joints.size());

            for (auto& joint : anim.m_joints) {
                output += ::_build_bin_joint_keyframes(joint);
            }
        }

        return output;
    }

}


// Build render units
namespace {

    ::BinaryBuildBuffer build_bin_material(const dalp::Material& material) {
        ::BinaryBuildBuffer output;

        output.append_float32(material.m_roughness);
        output.append_float32(material.m_metallic);
        output.append_bool8(material.alpha_blend);
        output.append_str(material.m_albedo_map);
        output.append_str(material.m_roughness_map);
        output.append_str(material.m_metallic_map);
        output.append_str(material.m_normal_map);

        return output;
    }

    ::BinaryBuildBuffer build_bin_mesh_straight(const dalp::Mesh_Straight& mesh) {
        ::BinaryBuildBuffer output;

        assert(mesh.m_vertices.size() * 2 == mesh.m_texcoords.size() * 3);
        assert(mesh.m_vertices.size() == mesh.m_normals.size());
        assert(mesh.m_vertices.size() % 3 == 0);
        const auto num_vertices = mesh.m_vertices.size() / 3;

        output.append_int32(num_vertices);
        output.append_float32_vector(mesh.m_vertices);
        output.append_float32_vector(mesh.m_texcoords);
        output.append_float32_vector(mesh.m_normals);

        return output;
    }

    ::BinaryBuildBuffer build_bin_mesh_straight_joint(const dalp::Mesh_StraightJoint& mesh) {
        ::BinaryBuildBuffer output;

        assert(mesh.m_vertices.size() * dal::parser::NUM_JOINTS_PER_VERTEX == mesh.m_boneIndex.size() * 3);
        assert(mesh.m_vertices.size() * dal::parser::NUM_JOINTS_PER_VERTEX == mesh.m_boneWeights.size() * 3);

        output += ::build_bin_mesh_straight(mesh);

        output.append_float32_vector(mesh.m_boneWeights);
        output.append_int32_array(mesh.m_boneIndex.data(), mesh.m_boneIndex.size());

        return output;
    }

    ::BinaryBuildBuffer build_bin_mesh_indexed(const dalp::Mesh_Indexed& mesh) {
        ::BinaryBuildBuffer output;
        static_assert(32 == sizeof(dalp::Mesh_Indexed::VERT_TYPE));

        output.append_int32(mesh.m_vertices.size());
        for (auto& vert : mesh.m_vertices) {
            output.append_float32(vert.m_position.x);
            output.append_float32(vert.m_position.y);
            output.append_float32(vert.m_position.z);

            output.append_float32(vert.m_normal.x);
            output.append_float32(vert.m_normal.y);
            output.append_float32(vert.m_normal.z);

            output.append_float32(vert.m_uv_coords.x);
            output.append_float32(vert.m_uv_coords.y);
        }

        output.append_int32(mesh.m_indices.size());
        for (auto index : mesh.m_indices) {
            output.append_int32(index);
        }

        return output;
    }

    ::BinaryBuildBuffer build_bin_mesh_indexed_joint(const dalp::Mesh_IndexedJoint& mesh) {
        ::BinaryBuildBuffer output;
        static_assert(64 == sizeof(dalp::Mesh_IndexedJoint::VERT_TYPE));

        output.append_int32(mesh.m_vertices.size());
        for (auto& vert : mesh.m_vertices) {
            output.append_float32(vert.m_position.x);
            output.append_float32(vert.m_position.y);
            output.append_float32(vert.m_position.z);

            output.append_float32(vert.m_normal.x);
            output.append_float32(vert.m_normal.y);
            output.append_float32(vert.m_normal.z);

            output.append_float32(vert.m_uv_coords.x);
            output.append_float32(vert.m_uv_coords.y);

            output.append_float32(vert.m_joint_weights.x);
            output.append_float32(vert.m_joint_weights.y);
            output.append_float32(vert.m_joint_weights.z);
            output.append_float32(vert.m_joint_weights.w);

            output.append_int32(vert.m_joint_indices.x);
            output.append_int32(vert.m_joint_indices.y);
            output.append_int32(vert.m_joint_indices.z);
            output.append_int32(vert.m_joint_indices.w);
        }

        output.append_int32(mesh.m_indices.size());
        for (auto index : mesh.m_indices) {
            output.append_int32(index);
        }

        return output;
    }

}


namespace dal::parser {

    ModelExportResult build_binary_model(
        binary_buffer_t& output,
        const Model& input,
        const crypto::PublicKeySignature::SecretKey* const sign_key,
        crypto::PublicKeySignature* const sign_mgr
    ) {
        BinaryBuildBuffer buffer;

        ::append_bin_aabb(buffer, input.m_aabb);
        buffer += ::build_bin_skeleton(input.m_skeleton);
        buffer += ::build_bin_animation(input.m_animations);

        buffer.append_int32(input.m_units_straight.size());
        for (auto& unit : input.m_units_straight) {
            buffer.append_str(unit.m_name);
            buffer += ::build_bin_material(unit.m_material);
            buffer += ::build_bin_mesh_straight(unit.m_mesh);
        }

        buffer.append_int32(input.m_units_straight_joint.size());
        for (auto& unit : input.m_units_straight_joint) {
            buffer.append_str(unit.m_name);
            buffer += ::build_bin_material(unit.m_material);
            buffer += ::build_bin_mesh_straight_joint(unit.m_mesh);
        }

        buffer.append_int32(input.m_units_indexed.size());
        for (auto& unit : input.m_units_indexed) {
            buffer.append_str(unit.m_name);
            buffer += ::build_bin_material(unit.m_material);
            buffer += ::build_bin_mesh_indexed(unit.m_mesh);
        }

        buffer.append_int32(input.m_units_indexed_joint.size());
        for (auto& unit : input.m_units_indexed_joint) {
            buffer.append_str(unit.m_name);
            buffer += ::build_bin_material(unit.m_material);
            buffer += ::build_bin_mesh_indexed_joint(unit.m_mesh);
        }

        // Null terminated signature
        if (nullptr != sign_key && nullptr != sign_key) {
            const auto signature = sign_mgr->create_signature(buffer.data(), buffer.size(), *sign_key);
            if (!signature.has_value())
                return ModelExportResult::unknown_error;

            buffer.append_str(signature->make_hex_str());
        }
        else {
            buffer.append_char('\0');
        }

        output = buffer.move();
        return ModelExportResult::success;
    }

    std::optional<binary_buffer_t> build_binary_model(
        const Model& input,
        const crypto::PublicKeySignature::SecretKey* const sign_key,
        crypto::PublicKeySignature* const sign_mgr
    ) {
        binary_buffer_t result;

        if (ModelExportResult::success != build_binary_model(result, input, sign_key, sign_mgr)) {
            return std::nullopt;
        }
        else {
            return result;
        }
    }

    std::optional<binary_buffer_t> zip_binary_model(const uint8_t* const data, const size_t data_size) {
        return ::compress_dal_model(data, data_size);
    }

}
