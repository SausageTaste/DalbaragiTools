#pragma once

#include <optional>

#include "daltools/crypto.h"
#include "daltools/struct.h"


namespace dal::parser {

    enum class ModelParseResult {
        success,
        magic_numbers_dont_match,
        decompression_failed,
        corrupted_content,
        invalid_signature,
    };

    ModelParseResult parse_dmd(
        Model& output,
        const uint8_t* const file_content,
        const size_t content_size
    );

    std::optional<Model> parse_dmd(
        const uint8_t* const file_content, const size_t content_size
    );

    ModelParseResult parse_verify_dmd(
        Model& output,
        const uint8_t* const file_content,
        const size_t content_size,
        const crypto::PublicKeySignature::PublicKey& public_key,
        crypto::PublicKeySignature& m_sign_mgr
    );


    enum class JsonParseResult {
        success,
    };

    JsonParseResult parse_json(
        std::vector<SceneIntermediate>& scenes,
        const uint8_t* const file_content,
        const size_t content_size
    );

    JsonParseResult parse_json_bin(
        std::vector<SceneIntermediate>& scenes,
        const uint8_t* const json_data,
        const size_t json_size,
        const uint8_t* const bin_data,
        const size_t bin_size
    );

}  // namespace dal::parser
