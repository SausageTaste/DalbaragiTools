#include <string>
#include <iostream>

#include <daltools/crypto.h>


namespace {

    const std::string TEST_MSG = "This is a test message!";


    auto create_attrib() noexcept {
        dal::crypto::KeyAttrib attrib;
        attrib.m_owner_name = "Sungmin Woo";
        attrib.m_email = "woos8899@gmail.com";
        attrib.m_description = "Test";
        return attrib;
    }

    bool test_key_export_import(const dal::crypto::IKey& key, dal::crypto::KeyAttrib attrib) {
        attrib.m_type = key.key_type();

        const auto build1 = dal::crypto::build_key_store(key, attrib);
        const auto parse1 = dal::crypto::parse_key_store<dal::crypto::IKey>(build1, "");
        const auto build2 = dal::crypto::build_key_store(parse1.first, parse1.second);

        if (build1 != build2)
            return false;
        if (parse1.first != key)
            return false;
        if (parse1.second != attrib)
            return false;

        return true;
    }

}


int main() {
    const auto attrib = create_attrib();

    dal::crypto::PublicKeySignature sign_mgr{"test"};
    const auto [public_key, secret_key] = sign_mgr.gen_keys();

    if (!::test_key_export_import(public_key, attrib))
        return 1;
    if (!::test_key_export_import(secret_key, attrib))
        return 2;

    const auto signature = sign_mgr.create_signature(TEST_MSG.data(), TEST_MSG.size(), secret_key);
    if (!signature)
        return 3;
    if (!sign_mgr.verify(TEST_MSG.data(), TEST_MSG.size(), public_key, *signature))
        return 4;

    const auto [public_key2, secret_key2] = sign_mgr.gen_keys();
    if (sign_mgr.verify(TEST_MSG.data(), TEST_MSG.size(), public_key2, *signature))
        return 5;

    return 0;
}
