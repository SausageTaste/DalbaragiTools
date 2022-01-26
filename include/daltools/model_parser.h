#pragma once

#include <optional>

#include "daltools/struct.h"
#include "daltools/crypto.h"


namespace dal::parser {

    enum class ModelParseResult{
        success,
        magic_numbers_dont_match,
        decompression_failed,
        corrupted_content,
        invalid_signature,
    };

    ModelParseResult parse_dmd(Model& output, const uint8_t* const file_content, const size_t content_size);

    std::optional<Model> parse_dmd(const uint8_t* const file_content, const size_t content_size);

    ModelParseResult parse_verify_dmd(
        Model& output,
        const uint8_t* const file_content,
        const size_t content_size,
        const crypto::PublicKeySignature::PublicKey& public_key,
        crypto::PublicKeySignature& m_sign_mgr
    );

}
