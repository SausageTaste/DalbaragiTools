#pragma once

#include <optional>

#include "daltools/struct.h"


namespace dal::parser {

    enum class ModelParseResult{
        success,
        magic_numbers_dont_match,
        decompression_failed,
        corrupted_content,
    };

    ModelParseResult unzip_dmd(std::vector<uint8_t>& output, const uint8_t* const file_content, const size_t content_size);

    std::optional<std::vector<uint8_t>> unzip_dmd(const uint8_t* const file_content, const size_t content_size);

    ModelParseResult parse_dmd(Model& output, const uint8_t* const unzipped_content, const size_t content_size);

    std::optional<Model> parse_dmd(const uint8_t* const unzipped_content, const size_t content_size);

}
