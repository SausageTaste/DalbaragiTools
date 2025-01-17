vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO SausageTaste/SungToolsCpp
    REF v0.1.1
    SHA512 24aa4fbc6fb18912f205aa2e2cf8eb3e0c907dc8bbf0c83281175c602e942e58802b8f9125c4e71cd4b5276a30415dfaeaa562cb68368e641139732e2587469d
)

vcpkg_configure_cmake(
    SOURCE_PATH ${SOURCE_PATH}
    PREFER_NINJA
    OPTIONS
        -DCMAKE_INSTALL_PREFIX=${CURRENT_PACKAGES_DIR}
)

vcpkg_install_cmake()
