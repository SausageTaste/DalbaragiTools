#include <cstring>
#include <iostream>

#include <daltools/crypto.h>


const char* const TEST_MSG = "This is cool";


namespace {

    bool test_key_export_import(const dal::crypto::IKey& key) {
        dal::crypto::KeyAttrib attrib;
        attrib.m_owner_name = "Sungmin Woo";
        attrib.m_email = "woos8899@gmail.com";
        attrib.m_description = "Test";

        const auto build1 = dal::crypto::build_key_store(key, attrib);
        const auto parse1 = dal::crypto::parse_key_store(build1, "");
        const auto build2 = dal::crypto::build_key_store(parse1.first, parse1.second);

        return build1 == build2;
    }

}


int main() {
    std::cout << "Test 2" << std::endl;

    dal::crypto::PublicKeySignature sign_mgr{"test"};
    const auto [public_key, secret_key] = sign_mgr.gen_keys();
    const auto [public_key2, secret_key2] = sign_mgr.gen_keys();

    if (!::test_key_export_import(public_key))
        return 1;
    if (!::test_key_export_import(secret_key))
        return 2;

    const auto signature = sign_mgr.create_signature(TEST_MSG, std::strlen(TEST_MSG), secret_key);
    if (!signature.has_value())
        return 3;

    if (true != sign_mgr.verify(TEST_MSG, std::strlen(TEST_MSG), public_key, signature.value()))
        return 4;
    if (false != sign_mgr.verify(TEST_MSG, std::strlen(TEST_MSG), public_key2, signature.value()))
        return 5;

    return 0;
}
