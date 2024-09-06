#include <array>
#include <iostream>
#include <string>

#include <gtest/gtest.h>

#include "daltools/common/crypto.h"


namespace {

    TEST(Crypto, KeyGen) {
        dal::KeyMetadata metadata;
        metadata.owner_name_ = "Sungmin Woo";
        metadata.email_ = "woos8899@gmail.com";
        metadata.description_ = "This is a test key from `daltest_crypto`";
        metadata.update_created_time();

        const auto [pub, sec] = dal::gen_data_key_pair();

        {
            const auto pub_serialized = dal::serialize_key(pub, metadata);
            const auto pub_recon = dal::deserialize_key(pub_serialized);

            ASSERT_TRUE(pub_recon.has_value());
            ASSERT_TRUE(pub_recon->first.index() == 0);
            ASSERT_EQ(pub.sign_key_, std::get<0>(pub_recon->first).sign_key_);
        }

        {
            const auto sec_serialized = dal::serialize_key(sec, metadata);
            const auto sec_recon = dal::deserialize_key(sec_serialized);

            ASSERT_TRUE(sec_recon.has_value());
            ASSERT_TRUE(sec_recon->first.index() == 1);
            ASSERT_EQ(sec.sign_key_, std::get<1>(sec_recon->first).sign_key_);
            ASSERT_EQ(
                sec.encrypt_key_, std::get<1>(sec_recon->first).encrypt_key_
            );
        }
    }


    TEST(Crypto, Encryption) {
        std::string plain_text = "Hello, world!";

        const auto [pk, sk] = dal::gen_data_key_pair();

        const auto cipher = dal::encrypt_data(
            sk, plain_text.data(), plain_text.size()
        );
        ASSERT_TRUE(cipher.has_value());

        const auto plain = dal::decrypt_data(
            sk, cipher->data(), cipher->size()
        );
        ASSERT_TRUE(plain.has_value());
        for (size_t i = 0; i < plain_text.size(); ++i)
            ASSERT_EQ(plain_text[i], plain->at(i));

        return;
    }


    TEST(Crypto, Signature) {
        std::string plain_text = "Hello, world!";

        const auto [pk, sk] = dal::gen_data_key_pair();

        const auto signature = dal::create_signature(
            sk, plain_text.data(), plain_text.size()
        );
        ASSERT_TRUE(signature.has_value());

        const auto verified = dal::verify_signature(
            pk,
            plain_text.data(),
            plain_text.size(),
            signature->data(),
            signature->size()
        );
        ASSERT_TRUE(verified);
    }

}  // namespace


int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
