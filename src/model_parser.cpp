#include "daltools/model_parser.h"

#include "daltools/byte_tool.h"
#include "daltools/compression.h"
#include "daltools/konst.h"


namespace dalp = dal::parser;


namespace {

    std::optional<std::vector<uint8_t>> unzip_dal_model(
        const uint8_t* const buf, const size_t buf_size
    ) {
        const auto expected_unzipped_size = dal::parser::make_int64(
            buf + dalp::MAGIC_NUMBER_SIZE
        );
        const auto data_offset = dalp::MAGIC_NUMBER_SIZE + sizeof(int64_t);

        const auto unzipped = dal::decomp_bro(
            buf + data_offset, buf_size - data_offset, expected_unzipped_size
        );

        if (unzipped.has_value())
            return unzipped.value();
        else
            return std::nullopt;
    }

    bool is_magic_numbers_correct(const uint8_t* const buf) {
        for (int i = 0; i < dalp::MAGIC_NUMBER_SIZE; ++i) {
            if (buf[i] != dalp::MAGIC_NUMBERS_DAL_MODEL[i]) {
                return false;
            }
        }

        return true;
    }

}  // namespace


// Parser functions
namespace {

    const uint8_t* parse_aabb(
        const uint8_t* header,
        const uint8_t* const end,
        dal::parser::AABB3& output
    ) {
        float fbuf[6];
        header = dal::parser::assemble_4_bytes_array<float>(header, fbuf, 6);

        output.min_ = glm::vec3{ fbuf[0], fbuf[1], fbuf[2] };
        output.max_ = glm::vec3{ fbuf[3], fbuf[4], fbuf[5] };

        return header;
    }

    const uint8_t* parse_mat4(
        const uint8_t* header, const uint8_t* const end, glm::mat4& mat
    ) {
        float fbuf[16];
        header = dalp::assemble_4_bytes_array<float>(header, fbuf, 16);

        for (size_t row = 0; row < 4; ++row) {
            for (size_t col = 0; col < 4; ++col) {
                mat[col][row] = fbuf[4 * row + col];
            }
        }

        return header;
    }

}  // namespace


// Parse animations
namespace {

    const uint8_t* parse_skeleton(
        const uint8_t* header, const uint8_t* const end, dalp::Skeleton& output
    ) {
        header = ::parse_mat4(header, end, output.root_transform_);

        const auto joint_count = dal::parser::make_int32(header);
        header += 4;
        output.joints_.resize(joint_count);

        for (int i = 0; i < joint_count; ++i) {
            auto& joint = output.joints_.at(i);

            joint.name_ = reinterpret_cast<const char*>(header);
            header += joint.name_.size() + 1;
            joint.parent_index_ = dalp::make_int32(header);
            header += 4;

            const auto joint_type_index = dalp::make_int32(header);
            header += 4;
            switch (joint_type_index) {
                case 0:
                    joint.joint_type_ = dalp::JointType::basic;
                    break;
                case 1:
                    joint.joint_type_ = dalp::JointType::hair_root;
                    break;
                case 2:
                    joint.joint_type_ = dalp::JointType::skirt_root;
                    break;
                default:
                    joint.joint_type_ = dalp::JointType::basic;
            }

            header = parse_mat4(header, end, joint.offset_mat_);
        }

        return header;
    }

    const uint8_t* parse_animJoint(
        const uint8_t* header, const uint8_t* const end, dalp::AnimJoint& output
    ) {
        {
            output.name_ = reinterpret_cast<const char*>(header);
            header += output.name_.size() + 1;

            glm::mat4 _;
            header = parse_mat4(header, end, _);
        }

        {
            const auto num = dalp::make_int32(header);
            header += 4;

            for (int i = 0; i < num; ++i) {
                float fbuf[4];
                header = dalp::assemble_4_bytes_array<float>(header, fbuf, 4);
                output.add_position(fbuf[0], fbuf[1], fbuf[2], fbuf[3]);
            }
        }

        {
            const auto num = dalp::make_int32(header);
            header += 4;

            for (int i = 0; i < num; ++i) {
                float fbuf[5];
                header = dalp::assemble_4_bytes_array<float>(header, fbuf, 5);
                output.add_rotation(
                    fbuf[0], fbuf[4], fbuf[1], fbuf[2], fbuf[3]
                );
            }
        }

        {
            const auto num = dalp::make_int32(header);
            header += 4;
            for (int i = 0; i < num; ++i) {
                float fbuf[2];
                header = dalp::assemble_4_bytes_array<float>(header, fbuf, 2);
                output.add_scale(fbuf[0], fbuf[1]);
            }
        }

        return header;
    }

