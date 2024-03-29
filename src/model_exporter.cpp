#include "daltools/model_exporter.h"

#include "daltools/byte_tool.h"
#include "daltools/konst.h"
#include "daltools/compression.h"


namespace dalp = dal::parser;


namespace {

    class BinaryBuildBuffer : public dalp::BinaryDataArray {

    public:
        void append_float32_vector(const std::vector<float>& v) {
            this->append_float32_array(v.data(), v.size());
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
            this->append_array(arr, arr_size);
        }

    };

}


namespace {

    std::optional<dalp::binary_buffer_t> compress_dal_model(const uint8_t* const src, const size_t src_size) {
        dalp::binary_buffer_t output(src_size + 64);

        const auto src_size_int32 = static_cast<int32_t>(src_size);
        const auto data_offset = dalp::MAGIC_NUMBER_SIZE + sizeof(int32_t);

        static_assert(4 == sizeof(int32_t));

        memcpy(output.data(), dalp::MAGIC_NUMBERS_DAL_MODEL, dalp::MAGIC_NUMBER_SIZE);
        memcpy(output.data() + dalp::MAGIC_NUMBER_SIZE, &src_size_int32, sizeof(int32_t));

        const auto result = dal::compress_zip(output.data() + data_offset, output.size() - data_offset, src, src_size);
        if (dal::CompressResult::success != result.m_result) {
            return std::nullopt;
        }
        else {
            output.resize(result.m_output_size + data_offset);
            return output;
        }
    }

    void append_bin_aabb(::BinaryBuildBuffer& output, const dalp::AABB3& aabb) {
        output.append_float32(aabb.min_.x);
        output.append_float32(aabb.min_.y);
        output.append_float32(aabb.min_.z);
        output.append_float32(aabb.max_.x);
        output.append_float32(aabb.max_.y);
        output.append_float32(aabb.max_.z);
    }

}


// Build animations
namespace {

