#include "daltools/model_parser.h"

#include "daltools/byte_tool.h"
#include "daltools/konst.h"
#include "daltools/compression.h"


namespace dalp = dal::parser;


namespace {

    std::optional<std::vector<uint8_t>> unzip_dal_model(const uint8_t* const buf, const size_t buf_size) {
        const auto expected_unzipped_size = dal::parser::make_int32(buf + dalp::MAGIC_NUMBER_SIZE);
        const auto zipped_data_offset = dalp::MAGIC_NUMBER_SIZE + 4;

        std::vector<uint8_t> unzipped(expected_unzipped_size);
        const auto decomp_result = dal::decompress_zip(unzipped.data(), unzipped.size(), buf + zipped_data_offset, buf_size - zipped_data_offset);
        if (dal::CompressResult::success != decomp_result.m_result) {
            return std::nullopt;
        }
        else {
            return unzipped;
        }
    }

    bool is_magic_numbers_correct(const uint8_t* const buf) {
        for (int i = 0; i < dalp::MAGIC_NUMBER_SIZE; ++i) {
            if (buf[i] != dalp::MAGIC_NUMBERS_DAL_MODEL[i]) {
                return false;
            }
        }

        return true;
    }

}


// Parser functions
namespace {

    const uint8_t* parse_aabb(const uint8_t* header, const uint8_t* const end, dal::parser::AABB3& output) {
        float fbuf[6];
        header = dal::parser::assemble_4_bytes_array<float>(header, fbuf, 6);

        output.m_min = glm::vec3{ fbuf[0], fbuf[1], fbuf[2] };
        output.m_max = glm::vec3{ fbuf[3], fbuf[4], fbuf[5] };

        return header;
    }

    const uint8_t* parse_mat4(const uint8_t* header, const uint8_t* const end, glm::mat4& mat) {
        float fbuf[16];
        header = dalp::assemble_4_bytes_array<float>(header, fbuf, 16);

        for ( size_t row = 0; row < 4; ++row ) {
            for ( size_t col = 0; col < 4; ++col ) {
                mat[col][row] = fbuf[4 * row + col];
            }
        }

        return header;
    }

}


// Parse animations
namespace {

    const uint8_t* parse_skeleton(const uint8_t* header, const uint8_t* const end, dalp::Skeleton& output) {
        const auto joint_count = dal::parser::make_int32(header); header += 4;
        output.m_joints.resize(joint_count);

        for ( int i = 0; i < joint_count; ++i ) {
            auto& joint = output.m_joints.at(i);

            joint.m_name = reinterpret_cast<const char*>(header); header += joint.m_name.size() + 1;
            joint.m_parent_index = dalp::make_int32(header); header += 4;

            const auto joint_type_index = dalp::make_int32(header); header += 4;
            switch ( joint_type_index ) {
            case 0:
                joint.m_joint_type = dalp::JointType::basic;
                break;
            case 1:
                joint.m_joint_type = dalp::JointType::hair_root;
                break;
            case 2:
                joint.m_joint_type = dalp::JointType::skirt_root;
                break;
            default:
                joint.m_joint_type = dalp::JointType::basic;
            }

            header = parse_mat4(header, end, joint.m_offset_mat);
        }

        return header;
    }

    const uint8_t* parse_animJoint(const uint8_t* header, const uint8_t* const end, dalp::AnimJoint& output) {
        {
            output.m_name = reinterpret_cast<const char*>(header); header += output.m_name.size() + 1;
            header = parse_mat4(header, end, output.m_transform);
        }

        {
            const auto num = dalp::make_int32(header); header += 4;
            for ( int i = 0; i < num; ++i ) {
                float fbuf[4];
                header = dalp::assemble_4_bytes_array<float>(header, fbuf, 4);
                output.add_translate(fbuf[0], fbuf[1], fbuf[2], fbuf[3]);
            }
        }

        {
            const auto num = dalp::make_int32(header); header += 4;
            for ( int i = 0; i < num; ++i ) {
                float fbuf[5];
                header = dalp::assemble_4_bytes_array<float>(header, fbuf, 5);
                output.add_rotation(fbuf[0], fbuf[4], fbuf[1], fbuf[2], fbuf[3]);
            }
        }

        {
            const auto num = dalp::make_int32(header); header += 4;
            for ( int i = 0; i < num; ++i ) {
                float fbuf[2];
                header = dalp::assemble_4_bytes_array<float>(header, fbuf, 2);
                output.add_scale(fbuf[0], fbuf[1]);
            }
        }

        return header;
    }

