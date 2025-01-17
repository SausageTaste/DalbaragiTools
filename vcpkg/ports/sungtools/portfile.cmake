vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO SausageTaste/sungtools
    REF v0.1.1
    SHA512 ae74a7c9b40e3a7222932bb0c3f3f0e5dd59168838d4f157e74e278938824ae0857bc5262aa4954237d2ac5328e30f29a3c0b6a4f33a284c929f420fe475f528
)

vcpkg_install_copyright(FILE_LIST ${SOURCE_PATH}/LICENSE)

vcpkg_configure_cmake(
    SOURCE_PATH ${SOURCE_PATH}
    PREFER_NINJA
    OPTIONS
        -DCMAKE_INSTALL_PREFIX=${CURRENT_PACKAGES_DIR}
)

vcpkg_install_cmake()