    const uint8_t* parse_animations(
        const uint8_t* header,
        const uint8_t* const end,
        std::vector<dalp::Animation>& animations
    ) {
        const auto anim_count = dalp::make_int32(header);
        header += 4;

        animations.resize(anim_count);
        for (int i = 0; i < anim_count; ++i) {
            auto& anim = animations.at(i);

            anim.name_ = reinterpret_cast<const char*>(header);
            header += anim.name_.size() + 1;

            const auto duration_tick = dalp::make_float32(header);
            header += 4;

            anim.ticks_per_sec_ = dalp::make_float32(header);
            header += 4;

            const auto joint_count = dalp::make_int32(header);
            header += 4;

            anim.joints_.resize(joint_count);
            for (int j = 0; j < joint_count; ++j) {
                header = ::parse_animJoint(header, end, anim.joints_.at(j));
            }
        }

        return header;
    }

}  // namespace


// Parse render units
namespace {

    const uint8_t* parse_material(
        const uint8_t* header,
        const uint8_t* const end,
        dalp::Material& material
    ) {
        material.roughness_ = dalp::make_float32(header);
        header += 4;
        material.metallic_ = dalp::make_float32(header);
        header += 4;
        material.transparency_ = dalp::make_bool8(header);
        header += 1;

        material.albedo_map_ = reinterpret_cast<const char*>(header);
        header += material.albedo_map_.size() + 1;

        material.roughness_map_ = reinterpret_cast<const char*>(header);
        header += material.roughness_map_.size() + 1;

        material.metallic_map_ = reinterpret_cast<const char*>(header);
        header += material.metallic_map_.size() + 1;

        material.normal_map_ = reinterpret_cast<const char*>(header);
        header += material.normal_map_.size() + 1;

        return header;
    }

    const uint8_t* parse_mesh(
        const uint8_t* header,
        const uint8_t* const end,
        dalp::Mesh_Straight& mesh
    ) {
        const auto vert_count = dalp::make_int32(header);
        header += 4;

        const auto vert_count_times_3 = vert_count * 3;
        const auto vert_count_times_2 = vert_count * 2;

        mesh.vertices_.resize(vert_count_times_3);
        header = dalp::assemble_4_bytes_array<float>(
            header, mesh.vertices_.data(), vert_count_times_3
        );

        mesh.uv_coordinates_.resize(vert_count_times_2);
        header = dalp::assemble_4_bytes_array<float>(
            header, mesh.uv_coordinates_.data(), vert_count_times_2
        );

        mesh.normals_.resize(vert_count_times_3);
        header = dalp::assemble_4_bytes_array<float>(
            header, mesh.normals_.data(), vert_count_times_3
        );

        return header;
    }

