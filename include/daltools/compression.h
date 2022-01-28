#include <vector>


namespace dal {

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

    CompressResultData compress_zip(uint8_t* const dst, const size_t dst_size, const uint8_t* const src, const size_t src_size);

    CompressResultData decompress_zip(uint8_t* const dst, const size_t dst_size, const uint8_t* const src, const size_t src_size);

}
