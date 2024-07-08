#include <gtest/gtest.h>

#include "daltools/compression.h"


namespace {

    dal::uint8vec_t gen_test_data() {
        dal::uint8vec_t output;
        output.reserve(1024);

        for (int i = 0; i < 1024; ++i) {
            output.push_back(static_cast<uint8_t>(i % 256));
        }

        return output;
    }


    TEST(DaltestZip, CompareCompressionAlgorithms) {
        const auto test_data = ::gen_test_data();

        const auto zip_comp = dal::compress_zip(test_data);
        const auto bro_comp = dal::compress_bro(test_data);

        ASSERT_TRUE(zip_comp.has_value());
        ASSERT_TRUE(bro_comp.has_value());

        const auto zip_decomp = dal::decompress_zip(zip_comp.value());
        const auto bro_decomp = dal::decompress_bro(bro_comp.value());

        ASSERT_TRUE(zip_decomp.has_value());
        ASSERT_TRUE(bro_decomp.has_value());
        ASSERT_EQ(zip_decomp.value(), bro_decomp.value());
    }

}  // namespace


int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
