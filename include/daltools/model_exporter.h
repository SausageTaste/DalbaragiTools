#pragma once

#include <vector>
#include <optional>

#include "daltools/struct.h"
#include "daltools/crypto.h"


namespace dal::parser {

    using binary_buffer_t = std::vector<uint8_t>;


    enum class ModelExportResult{
        success,
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

    std::optional<binary_buffer_t> zip_binary_model(const uint8_t* const data, const size_t data_size);

}
