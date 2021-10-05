#pragma once

#include <string>
#include <vector>
#include <utility>
#include <optional>


namespace dal::crypto {

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


    class PublicKeySignature : private IContextInfo {

    public:
        class PublicKey : public IKey {
            using IKey::IKey;
        };


        class SecretKey : public IKey {
            using IKey::IKey;
        };


        class Signature : public IKey {
            using IKey::IKey;
        };


    public:
        PublicKeySignature(const char* const context);

        std::pair<PublicKey, SecretKey> gen_keys();

        std::optional<Signature> create_signature(const void* const msg, const size_t msg_size, const SecretKey& secret_key);

        bool verify(const void* const msg, const size_t msg_size, const PublicKey& public_key, const Signature& signature);

    };

}
