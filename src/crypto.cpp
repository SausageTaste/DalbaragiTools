#include "daltools/crypto.h"

#include <mutex>
#include <fstream>
#include <cassert>
#include <stdexcept>

#include "hydrogen.h"
#include <fmt/format.h>

#include "daltools/byte_tool.h"
#include "daltools/compression.h"


namespace {

    const char* const KEY_MAGIC_NUMBERS = "DKEY";

    std::once_flag g_flag_init_hydrogen;

    void init_hydrogen() {
        std::call_once(g_flag_init_hydrogen, []() {
            if (0 != hydro_init()) {
                throw std::runtime_error{ "Failed to initizate libhydrogen" };
            }
        });
    }

    template <typename T>
    std::optional<T> read_file(const char* const path) {
        using namespace std::string_literals;

        std::ifstream file{ path, std::ios::ate | std::ios::binary | std::ios::in };

        if (!file.is_open())
            return std::nullopt;

        const auto file_size = static_cast<size_t>(file.tellg());
        T buffer;
        buffer.resize(file_size);

        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        file.close();

        return buffer;
    }

    std::string add_line_breaks(const std::string input, const size_t line_length) {
        std::string output;
        auto header = input.data();
        auto end = header + input.size();

        while (header < end) {
            const auto distance = end - header;
            const auto line_size = std::min<long long>(line_length, distance);
            output.append(header, line_size);
            output.push_back('\n');
            header += line_size;
        }

        return output;
    }

    std::string clean_up_base64_str(std::string string) {
        string.erase(std::remove_if(
            string.begin(),
            string.end(),
            [](char x) { return std::isspace(x); }
        ), string.end());
        return string;
    }

    template <typename T>
    dal::crypto::KeyType convert_to_key_type(const T value) {
        if (value >= static_cast<T>(dal::crypto::KeyType::unknown))
            return dal::crypto::KeyType::unknown;
        else if (value < 0)
            return dal::crypto::KeyType::unknown;
        else
            return static_cast<dal::crypto::KeyType>(value);
    }

}


// KeyAttrib
namespace dal::crypto {

     bool KeyAttrib::operator!=(const KeyAttrib& rhs) const {
        if (this->m_created_time != rhs.m_created_time)
            return true;
        if (this->m_owner_name != rhs.m_owner_name)
            return true;
        if (this->m_email != rhs.m_email)
            return true;
        if (this->m_description != rhs.m_description)
            return true;
        if (this->m_type != rhs.m_type)
            return true;

        return false;
     }

    std::vector<uint8_t> KeyAttrib::build_binary_v1() const {
        /*
        I have no idea why this contraint is needed.

        static_assert(
            std::is_same<std::chrono::system_clock::rep, int64_t>::value ||
            std::is_same<std::chrono::system_clock::rep, int32_t>::value ||
            std::is_same<std::chrono::system_clock::rep, int16_t>::value
        );
        */

        dal::parser::BinaryDataArray array;

        array.append_str(this->m_owner_name);
        array.append_str(this->m_email);
        array.append_str(this->m_description);
        array.append_int32(static_cast<int32_t>(this->m_type));
        array.append_int64(this->m_created_time.time_since_epoch().count());

        return array.release();
    }

    bool KeyAttrib::parse_binary_v1(const uint8_t* const arr, const size_t arr_size) {
        dal::parser::BinaryArrayParser parser{arr, arr_size};

        this->m_owner_name = parser.parse_str();
        this->m_email = parser.parse_str();
        this->m_description = parser.parse_str();
        this->m_type = ::convert_to_key_type(parser.parse_int32());

        const auto a = std::chrono::system_clock::duration{parser.parse_int64()};
        this->m_created_time = std::chrono::system_clock::time_point{a};
        return parser.is_emtpy();
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

    bool IKey::operator!=(const IKey& other) const {
        return !IKey::operator==(other);
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

}


namespace dal::crypto {

    const char* get_key_type_str(const KeyType e) {
        switch (e) {
            case KeyType::sign_public:
                return "sign_public";
            case KeyType::sign_private:
                return "sign_private";
            case KeyType::signature:
                return "signature";
            default:
                return "unknown";
        }
    }


    std::vector<uint8_t> build_key_binary(const IKey& key, const KeyAttrib& attrib) {
        const auto magic_num_len = std::strlen(::KEY_MAGIC_NUMBERS);
        const int32_t HEADER_SIZE = sizeof(char) * magic_num_len + sizeof(int32_t) * 5;
        const auto attrib_bin = attrib.build_binary_v1();
        dal::parser::BinaryDataArray array;

        // Magic numbers
        for (int i = 0; i < magic_num_len; ++i) {
            array.append_char(::KEY_MAGIC_NUMBERS[i]);
        }

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

    std::string build_key_store(const IKey& key, const KeyAttrib& attrib) {
        const auto data = build_key_binary(key, attrib);
        const auto compressed = dal::compress_with_header(data.data(), data.size());  // If it fails, it's a bug
        const auto base64 = dal::encode_base64(compressed->data(), compressed->size());
        return ::add_line_breaks(base64, 40);
    }

    void save_key(const char* const path, const IKey& key, const KeyAttrib& attrib) {
        const auto data = build_key_store(key, attrib);
        std::ofstream file(path);
        file.write(data.data(), data.size());
        file.close();
    }


    bool parse_key_binary(const std::string& passwd, const std::vector<uint8_t>& data, IKey& key, KeyAttrib& attrib) {
        dal::parser::BinaryArrayParser parser(data);

        const auto magic_num_len = std::strlen(::KEY_MAGIC_NUMBERS);
        for (int i = 0; i < magic_num_len; ++i) {
            if (::KEY_MAGIC_NUMBERS[i] != parser.parse_char())
                return false;
        }

        const auto version = parser.parse_int32();
        const auto attrib_loc = parser.parse_int32();
        const auto attrib_size = parser.parse_int32();
        const auto key_loc = parser.parse_int32();
        const auto key_size = parser.parse_int32();

        bool parse_result = false;
        switch (version) {
            case 1:
                parse_result = attrib.parse_binary_v1(data.data() + attrib_loc, attrib_size);
                break;
            default:
                return false;
        }
        if (!parse_result)
            return false;

        key.set(data.data() + key_loc, key_size);
        return true;
    }

    void parse_key_store(const std::string& data, const char* const key_path, IKey& out_key, KeyAttrib& out_attrib) {
        const auto base64 = ::clean_up_base64_str(data);
        const auto compressed = dal::decode_base64(base64.data(), base64.size());
        if (!compressed)
            throw std::runtime_error{fmt::format("Failed to decode a key file: \"{}\"\n", key_path)};

        const auto key_data = dal::decompress_with_header(compressed->data(), compressed->size());
        if (!key_data)
            throw std::runtime_error{fmt::format("Failed to uncompress a key file: \"{}\"\n", key_path)};

        if (!parse_key_binary("", *key_data, out_key, out_attrib))
            throw std::runtime_error{fmt::format("Failed to parse a key file: \"{}\"\n", key_path)};
    }

    void load_key(const char* const key_path, IKey& out_key, KeyAttrib& out_attrib) {
        const auto base64 = ::read_file<std::string>(key_path);
        if (!base64)
            throw std::runtime_error{fmt::format("Failed to open a key file: \"{}\"\n", key_path)};

        parse_key_store(base64.value(), key_path, out_key, out_attrib);
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
