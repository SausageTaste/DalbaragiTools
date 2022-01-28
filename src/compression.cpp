#include "daltools/compression.h"

#include <zlib.h>


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

}
