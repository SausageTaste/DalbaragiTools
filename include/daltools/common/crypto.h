#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>


namespace dal::crypto {

    enum class KeyType {
        sign_public,
        sign_private,
        signature,
        unknown,
    };

    const char* get_key_type_str(const KeyType e);


    class KeyAttrib {

    public:
        // Use system clock since it's epoch is (de facto) defined to be
        // consistent as Unix time
        std::chrono::system_clock::time_point m_created_time =
            std::chrono::system_clock::now();
        std::string m_owner_name;
        std::string m_email;
        std::string m_description;
        KeyType m_type = KeyType::unknown;

    public:
        bool operator!=(const KeyAttrib& rhs) const;

        std::vector<uint8_t> build_binary_v1() const;

        bool parse_binary_v1(const uint8_t* const arr, const size_t arr_size);

        const char* get_type_str() const {
            return get_key_type_str(this->m_type);
        }
    };


    class IContextInfo {

    private:
        std::string m_data;

    public:
        IContextInfo(const char* const context) : m_data(context) {}

        auto context_char() const { return this->m_data.c_str(); }

        auto& context_str() const { return this->m_data; }
    };


    class IKey {

    private:
        std::vector<uint8_t> m_key;

    public:
        virtual ~IKey() = default;

        virtual KeyType key_type() const { return KeyType::unknown; }

        IKey() = default;

        IKey(const uint8_t* const buf, const size_t buf_size);

        explicit IKey(const std::vector<uint8_t>& bin);

        explicit IKey(const std::string& hex);

        bool operator==(const IKey& other) const;

        bool operator!=(const IKey& other) const;

        auto data() const { return this->m_key.data(); }

        auto size() const { return this->m_key.size(); }

        void set(const uint8_t* const buf, const size_t buf_size);

        bool set_from_hex_str(const char* const buf, const size_t buf_size);

        std::string make_hex_str() const;
    };


    std::vector<uint8_t> build_key_binary(
        const IKey& key, const KeyAttrib& attrib
    );

    std::string build_key_store(const IKey& key, const KeyAttrib& attrib);

    void save_key(
        const char* const path, const IKey& key, const KeyAttrib& attrib
    );


    bool parse_key_binary(
        const std::vector<uint8_t>& data, IKey& out_key, KeyAttrib& out_attrib
    );

    void parse_key_store(
        const std::string& data,
        const char* const key_path,
        IKey& out_key,
        KeyAttrib& out_attrib
    );

    void load_key(
        const char* const key_path, IKey& out_key, KeyAttrib& out_attrib
    );

    template <typename _KeyType>
    std::pair<_KeyType, KeyAttrib> parse_key_store(
        const std::string& data, const char* const key_path
    ) {
        std::pair<_KeyType, KeyAttrib> output;
        parse_key_store(data, key_path, output.first, output.second);
        return output;
    }

    template <typename _KeyType>
    std::pair<_KeyType, KeyAttrib> load_key(const char* const key_path) {
        std::pair<_KeyType, KeyAttrib> output;
        load_key(key_path, output.first, output.second);
        return output;
    }


    class PublicKeySignature : private IContextInfo {

    public:
        class PublicKey : public IKey {

        public:
            using IKey::IKey;

            bool is_valid() const;

            KeyType key_type() const override {
                if (this->is_valid())
                    return KeyType::sign_public;
                else
                    return KeyType::unknown;
            }
        };


        class SecretKey : public IKey {

        public:
            using IKey::IKey;

            bool is_valid() const;

            KeyType key_type() const override {
                if (this->is_valid())
                    return KeyType::sign_private;
                else
                    return KeyType::unknown;
            }
        };


        class Signature : public IKey {

        public:
            using IKey::IKey;

            bool is_valid() const;

            KeyType key_type() const override {
                if (this->is_valid())
                    return KeyType::signature;
                else
                    return KeyType::unknown;
            }
        };


    public:
        PublicKeySignature(const char* const context);

        std::pair<PublicKey, SecretKey> gen_keys();

        std::optional<Signature> create_signature(
            const void* const msg,
            const size_t msg_size,
            const SecretKey& secret_key
        );

        bool verify(
            const void* const msg,
            const size_t msg_size,
            const PublicKey& public_key,
            const Signature& signature
        );
    };

}  // namespace dal::crypto


namespace dal {

    const char* const CRYPTO_CONTEXT = "daltools";


    class KeyMetadata {

    public:
        void update_created_time();

        std::string owner_name_;
        std::string email_;
        std::string description_;
        std::string created_time_;
    };


    class KeyBytes {

    public:
        bool operator==(const KeyBytes& other) const {
            return data_ == other.data_;
        }

        auto data() { return data_.data(); }
        auto data() const { return data_.data(); }
        auto size() const { return data_.size(); }
        void resize(const size_t size) { data_.resize(size); }

        void set(const uint8_t* const buf, const size_t buf_size) {
            this->resize(buf_size);
            std::copy(buf, buf + buf_size, data_.data());
        }

    private:
        std::vector<uint8_t> data_;
    };


    enum class KeyType {
        data_pub,
        data_sec,
    };


    struct DataKeyPublic {
        KeyBytes sign_key_;
    };


    struct DataKeySecret {
        KeyBytes sign_key_;
        KeyBytes encrypt_key_;
    };


    std::pair<DataKeyPublic, DataKeySecret> gen_data_key_pair();

    std::string serialize_key(const DataKeyPublic& key, const KeyMetadata& md);
    std::string serialize_key(const DataKeySecret& key, const KeyMetadata& md);

    using VKeys = std::variant<DataKeyPublic, DataKeySecret>;
    using VKeysMetadata = std::pair<VKeys, KeyMetadata>;

    std::optional<VKeysMetadata> deserialize_key(const std::string& b64);

}  // namespace dal