    const uint8_t* parse_mesh(
        const uint8_t* header,
        const uint8_t* const end,
        dalp::Mesh_StraightJoint& mesh
    ) {
        const auto vert_count = dalp::make_int32(header);
        header += 4;
        const auto vert_count_times_3 = vert_count * 3;
        const auto vert_count_times_2 = vert_count * 2;
        const auto vert_count_times_joint_count =
            vert_count * dal::parser::NUM_JOINTS_PER_VERTEX;

        mesh.vertices_.resize(vert_count_times_3);
        header = dalp::assemble_4_bytes_array<float>(
            header, mesh.vertices_.data(), vert_count_times_3
        );

        mesh.uv_coordinates_.resize(vert_count_times_2);
        header = dalp::assemble_4_bytes_array<float>(
            header, mesh.uv_coordinates_.data(), vert_count_times_2
        );

        mesh.normals_.resize(vert_count_times_3);
        header = dalp::assemble_4_bytes_array<float>(
            header, mesh.normals_.data(), vert_count_times_3
        );

        mesh.joint_weights_.resize(vert_count_times_joint_count);
        header = dalp::assemble_4_bytes_array<float>(
            header, mesh.joint_weights_.data(), vert_count_times_joint_count
        );

        mesh.joint_indices_.resize(vert_count_times_joint_count);
        header = dalp::assemble_4_bytes_array<int32_t>(
            header, mesh.joint_indices_.data(), vert_count_times_joint_count
        );

        return header;
    }

    const uint8_t* parse_mesh(
        const uint8_t* header,
        const uint8_t* const end,
        dalp::Mesh_Indexed& mesh
    ) {
        const auto vertex_count = dalp::make_int32(header);
        header += 4;
        for (int32_t i = 0; i < vertex_count; ++i) {
            auto& vert = mesh.vertices_.emplace_back();

            float fbuf[8];
            header = dalp::assemble_4_bytes_array<float>(header, fbuf, 8);

            vert.pos_ = glm::vec3{ fbuf[0], fbuf[1], fbuf[2] };
            vert.normal_ = glm::vec3{ fbuf[3], fbuf[4], fbuf[5] };
            vert.uv_ = glm::vec2{ fbuf[6], fbuf[7] };
        }

        const auto index_count = dalp::make_int32(header);
        header += 4;
        for (int32_t i = 0; i < index_count; ++i) {
            mesh.indices_.push_back(dalp::make_int32(header));
            header += 4;
        }

        return header;
    }

    const uint8_t* parse_mesh(
        const uint8_t* header,
        const uint8_t* const end,
        dalp::Mesh_IndexedJoint& mesh
    ) {
        const auto vertex_count = dalp::make_int32(header);
        header += 4;
        for (int32_t i = 0; i < vertex_count; ++i) {
            auto& vert = mesh.vertices_.emplace_back();

            float fbuf[8];
            header = dalp::assemble_4_bytes_array<float>(header, fbuf, 8);
            float fbuf_joint[dal::parser::NUM_JOINTS_PER_VERTEX];
            header = dalp::assemble_4_bytes_array<float>(
                header, fbuf_joint, dal::parser::NUM_JOINTS_PER_VERTEX
            );
            int32_t ibuf_joint[dal::parser::NUM_JOINTS_PER_VERTEX];
            header = dalp::assemble_4_bytes_array<int32_t>(
                header, ibuf_joint, dal::parser::NUM_JOINTS_PER_VERTEX
            );

            vert.pos_ = glm::vec3{ fbuf[0], fbuf[1], fbuf[2] };
            vert.normal_ = glm::vec3{ fbuf[3], fbuf[4], fbuf[5] };
            vert.uv_ = glm::vec2{ fbuf[6], fbuf[7] };

            static_assert(sizeof(vert.joint_weights_.x) == sizeof(float));
            static_assert(sizeof(vert.joint_indices_.x) == sizeof(int32_t));
            static_assert(
                sizeof(vert.joint_weights_) ==
                sizeof(float) * dal::parser::NUM_JOINTS_PER_VERTEX
            );
            static_assert(
                sizeof(vert.joint_indices_) ==
                sizeof(int32_t) * dal::parser::NUM_JOINTS_PER_VERTEX
            );

            memcpy(
                &vert.joint_weights_, fbuf_joint, sizeof(vert.joint_weights_)
            );
            memcpy(
                &vert.joint_indices_, ibuf_joint, sizeof(vert.joint_indices_)
            );
        }

        const auto index_count = dalp::make_int32(header);
        header += 4;
        for (int32_t i = 0; i < index_count; ++i) {
            mesh.indices_.push_back(dalp::make_int32(header));
            header += 4;
        }

        return header;
    }

