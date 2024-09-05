#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>


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
