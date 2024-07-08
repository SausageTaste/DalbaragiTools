#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>


namespace dal {

    using uint8vec_t = std::vector<uint8_t>;


    enum class CompressResult {
        success,
        not_enough_buffer_size,
        insufficient_memory,
        corrupted_data,
        unknown_error,
    };

    struct CompressResultData {
        size_t m_output_size;
        CompressResult m_result;
    };

    CompressResultData compress_zip(
        uint8_t* const dst,
        const size_t dst_size,
        const uint8_t* const src,
        const size_t src_size
    );
    std::optional<uint8vec_t> compress_zip(const uint8vec_t& src);

    CompressResultData decomp_zip(
        uint8_t* const dst,
        const size_t dst_size,
        const uint8_t* const src,
        const size_t src_size
    );
    std::optional<uint8vec_t> decomp_zip(const uint8vec_t& src, size_t hint);


    std::optional<uint8vec_t> compress_bro(const uint8vec_t& src);
    std::optional<uint8vec_t> decomp_bro(const uint8vec_t& src, size_t hint);


    std::optional<std::vector<uint8_t>> compress_with_header(
        const uint8_t* const src, const size_t src_size
    );

    std::optional<std::vector<uint8_t>> decompress_with_header(
        const uint8_t* const src, const size_t src_size
    );


    std::string encode_base64(const uint8_t* const src, const size_t src_size);

    std::optional<std::vector<uint8_t>> decode_base64(
        const char* const base64_data, const size_t data_size
    );

}  // namespace dal
