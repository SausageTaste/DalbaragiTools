#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <utility>
#include <optional>


namespace dal::crypto {

    class KeyAttrib {

    public:
        // Use system clock since it's epoch is (de facto) defined to be consistent as Unix time
        std::chrono::system_clock::time_point m_created_time = std::chrono::system_clock::now();
        std::string m_owner_name;
        std::string m_email;
        std::string m_description;

    public:
        std::vector<uint8_t> build_binary_v1() const;

        bool parse_binary_v1(const uint8_t* const arr, const size_t arr_size);

    };


    class IContextInfo {

    private:
        std::string m_data;

    public:
        IContextInfo(const char* const context)
            : m_data(context)
        {

        }

        auto context_char() const {
            return this->m_data.c_str();
        }

        auto& context_str() const {
            return this->m_data;
        }

    };


    class IKey {

    private:
        std::vector<uint8_t> m_key;

    public:
        IKey() = default;

        IKey(const uint8_t* const buf, const size_t buf_size);

        explicit
        IKey(const std::vector<uint8_t>& bin);

        explicit
        IKey(const std::string& hex);

        bool operator==(const IKey& other) const;

        auto data() const {
            return this->m_key.data();
        }

        auto size() const {
            return this->m_key.size();
        }

        void set(const uint8_t* const buf, const size_t buf_size);

        bool set_from_hex_str(const char* const buf, const size_t buf_size);

        std::string make_hex_str() const;

    };


    std::vector<uint8_t> build_key_store_output(const std::string& passwd, const IKey& key, const KeyAttrib& attrib);

    bool parse_key_store_output(const std::string& passwd, const std::vector<uint8_t>& data, IKey& key, KeyAttrib& attrib);


    class PublicKeySignature : private IContextInfo {

    public:
        class PublicKey : public IKey {

        public:
            using IKey::IKey;

            bool is_valid() const;

        };


        class SecretKey : public IKey {

        public:
            using IKey::IKey;

            bool is_valid() const;

        };


        class Signature : public IKey {

        public:
            using IKey::IKey;

            bool is_valid() const;

        };


    public:
        PublicKeySignature(const char* const context);

        std::pair<PublicKey, SecretKey> gen_keys();

        std::optional<Signature> create_signature(const void* const msg, const size_t msg_size, const SecretKey& secret_key);

        bool verify(const void* const msg, const size_t msg_size, const PublicKey& public_key, const Signature& signature);

    };

}