    const uint8_t* parse_animations(const uint8_t* header, const uint8_t* const end, std::vector<dalp::Animation>& animations) {
        const auto anim_count = dalp::make_int32(header); header += 4;
        animations.resize(anim_count);

        for ( int i = 0; i < anim_count; ++i ) {
            auto& anim = animations.at(i);

            anim.m_name = reinterpret_cast<const char*>(header);
            header += anim.m_name.size() + 1;

            anim.m_duration_tick = dalp::make_float32(header); header += 4;
            anim.m_ticks_par_sec = dalp::make_float32(header); header += 4;

            const auto joint_count = dalp::make_int32(header); header += 4;
            anim.m_joints.resize(joint_count);

            for ( int j = 0; j < joint_count; ++j ) {
                header = ::parse_animJoint(header, end, anim.m_joints.at(j));
            }
        }

        return header;
    }

}


// Parse render units
namespace {

    const uint8_t* parse_material(const uint8_t* header, const uint8_t* const end, dalp::Material& material) {
        material.m_roughness = dalp::make_float32(header); header += 4;
        material.m_metallic = dalp::make_float32(header); header += 4;
        material.alpha_blend = dalp::make_bool8(header); header += 1;

        material.m_albedo_map = reinterpret_cast<const char*>(header);
        header += material.m_albedo_map.size() + 1;

        material.m_roughness_map = reinterpret_cast<const char*>(header);
        header += material.m_roughness_map.size() + 1;

        material.m_metallic_map = reinterpret_cast<const char*>(header);
        header += material.m_metallic_map.size() + 1;

        material.m_normal_map = reinterpret_cast<const char*>(header);
        header += material.m_normal_map.size() + 1;

        return header;
    }

    const uint8_t* parse_mesh(const uint8_t* header, const uint8_t* const end, dalp::Mesh_Straight& mesh) {
        const auto vert_count = dalp::make_int32(header); header += 4;
        const auto vert_count_times_3 = vert_count * 3;
        const auto vert_count_times_2 = vert_count * 2;

        mesh.m_vertices.resize(vert_count_times_3);
        header = dalp::assemble_4_bytes_array<float>(header, mesh.m_vertices.data(), vert_count_times_3);

        mesh.m_texcoords.resize(vert_count_times_2);
        header = dalp::assemble_4_bytes_array<float>(header, mesh.m_texcoords.data(), vert_count_times_2);

        mesh.m_normals.resize(vert_count_times_3);
        header = dalp::assemble_4_bytes_array<float>(header, mesh.m_normals.data(), vert_count_times_3);

        return header;
    }

    const uint8_t* parse_mesh(const uint8_t* header, const uint8_t* const end, dalp::Mesh_StraightJoint& mesh) {
        const auto vert_count = dalp::make_int32(header); header += 4;
        const auto vert_count_times_3 = vert_count * 3;
        const auto vert_count_times_2 = vert_count * 2;
        const auto vert_count_times_joint_count = vert_count * dal::parser::NUM_JOINTS_PER_VERTEX;

        mesh.m_vertices.resize(vert_count_times_3);
        header = dalp::assemble_4_bytes_array<float>(header, mesh.m_vertices.data(), vert_count_times_3);

        mesh.m_texcoords.resize(vert_count_times_2);
        header = dalp::assemble_4_bytes_array<float>(header, mesh.m_texcoords.data(), vert_count_times_2);

        mesh.m_normals.resize(vert_count_times_3);
        header = dalp::assemble_4_bytes_array<float>(header, mesh.m_normals.data(), vert_count_times_3);

        mesh.m_boneWeights.resize(vert_count_times_joint_count);
        header = dalp::assemble_4_bytes_array<float>(header, mesh.m_boneWeights.data(), vert_count_times_joint_count);

        mesh.m_boneIndex.resize(vert_count_times_joint_count);
        header = dalp::assemble_4_bytes_array<int32_t>(header, mesh.m_boneIndex.data(), vert_count_times_joint_count);

        return header;
    }

    const uint8_t* parse_mesh(const uint8_t* header, const uint8_t* const end, dalp::Mesh_Indexed& mesh) {
        const auto vertex_count = dalp::make_int32(header); header += 4;
        for (int32_t i = 0; i < vertex_count; ++i) {
            auto& vert = mesh.m_vertices.emplace_back();

            float fbuf[8];
            header = dalp::assemble_4_bytes_array<float>(header, fbuf, 8);

            vert.m_position = glm::vec3{ fbuf[0], fbuf[1], fbuf[2] };
            vert.m_normal = glm::vec3{ fbuf[3], fbuf[4], fbuf[5] };
            vert.m_uv_coords = glm::vec2{ fbuf[6], fbuf[7] };
        }

        const auto index_count = dalp::make_int32(header); header += 4;
        for (int32_t i = 0; i < index_count; ++i) {
            mesh.m_indices.push_back(dalp::make_int32(header)); header += 4;
        }

        return header;
    }

