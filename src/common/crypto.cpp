#include "daltools/common/crypto.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <mutex>
#include <stdexcept>

#include <hydrogen.h>
#include <spdlog/fmt/fmt.h>
#include <sung/general/bytes.hpp>
#include <sung/general/time.hpp>

#include "daltools/common/byte_tool.h"
#include "daltools/common/compression.h"


namespace {

    std::once_flag g_flag_init_hydrogen;

    void init_hydrogen() {
        std::call_once(g_flag_init_hydrogen, []() {
            if (0 != hydro_init()) {
                throw std::runtime_error{ "Failed to initizate libhydrogen" };
            }
        });
    }

    std::string add_line_breaks(
        const std::string input, const size_t line_length
    ) {
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
        string.erase(
            std::remove_if(
                string.begin(),
                string.end(),
                [](char x) { return std::isspace(x); }
            ),
            string.end()
        );
        return string;
    }

}  // namespace


namespace {

    void serialize_key_metadata(
        sung::BytesBuilder& b, const dal::KeyMetadata& metadata
    ) {
        b.add_nt_str(metadata.owner_name_.c_str());
        b.add_nt_str(metadata.email_.c_str());
        b.add_nt_str(metadata.description_.c_str());
        b.add_nt_str(metadata.created_time_.c_str());
    }

    void parse_key_metadata(
        sung::BytesReader& reader, dal::KeyMetadata& metadata
    ) {
        metadata.owner_name_ = reader.read_nt_str();
        metadata.email_ = reader.read_nt_str();
        metadata.description_ = reader.read_nt_str();
        metadata.created_time_ = reader.read_nt_str();
    }

    std::string serialized_into_str(const uint8_t* data, const size_t size) {
        const auto compressed = dal::compress_bro(data, size);
        if (!compressed)
            return {};

        sung::BytesBuilder com_data;
        com_data.add_uint64(size);
        com_data.add_uint64(compressed->size());
        com_data.add_arr(compressed->data(), compressed->size());
        const auto b64 = dal::encode_base64(com_data.data(), com_data.size());
        return ::add_line_breaks(b64, 40);
    }

    std::vector<uint8_t> deserialize_from_str(const std::string& b64) {
        const auto c_b64 = clean_up_base64_str(b64);
        const auto n_b64 = dal::decode_base64(c_b64.data(), c_b64.size());
        if (!n_b64)
            return {};

        sung::BytesReader reader{ n_b64->data(), n_b64->size() };
        const auto raw_size = reader.read_uint64();
        if (!raw_size)
            return {};
        const auto com_size = reader.read_uint64();
        if (!com_size)
            return {};

        fmt::print(
            "Raw size: {}, Compressed size: {}\n",
            raw_size.value(),
            com_size.value()
        );

        const auto decomp = dal::decomp_bro(
            reader.head(), com_size.value(), raw_size.value()
        );
        if (!decomp)
            return {};

        return decomp.value();
    }

}  // namespace
namespace dal {

    std::pair<DataKeyPublic, DataKeySecret> gen_data_key_pair() {
        ::init_hydrogen();

        std::pair<DataKeyPublic, DataKeySecret> out;
        auto& pub = out.first;
        auto& sec = out.second;

        sec.encrypt_key_.resize(hydro_secretbox_KEYBYTES);
        hydro_secretbox_keygen(sec.encrypt_key_.data());

        hydro_sign_keypair key_pair;
        hydro_sign_keygen(&key_pair);
        pub.sign_key_.set(key_pair.pk, hydro_sign_PUBLICKEYBYTES);
        sec.sign_key_.set(key_pair.sk, hydro_sign_SECRETKEYBYTES);

        return out;
    }

    std::string serialize_key(const DataKeyPublic& key, const KeyMetadata& md) {
        sung::BytesBuilder raw_data;
        ::serialize_key_metadata(raw_data, md);
        raw_data.add_int32((int)KeyType::data_pub);
        raw_data.add_arr(key.sign_key_.data(), key.sign_key_.size());

        return ::serialized_into_str(raw_data.data(), raw_data.size());
    }

    std::string serialize_key(const DataKeySecret& key, const KeyMetadata& md) {
        sung::BytesBuilder raw_data;
        ::serialize_key_metadata(raw_data, md);
        raw_data.add_int32((int)KeyType::data_sec);
        raw_data.add_arr(key.sign_key_.data(), key.sign_key_.size());
        raw_data.add_arr(key.encrypt_key_.data(), key.encrypt_key_.size());

        return ::serialized_into_str(raw_data.data(), raw_data.size());
    }