    template <typename _Mesh>
    const uint8_t* parse_render_unit(
        const uint8_t* header,
        const uint8_t* const end,
        dalp::RenderUnit<_Mesh>& unit
    ) {
        unit.name_ = reinterpret_cast<const char*>(header);
        header += unit.name_.size() + 1;
        header = ::parse_material(header, end, unit.material_);
        header = ::parse_mesh(header, end, unit.mesh_);

        return header;
    }


    dalp::ModelParseResult parse_all(
        dalp::Model& output,
        const uint8_t* const begin,
        const uint8_t* const end
    ) {
        const uint8_t* header = begin;

        header = ::parse_aabb(header, end, output.aabb_);
        header = ::parse_skeleton(header, end, output.skeleton_);
        header = ::parse_animations(header, end, output.animations_);

        output.units_straight_.resize(dalp::make_int32(header));
        header += 4;
        for (auto& unit : output.units_straight_) {
            header = ::parse_render_unit(header, end, unit);
        }

        output.units_straight_joint_.resize(dalp::make_int32(header));
        header += 4;
        for (auto& unit : output.units_straight_joint_) {
            header = ::parse_render_unit(header, end, unit);
        }

        output.units_indexed_.resize(dalp::make_int32(header));
        header += 4;
        for (auto& unit : output.units_indexed_) {
            header = ::parse_render_unit(header, end, unit);
        }

        output.units_indexed_joint_.resize(dalp::make_int32(header));
        header += 4;
        for (auto& unit : output.units_indexed_joint_) {
            header = ::parse_render_unit(header, end, unit);
        }

        output.signature_hex_ = reinterpret_cast<const char*>(header);
        header += output.signature_hex_.size() + 1;

        if (header != end)
            return dalp::ModelParseResult::corrupted_content;
        else
            return dalp::ModelParseResult::success;
    }

}  // namespace


namespace dal::parser {

    ModelParseResult parse_dmd(
        Model& output,
        const uint8_t* const file_content,
        const size_t content_size
    ) {
        // Check magic numbers
        if (!::is_magic_numbers_correct(file_content))
            return dalp::ModelParseResult::magic_numbers_dont_match;

        // Decompress
        const auto unzipped = ::unzip_dal_model(file_content, content_size);
        if (!unzipped)
            return dalp::ModelParseResult::decompression_failed;

        // Parse
        return ::parse_all(
            output, unzipped->data(), unzipped->data() + unzipped->size()
        );
    }

    ModelParseResult parse_verify_dmd(
        Model& output,
        const uint8_t* const file_content,
        const size_t content_size,
        const crypto::PublicKeySignature::PublicKey& public_key,
        crypto::PublicKeySignature& sign_mgr
    ) {
        // Check magic numbers
        if (!::is_magic_numbers_correct(file_content))
            return dalp::ModelParseResult::magic_numbers_dont_match;

        // Decompress
        const auto unzipped = ::unzip_dal_model(file_content, content_size);
        if (!unzipped)
            return dalp::ModelParseResult::decompression_failed;

        // Parse
        {
            const auto parse_result = ::parse_all(
                output, unzipped->data(), unzipped->data() + unzipped->size()
            );
            if (dalp::ModelParseResult::success != parse_result)
                return parse_result;
        }

        // Varify
        {
            const dal::crypto::PublicKeySignature::Signature signature{
                output.signature_hex_
            };

            const auto varify_result = sign_mgr.verify(
                unzipped->data(),
                unzipped->size() - output.signature_hex_.size() - 1,
                public_key,
                signature
            );

            if (!varify_result)
                return ModelParseResult::invalid_signature;
        }

        return ModelParseResult::success;
    }

    std::optional<Model> parse_dmd(
        const uint8_t* const file_content, const size_t content_size
    ) {
        Model output;

        if (ModelParseResult::success !=
            dalp::parse_dmd(output, file_content, content_size))
            return std::nullopt;
        else
            return output;
    }

}  // namespace dal::parser
