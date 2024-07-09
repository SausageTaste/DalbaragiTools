#pragma once

#include <optional>
#include <vector>

#include "daltools/crypto.h"
#include "daltools/struct.h"


namespace dal::parser {

    using binary_buffer_t = std::vector<uint8_t>;


    enum class ModelExportResult {
        success,
        compression_failure,
        unknown_error,
    };

    // `sign_key` can be null if you don't want to sign it
    ModelExportResult build_binary_model(
        binary_buffer_t& output,
        const Model& input,
        const crypto::PublicKeySignature::SecretKey* const sign_key,
        crypto::PublicKeySignature* const sign_mgr
    );

    std::optional<binary_buffer_t> build_binary_model(
        const Model& input,
        const crypto::PublicKeySignature::SecretKey* const sign_key,
        crypto::PublicKeySignature* const sign_mgr
    );

}  // namespace dal::parser