    std::optional<VKeysMetadata> deserialize_key(const std::string& b64) {
        VKeysMetadata out;

        const auto deserialized = ::deserialize_from_str(b64);
        if (deserialized.empty())
            return sung::nullopt;

        sung::BytesReader reader{ deserialized.data(), deserialized.size() };
        ::parse_key_metadata(reader, out.second);
        const auto key_type_int = reader.read_int32();
        if (!key_type_int)
            return sung::nullopt;
        const auto key_type = static_cast<KeyType>(key_type_int.value());

        if (key_type == KeyType::data_pub) {
            out.first = DataKeyPublic{};
            auto& key = std::get<DataKeyPublic>(out.first);

            key.sign_key_.resize(hydro_sign_PUBLICKEYBYTES);
            reader.read_raw_arr(key.sign_key_.data(), key.sign_key_.size());
        } else if (key_type == KeyType::data_sec) {
            out.first = DataKeySecret{};
            auto& key = std::get<DataKeySecret>(out.first);

            key.sign_key_.resize(hydro_sign_SECRETKEYBYTES);
            reader.read_raw_arr(key.sign_key_.data(), key.sign_key_.size());

            key.encrypt_key_.resize(hydro_secretbox_KEYBYTES);
            reader.read_raw_arr(
                key.encrypt_key_.data(), key.encrypt_key_.size()
            );
        }

        if (!reader.is_eof())
            return sung::nullopt;

        return out;
    }


    std::optional<std::vector<uint8_t>> encrypt_data(
        const DataKeySecret& key, const void* const data, const size_t data_size
    ) {
        if (key.encrypt_key_.size() != hydro_secretbox_KEYBYTES)
            return sung::nullopt;

        std::vector<uint8_t> out(hydro_secretbox_HEADERBYTES + data_size);
        ::init_hydrogen();
        const auto res = hydro_secretbox_encrypt(
            out.data(),
            reinterpret_cast<const uint8_t*>(data),
            data_size,
            0,
            CRYPTO_CONTEXT,
            key.encrypt_key_.data()
        );
        if (0 != res)
            return sung::nullopt;

        return out;
    }

    std::optional<std::vector<uint8_t>> decrypt_data(
        const DataKeySecret& key, const void* const data, const size_t data_size
    ) {
        if (key.encrypt_key_.size() != hydro_secretbox_KEYBYTES)
            return sung::nullopt;

        std::vector<uint8_t> out(data_size - hydro_secretbox_HEADERBYTES);
        ::init_hydrogen();
        const auto res = hydro_secretbox_decrypt(
            out.data(),
            reinterpret_cast<const uint8_t*>(data),
            data_size,
            0,
            CRYPTO_CONTEXT,
            key.encrypt_key_.data()
        );
        if (0 != res)
            return sung::nullopt;

        return out;
    }

    std::optional<std::vector<uint8_t>> create_signature(
        const DataKeySecret& key, const void* const data, const size_t data_size
    ) {
        if (key.sign_key_.size() != hydro_sign_SECRETKEYBYTES)
            return sung::nullopt;

        std::vector<uint8_t> out(hydro_sign_BYTES);
        ::init_hydrogen();
        const auto res = hydro_sign_create(
            out.data(),
            reinterpret_cast<const uint8_t*>(data),
            data_size,
            CRYPTO_CONTEXT,
            key.sign_key_.data()
        );

        if (0 != res)
            return sung::nullopt;

        return out;
    }

    bool verify_signature(
        const DataKeyPublic& key,
        const void* const data,
        const size_t data_size,
        const void* const sig,
        const size_t sig_size
    ) {
        if (key.sign_key_.size() != hydro_sign_PUBLICKEYBYTES)
            return false;
        if (sig_size != hydro_sign_BYTES)
            return false;

        ::init_hydrogen();
        const auto res = hydro_sign_verify(
            reinterpret_cast<const uint8_t*>(sig),
            reinterpret_cast<const uint8_t*>(data),
            data_size,
            CRYPTO_CONTEXT,
            key.sign_key_.data()
        );

        return 0 == res;
    }

}  // namespace dal


// KeyMetadata
namespace dal {

    void KeyMetadata::update_created_time() {
        created_time_ = sung::get_time_iso_utc();
    }

}  // namespace dal