    const uint8_t* parse_mesh(const uint8_t* header, const uint8_t* const end, dalp::Mesh_IndexedJoint& mesh) {
        const auto vertex_count = dalp::make_int32(header); header += 4;
        for (int32_t i = 0; i < vertex_count; ++i) {
            auto& vert = mesh.m_vertices.emplace_back();

            float fbuf[8];
            header = dalp::assemble_4_bytes_array<float>(header, fbuf, 8);
            float fbuf_joint[dal::parser::NUM_JOINTS_PER_VERTEX];
            header = dalp::assemble_4_bytes_array<float>(header, fbuf_joint, dal::parser::NUM_JOINTS_PER_VERTEX);
            int32_t ibuf_joint[dal::parser::NUM_JOINTS_PER_VERTEX];
            header = dalp::assemble_4_bytes_array<int32_t>(header, ibuf_joint, dal::parser::NUM_JOINTS_PER_VERTEX);

            vert.m_position = glm::vec3{ fbuf[0], fbuf[1], fbuf[2] };
            vert.m_normal = glm::vec3{ fbuf[3], fbuf[4], fbuf[5] };
            vert.m_uv_coords = glm::vec2{ fbuf[6], fbuf[7] };

            static_assert(sizeof(vert.m_joint_weights.x) == sizeof(float));
            static_assert(sizeof(vert.m_joint_indices.x) == sizeof(int32_t));
            static_assert(sizeof(vert.m_joint_weights) == sizeof(float) * dal::parser::NUM_JOINTS_PER_VERTEX);
            static_assert(sizeof(vert.m_joint_indices) == sizeof(int32_t) * dal::parser::NUM_JOINTS_PER_VERTEX);

            memcpy(&vert.m_joint_weights, fbuf_joint, sizeof(vert.m_joint_weights));
            memcpy(&vert.m_joint_indices, ibuf_joint, sizeof(vert.m_joint_indices));
        }

        const auto index_count = dalp::make_int32(header); header += 4;
        for (int32_t i = 0; i < index_count; ++i) {
            mesh.m_indices.push_back(dalp::make_int32(header)); header += 4;
        }

        return header;
    }

    template <typename _Mesh>
    const uint8_t* parse_render_unit(const uint8_t* header, const uint8_t* const end, dalp::RenderUnit<_Mesh>& unit) {
        unit.m_name = reinterpret_cast<const char*>(header); header += unit.m_name.size() + 1;
        header = ::parse_material(header, end, unit.m_material);
        header = ::parse_mesh(header, end, unit.m_mesh);

        return header;
    }


    dalp::ModelParseResult parse_all(dalp::Model& output, const uint8_t* const begin, const uint8_t* const end) {
        const uint8_t* header = begin;

        header = ::parse_aabb(header, end, output.m_aabb);
        header = ::parse_skeleton(header, end, output.m_skeleton);
        header = ::parse_animations(header, end, output.m_animations);

        output.m_units_straight.resize(dalp::make_int32(header)); header += 4;
        for (auto& unit : output.m_units_straight) {
            header = ::parse_render_unit(header, end, unit);
        }

        output.m_units_straight_joint.resize(dalp::make_int32(header)); header += 4;
        for (auto& unit : output.m_units_straight_joint) {
            header = ::parse_render_unit(header, end, unit);
        }

        output.m_units_indexed.resize(dalp::make_int32(header)); header += 4;
        for (auto& unit : output.m_units_indexed) {
            header = ::parse_render_unit(header, end, unit);
        }

        output.m_units_indexed_joint.resize(dalp::make_int32(header)); header += 4;
        for (auto& unit : output.m_units_indexed_joint) {
            header = ::parse_render_unit(header, end, unit);
        }

        output.m_signature_hex = reinterpret_cast<const char*>(header);
        header += output.m_signature_hex.size() + 1;

        if (header != end)
            return dalp::ModelParseResult::corrupted_content;
        else
            return dalp::ModelParseResult::success;
    }

}


namespace dal::parser {

    ModelParseResult parse_dmd(Model& output, const uint8_t* const file_content, const size_t content_size) {
        // Check magic numbers
        if (!::is_magic_numbers_correct(file_content))
            return dalp::ModelParseResult::magic_numbers_dont_match;

        // Decompress
        const auto unzipped = ::unzip_dal_model(file_content, content_size);
        if (!unzipped)
            return dalp::ModelParseResult::decompression_failed;

        // Parse
        return ::parse_all(output, unzipped->data(), unzipped->data() + unzipped->size());
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
            const auto parse_result = ::parse_all(output, unzipped->data(), unzipped->data() + unzipped->size());
            if (dalp::ModelParseResult::success != parse_result)
                return parse_result;
        }

        // Varify
        {
            const dal::crypto::PublicKeySignature::Signature signature{ output.m_signature_hex };

            const auto varify_result = sign_mgr.verify(
                unzipped->data(),
                unzipped->size() - output.m_signature_hex.size() - 1,
                public_key,
                signature
            );

            if (!varify_result)
                return ModelParseResult::invalid_signature;
        }

        return ModelParseResult::success;
    }

    std::optional<Model> parse_dmd(const uint8_t* const file_content, const size_t content_size) {
        Model output;

        if (ModelParseResult::success != dalp::parse_dmd(output, file_content, content_size))
            return std::nullopt;
        else
            return output;
    }

}
