#include <gtest/gtest.h>

#define KHRONOS_STATIC
#include <ktx.h>


namespace {

    TEST(DaltestKtx, SimpleTest) {
        const auto path = "C:/Users/AORUS/Desktop/refine_output/src/damn.ktx";

        ktxTexture* texture;
        KTX_error_code result;
        ktx_size_t offset;
        ktx_uint8_t* image;
        ktx_uint32_t level, layer, faceSlice;

        result = ktxTexture_CreateFromNamedFile(
            path, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture
        );

        // Retrieve information about the texture from fields in the ktxTexture
        // such as:
        ktx_uint32_t numLevels = texture->numLevels;
        ktx_uint32_t baseWidth = texture->baseWidth;
        ktx_bool_t isArray = texture->isArray;

        // Retrieve a pointer to the image for a specific mip level, array layer
        // & face or depth slice.
        level = 1;
        layer = 0;
        faceSlice = 3;
        result = ktxTexture_GetImageOffset(
            texture, level, layer, faceSlice, &offset
        );
        image = ktxTexture_GetData(texture) + offset;
        // ...
        // Do something with the texture image.
        // ...
        ktxTexture_Destroy(texture);
    }

}  // namespace


int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
