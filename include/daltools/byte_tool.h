#pragma once

#include <vector>
#include <cstdint>
#include <cstring>


namespace dal::parser {

    bool is_big_endian();


    bool make_bool8(const uint8_t* begin);

    int32_t make_int16(const uint8_t* begin);

    int32_t make_int32(const uint8_t* begin);

    float make_float32(const uint8_t* begin);

    template <typename T>
    T assemble_4_bytes(const uint8_t* const begin) {
        static_assert(1 == sizeof(uint8_t));
        static_assert(4 == sizeof(T));

        T res;

        if ( is_big_endian() ) {
            uint8_t buf[4];
            buf[0] = begin[3];
            buf[1] = begin[2];
            buf[2] = begin[1];
            buf[3] = begin[0];
            memcpy(&res, buf, 4);
        }
        else {
            memcpy(&res, begin, 4);
        }

        return res;
    }

    template <typename T>
    const uint8_t* assemble_4_bytes_array(const uint8_t* src, T* const dst, const size_t element_size) {
        static_assert(4 == sizeof(T));
        const auto copy_size = 4 * element_size;

//*/
        if (is_big_endian()) {
            const auto dst_loc = reinterpret_cast<uint8_t*>(dst);

            for (size_t i = 0; i < element_size; ++i) {
                dst_loc[4 * i + 0] = src[4 * i + 3];
                dst_loc[4 * i + 1] = src[4 * i + 2];
                dst_loc[4 * i + 2] = src[4 * i + 1];
                dst_loc[4 * i + 3] = src[4 * i + 0];
            }
        }
        else {
            const auto copy_size = 4 * element_size;
            memcpy(dst, src, copy_size);
        }
/*/
        for ( size_t i = 0; i < size; ++i ) {
            dst[i] = assemble_4_bytes<T>(src);
        }
//*/

        return src + copy_size;
    }


    uint8_t to_bool8(const bool v);

    void to_int16(const int32_t v, uint8_t* const buffer);

    void to_int32(const int32_t v, uint8_t* const buffer);

    void to_float32(const float v, uint8_t* const buffer);


    // Store data in little endian regardless of system endianness
    // But member functions inputs and outputs data with system endiannnss
    class BinaryDataArray {

    private:
        std::vector<uint8_t> m_vector;

    public:
        BinaryDataArray& operator+=(const BinaryDataArray& other);

        const uint8_t* data() const;

        size_t size() const;

        void reserve(const size_t reserve_size);

        void push_back(const uint8_t v);

        template <typename T>
        void append_array(const T* const array, const size_t size) {
            if (is_big_endian()) {

            }
            else {
                const auto begin = reinterpret_cast<const uint8_t*>(array);
                const auto end = begin + (sizeof(T) * size);
                this->m_vector.insert(this->m_vector.end(), begin, end);
            }
        }

        void append_bool8(const bool v);

        void append_int32(const int32_t v);

        void append_int32_array(const int32_t* const arr, const size_t arr_size);

        void append_float32(const float v);

        void append_float32_array(const float* const arr, const size_t arr_size);

        void append_char(const char c);

        void append_null_terminated_str(const char* const str, const size_t str_size);

    };

}
