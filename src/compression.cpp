#include "daltools/compression.h"

#include <zlib.h>
#include <libbase64.h>

#include "daltools/byte_tool.h"


namespace dal {

    CompressResultData compress_zip(uint8_t* const dst, const size_t dst_size, const uint8_t* const src, const size_t src_size) {
        CompressResultData output;
        uLongf dst_len = dst_size;

        if (Z_OK == compress(dst, &dst_len, src, src_size)) {
            output.m_result = CompressResult::success;
            output.m_output_size = dst_len;
        }
        else {
            output.m_result = CompressResult::unknown_error;
        }

        return output;
    }

    CompressResultData decompress_zip(uint8_t* const dst, const size_t dst_size, const uint8_t* const src, const size_t src_size) {
        static_assert(sizeof(Bytef) == sizeof(uint8_t));

        CompressResultData output;
        uLongf decom_buffer_size = dst_size;

        const auto res = uncompress(dst, &decom_buffer_size, src, src_size);
        switch (res) {
            case Z_OK:
                output.m_result = CompressResult::success;
                output.m_output_size = decom_buffer_size;
                break;
            case Z_BUF_ERROR:
                output.m_result = CompressResult::not_enough_buffer_size;
                break;
            case Z_MEM_ERROR:
                output.m_result = CompressResult::insufficient_memory;
                break;
            case Z_DATA_ERROR:
                output.m_result = CompressResult::corrupted_data;
                break;
            default:
                output.m_result = CompressResult::unknown_error;
                break;
        }

        return output;
    }


    std::optional<std::vector<uint8_t>> compress_with_header(const uint8_t* const src, const size_t src_size) {
        std::vector<uint8_t> buffer(src_size * 2);
        const auto result = compress_zip(buffer.data(), buffer.size(), src, src_size);

        if (result.m_result != CompressResult::success) {
            return std::nullopt;
        }
        else {
            dal::parser::BinaryDataArray output;
            output.append_int64(src_size);
            output.append_array(buffer.data(), result.m_output_size);
            return output.release();
        }
    }

    std::optional<std::vector<uint8_t>> decompress_with_header(const uint8_t* const src, const size_t src_size) {
        constexpr size_t HEADER_SIZE = sizeof(int64_t);

        dal::parser::BinaryArrayParser parser(src, src_size);
        const auto raw_data_size = parser.parse_int64();
        std::vector<uint8_t> buffer(raw_data_size);

        const auto result = decompress_zip(buffer.data(), buffer.size(), src + HEADER_SIZE, src_size - HEADER_SIZE);
        if (result.m_result != CompressResult::success) {
            return std::nullopt;
        }
        else if (result.m_output_size != raw_data_size) {
            return std::nullopt;
        }
        else {
            return buffer;
        }
    }


    std::string encode_base64(const uint8_t* const src, const size_t src_size) {
        std::string output;
        output.resize(src_size * 2);
        size_t output_size = output.size();

        base64_encode(
            reinterpret_cast<const char*>(src),
            src_size,
            reinterpret_cast<char*>(output.data()),
            &output_size,
            0
        );

        output.resize(output_size);
        return output;
    }

    std::optional<std::vector<uint8_t>> decode_base64(const char* const base64_data, const size_t data_size) {
        std::vector<uint8_t> output(data_size);
        size_t output_size = output.size();

        const auto result = base64_decode(
            base64_data,
            data_size,
            reinterpret_cast<char*>(output.data()),
            &output_size,
            0
        );

        if (1 != result)
            return std::nullopt;

        output.resize(output_size);
        return output;
    }

}
