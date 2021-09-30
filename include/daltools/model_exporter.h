#pragma once

#include <vector>
#include <optional>

#include "daltools/struct.h"


namespace dal::parser {

    using binary_buffer_t = std::vector<uint8_t>;


    enum class ModelExportResult{
        success,
        unknown_error,
    };

    ModelExportResult build_binary_model(binary_buffer_t& output, const Model& input);

    std::optional<binary_buffer_t> build_binary_model(const Model& input);

    std::optional<binary_buffer_t> zip_binary_model(const uint8_t* const data, const size_t data_size);

}