    ::BinaryBuildBuffer build_bin_skeleton(const dalp::Skeleton& skeleton) {
        ::BinaryBuildBuffer output;

        output.append_mat4(skeleton.root_transform_);
        output.append_int32(skeleton.joints_.size());

        for (size_t i = 0; i < skeleton.joints_.size(); ++i) {
            const auto& joint = skeleton.joints_[i];

            output.append_str(joint.name_);
            output.append_int32(joint.parent_index_);

            switch (joint.joint_type_) {
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

            output.append_mat4(joint.offset_mat_);
        }

        return output;
    }

    ::BinaryBuildBuffer _build_bin_joint_keyframes(const dalp::AnimJoint& joint) {
        ::BinaryBuildBuffer output;

        output.append_str(joint.name_);
        output.append_mat4(glm::mat4{1});

        output.append_int32(joint.translations_.size());
        for (auto& trans : joint.translations_) {
            output.append_float32(trans.first);
            output.append_float32(trans.second.x);
            output.append_float32(trans.second.y);
            output.append_float32(trans.second.z);
        }

        output.append_int32(joint.rotations_.size());
        for (auto& rot : joint.rotations_) {
            output.append_float32(rot.first);
            output.append_float32(rot.second.x);
            output.append_float32(rot.second.y);
            output.append_float32(rot.second.z);
            output.append_float32(rot.second.w);
        }

        output.append_int32(joint.scales_.size());
        for (auto& scale : joint.scales_) {
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

            output.append_str(anim.name_);
            output.append_float32(anim.calc_duration_in_ticks());
            output.append_float32(anim.ticks_per_sec_);

            output.append_int32(anim.joints_.size());

            for (auto& joint : anim.joints_) {
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

        output.append_float32(material.roughness_);
        output.append_float32(material.metallic_);
        output.append_bool8(material.transparency_);
        output.append_str(material.albedo_map_);
        output.append_str(material.roughness_map_);
        output.append_str(material.metallic_map_);
        output.append_str(material.normal_map_);

        return output;
    }

    ::BinaryBuildBuffer build_bin_mesh_straight(const dalp::Mesh_Straight& mesh) {
        ::BinaryBuildBuffer output;

        assert(mesh.vertices_.size() * 2 == mesh.uv_coordinates_.size() * 3);
        assert(mesh.vertices_.size() == mesh.normals_.size());
        assert(mesh.vertices_.size() % 3 == 0);
        const auto nuvertices_ = mesh.vertices_.size() / 3;

        output.append_int32(nuvertices_);
        output.append_float32_vector(mesh.vertices_);
        output.append_float32_vector(mesh.uv_coordinates_);
        output.append_float32_vector(mesh.normals_);

        return output;
    }

    ::BinaryBuildBuffer build_bin_mesh_straight_joint(const dalp::Mesh_StraightJoint& mesh) {
        ::BinaryBuildBuffer output;

        assert(mesh.vertices_.size() * dal::parser::NUM_JOINTS_PER_VERTEX == mesh.joint_indices_.size() * 3);
        assert(mesh.vertices_.size() * dal::parser::NUM_JOINTS_PER_VERTEX == mesh.joint_weights_.size() * 3);

        output += ::build_bin_mesh_straight(mesh);

        output.append_float32_vector(mesh.joint_weights_);
        output.append_int32_array(mesh.joint_indices_.data(), mesh.joint_indices_.size());

        return output;
    }

    ::BinaryBuildBuffer build_bin_mesh_indexed(const dalp::Mesh_Indexed& mesh) {
        ::BinaryBuildBuffer output;
        static_assert(32 == sizeof(dalp::Mesh_Indexed::VERT_TYPE));

        output.append_int32(mesh.vertices_.size());
        for (auto& vert : mesh.vertices_) {
            output.append_float32(vert.pos_.x);
            output.append_float32(vert.pos_.y);
            output.append_float32(vert.pos_.z);

            output.append_float32(vert.normal_.x);
            output.append_float32(vert.normal_.y);
            output.append_float32(vert.normal_.z);

            output.append_float32(vert.uv_.x);
            output.append_float32(vert.uv_.y);
        }

        output.append_int32(mesh.indices_.size());
        for (auto index : mesh.indices_) {
            output.append_int32(index);
        }

        return output;
    }

    ::BinaryBuildBuffer build_bin_mesh_indexed_joint(const dalp::Mesh_IndexedJoint& mesh) {
        ::BinaryBuildBuffer output;
        static_assert(64 == sizeof(dalp::Mesh_IndexedJoint::VERT_TYPE));

        output.append_int32(mesh.vertices_.size());
        for (auto& vert : mesh.vertices_) {
            output.append_float32(vert.pos_.x);
            output.append_float32(vert.pos_.y);
            output.append_float32(vert.pos_.z);

            output.append_float32(vert.normal_.x);
            output.append_float32(vert.normal_.y);
            output.append_float32(vert.normal_.z);

            output.append_float32(vert.uv_.x);
            output.append_float32(vert.uv_.y);

            output.append_float32(vert.joint_weights_.x);
            output.append_float32(vert.joint_weights_.y);
            output.append_float32(vert.joint_weights_.z);
            output.append_float32(vert.joint_weights_.w);

            output.append_int32(vert.joint_indices_.x);
            output.append_int32(vert.joint_indices_.y);
            output.append_int32(vert.joint_indices_.z);
            output.append_int32(vert.joint_indices_.w);
        }

        output.append_int32(mesh.indices_.size());
        for (auto index : mesh.indices_) {
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

        ::append_bin_aabb(buffer, input.aabb_);
        buffer += ::build_bin_skeleton(input.skeleton_);
        buffer += ::build_bin_animation(input.animations_);

        buffer.append_int32(input.units_straight_.size());
        for (auto& unit : input.units_straight_) {
            buffer.append_str(unit.name_);
            buffer += ::build_bin_material(unit.material_);
            buffer += ::build_bin_mesh_straight(unit.mesh_);
        }

        buffer.append_int32(input.units_straight_joint_.size());
        for (auto& unit : input.units_straight_joint_) {
            buffer.append_str(unit.name_);
            buffer += ::build_bin_material(unit.material_);
            buffer += ::build_bin_mesh_straight_joint(unit.mesh_);
        }

        buffer.append_int32(input.units_indexed_.size());
        for (auto& unit : input.units_indexed_) {
            buffer.append_str(unit.name_);
            buffer += ::build_bin_material(unit.material_);
            buffer += ::build_bin_mesh_indexed(unit.mesh_);
        }

        buffer.append_int32(input.units_indexed_joint_.size());
        for (auto& unit : input.units_indexed_joint_) {
            buffer.append_str(unit.name_);
            buffer += ::build_bin_material(unit.material_);
            buffer += ::build_bin_mesh_indexed_joint(unit.mesh_);
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

        auto zipped = ::compress_dal_model(buffer.data(), buffer.size());
        if (!zipped.has_value())
            return ModelExportResult::compression_failure;

        output = std::move(zipped.value());
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
