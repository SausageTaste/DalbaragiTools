#include "daltools/crypto.h"

#include <mutex>
#include <stdexcept>

#include "hydrogen.h"


namespace {

    std::once_flag g_flag_init_hydrogen;

    void init_hydrogen() {
        std::call_once(g_flag_init_hydrogen, []() {
            if (0 != hydro_init()) {
                throw std::runtime_error{ "Failed to initizate libhydrogen" };
            }
        });
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
        return 0 == hydro_sign_verify(signature.data(), msg, msg_size, this->context_char(), public_key.data());
    }

}
