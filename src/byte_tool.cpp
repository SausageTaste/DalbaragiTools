#include "daltools/byte_tool.h"


namespace dal::parser {

    bool is_big_endian() {
        constexpr short int number = 0x1;
        const char* const numPtr = reinterpret_cast<const char*>(&number);
        return numPtr[0] != 1;
    }

    bool make_bool8(const uint8_t* begin) {
        return (*begin) != static_cast<uint8_t>(0);
    }

    int32_t make_int16(const uint8_t* begin) {
        static_assert(1 == sizeof(uint8_t), "Size of uint8 is not 1 byte. WTF???");
        static_assert(4 == sizeof(float), "Size of float is not 4 bytes.");

        uint8_t buf[4];

        if ( is_big_endian() ) {
            buf[0] = 0;
            buf[1] = 0;
            buf[2] = begin[1];
            buf[3] = begin[0];
        }
        else {
            buf[0] = begin[0];
            buf[1] = begin[1];
            buf[2] = 0;
            buf[3] = 0;
        }

        int32_t res;
        memcpy(&res, buf, 4);
        return res;
    }

    int32_t make_int32(const uint8_t* begin) {
        return assemble_4_bytes<int32_t>(begin);
    }

    float make_float32(const uint8_t* begin) {
        return assemble_4_bytes<float>(begin);
    }

}


namespace dal::parser {

    uint8_t to_bool8(const bool v) {
        return v ? 1 : 0;
    }

    void to_int16(const int32_t v, uint8_t* const buffer) {
        const auto src_loc = reinterpret_cast<const uint8_t*>(&v);

        if (is_big_endian()) {
            buffer[0] = src_loc[1];
            buffer[1] = src_loc[0];
        }
        else {
            std::memcpy(buffer, src_loc, 2);
        }
    }

    void to_int32(const int32_t v, uint8_t* const buffer) {
        const auto src_loc = reinterpret_cast<const uint8_t*>(&v);

        if (is_big_endian()) {
            buffer[0] = src_loc[3];
            buffer[1] = src_loc[2];
            buffer[2] = src_loc[1];
            buffer[3] = src_loc[0];
        }
        else {
            std::memcpy(buffer, src_loc, 4);
        }
    }

    void to_float32(const float v, uint8_t* const buffer) {
        const auto src_loc = reinterpret_cast<const uint8_t*>(&v);

        if (is_big_endian()) {
            buffer[0] = src_loc[3];
            buffer[1] = src_loc[2];
            buffer[2] = src_loc[1];
            buffer[3] = src_loc[0];
        }
        else {
            std::memcpy(buffer, src_loc, 4);
        }
    }

}
