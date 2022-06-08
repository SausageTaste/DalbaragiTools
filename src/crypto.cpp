#include "daltools/crypto.h"

#include <mutex>
#include <cassert>
#include <stdexcept>

#include "hydrogen.h"

#include "daltools/byte_tool.h"


namespace {

    const std::string KEY_MAGIC_NUMBERS{ "DKEY" };

    std::once_flag g_flag_init_hydrogen;

    void init_hydrogen() {
        std::call_once(g_flag_init_hydrogen, []() {
            if (0 != hydro_init()) {
                throw std::runtime_error{ "Failed to initizate libhydrogen" };
            }
        });
    }

}


// KeyAttrib
namespace dal::crypto {

    std::vector<uint8_t> KeyAttrib::build_binary_v1() const {
        static_assert(std::is_same<std::chrono::system_clock::rep, int64_t>::value);
        dal::parser::BinaryDataArray array;

        array.append_str(this->m_owner_name);
        array.append_str(this->m_email);
        array.append_str(this->m_description);
        array.append_int64(this->m_created_time.time_since_epoch().count());

        return array.release();
    }

    bool KeyAttrib::parse_binary_v1(const uint8_t* const arr, const size_t arr_size) {
        dal::parser::BinaryArrayParser parser{arr, arr_size};

        this->m_owner_name = parser.parse_str();
        this->m_email = parser.parse_str();
        this->m_description = parser.parse_str();

        const auto a = std::chrono::system_clock::duration{parser.parse_int64()};
        this->m_created_time = std::chrono::system_clock::time_point{a};
        return true;
    }

}


// IKey
namespace dal::crypto {

    IKey::IKey(const uint8_t* const buf, const size_t buf_size) {
        this->set(buf, buf_size);
    }

    IKey::IKey(const std::vector<uint8_t>& bin) {
        this->set(bin.data(), bin.size());
    }

    IKey::IKey(const std::string& hex) {
        this->set_from_hex_str(hex.data(), hex.size());
    }

    bool IKey::operator==(const IKey& other) const {
        if (this->size() != other.size())
            return false;

        return hydro_equal(this->data(), other.data(), this->size());
    }

    void IKey::set(const uint8_t* const buf, const size_t buf_size) {
        this->m_key.clear();
        this->m_key.assign(buf, buf + buf_size);
    }

    bool IKey::set_from_hex_str(const char* const buf, const size_t buf_size) {
        std::vector<uint8_t> buffer(buf_size);

        const auto result = hydro_hex2bin(buffer.data(), buffer.size(), buf, buf_size, nullptr, nullptr);
        if (-1 != result) {
            this->set(buffer.data(), result);
            return true;
        }
        else {
            return false;
        }
    }

    std::string IKey::make_hex_str() const {
        std::string output;

        std::vector<char> buffer;
        buffer.resize(this->size() * 2 + 1);

        if (nullptr != hydro_bin2hex(buffer.data(), buffer.size(), this->data(), this->size())) {
            output = buffer.data();
        }

        return output;
    }


    std::vector<uint8_t> build_key_store_output(const std::string& passwd, const IKey& key, const KeyAttrib& attrib) {
        const int32_t HEADER_SIZE = sizeof(char) * ::KEY_MAGIC_NUMBERS.size() + sizeof(int32_t) * 5;
        const auto attrib_bin = attrib.build_binary_v1();
        dal::parser::BinaryDataArray array;

        // Magic numbers
        for (const auto c : ::KEY_MAGIC_NUMBERS)
            array.append_char(c);
        array.append_int32(1);  // Version
        array.append_int32(HEADER_SIZE);
        array.append_int32(attrib_bin.size());
        array.append_int32(HEADER_SIZE + attrib_bin.size());
        array.append_int32(key.size());
        assert(array.size() == HEADER_SIZE);

        array.append_array(attrib_bin.data(), attrib_bin.size());
        array.append_array(key.data(), key.size());

        return array.release();
    }

    bool parse_key_store_output(const std::string& passwd, const std::vector<uint8_t>& data, IKey& key, KeyAttrib& attrib) {
        dal::parser::BinaryArrayParser parser(data);

        std::string magic_numbers;
        for (const auto c : ::KEY_MAGIC_NUMBERS) {
            if (c != parser.parse_char())
                return false;
        }

        const auto version = parser.parse_int32();
        const auto attrib_loc = parser.parse_int32();
        const auto attrib_size = parser.parse_int32();
        const auto key_loc = parser.parse_int32();
        const auto key_size = parser.parse_int32();

        switch (version) {
            case 1:
                attrib.parse_binary_v1(data.data() + attrib_loc, attrib_size);
                break;
            default:
                return false;
        }

        key.set(data.data() + key_loc, key_size);
        return true;
    }

}


// PublicKeySignature::IKey
namespace dal::crypto {

    bool PublicKeySignature::PublicKey::is_valid() const {
        return this->size() == hydro_sign_PUBLICKEYBYTES;
    }

    bool PublicKeySignature::SecretKey::is_valid() const {
        return this->size() == hydro_sign_SECRETKEYBYTES;
    }

    bool PublicKeySignature::Signature::is_valid() const {
        return this->size() == hydro_sign_BYTES;
    }

}


// SignManager
namespace dal::crypto {

    PublicKeySignature::PublicKeySignature(const char* const context)
        : IContextInfo(context)
    {
        ::init_hydrogen();
    }

    std::pair<PublicKeySignature::PublicKey, PublicKeySignature::SecretKey> PublicKeySignature::gen_keys() {
        std::pair<PublicKey, SecretKey> output;

        hydro_sign_keypair key_pair;
        hydro_sign_keygen(&key_pair);

        PublicKey& pk = output.first;
        SecretKey& sk = output.second;

        pk.set(key_pair.pk, hydro_sign_PUBLICKEYBYTES);
        sk.set(key_pair.sk, hydro_sign_SECRETKEYBYTES);

        return output;
    }

    std::optional<PublicKeySignature::Signature> PublicKeySignature::create_signature(
        const void* const msg,
        const size_t msg_size,
        const PublicKeySignature::SecretKey& secret_key
    ) {
        std::optional<Signature> output{ std::nullopt };

        if (!secret_key.is_valid())
            return output;

        uint8_t signature[hydro_sign_BYTES];
        if (0 == hydro_sign_create(signature, msg, msg_size, this->context_char(), secret_key.data())) {
            output = Signature{};
            output->set(signature, hydro_sign_BYTES);
        }

        return output;
    }

    bool PublicKeySignature::verify(
        const void* const msg,
        const size_t msg_size,
        const PublicKeySignature::PublicKey& public_key,
        const PublicKeySignature::Signature& signature
    ) {
        if (!public_key.is_valid())
            return false;
        if (!signature.is_valid())
            return false;

        return 0 == hydro_sign_verify(signature.data(), msg, msg_size, this->context_char(), public_key.data());
    }

}
