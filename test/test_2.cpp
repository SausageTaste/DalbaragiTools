#include <iostream>

#include <daltools/crypto.h>


const char* const TEST_MSG = "This is cool";


bool test_key_export_import(const dal::crypto::IKey& key) {
    const auto hex = key.make_hex_str();
    const auto bin = dal::crypto::IKey{hex};

    return key == bin;
}


int main() {
    std::cout << "Test 2" << std::endl;

    dal::crypto::PublicKeySignature sign_mgr{"test"};
    const auto [public_key, secret_key] = sign_mgr.gen_keys();
    const auto [public_key2, secret_key2] = sign_mgr.gen_keys();

    if (!test_key_export_import(public_key))
        return 1;
    if (!test_key_export_import(secret_key))
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
