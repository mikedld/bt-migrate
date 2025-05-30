include(FetchContent)

FetchContent_Declare(vcpkg
    GIT_REPOSITORY https://github.com/microsoft/vcpkg.git
    GIT_TAG 2025.04.09)
FetchContent_MakeAvailable(vcpkg)

set(VCPKG_VERBOSE ON CACHE INTERNAL "")

set(CMAKE_TOOLCHAIN_FILE ${vcpkg_SOURCE_DIR}/scripts/buildsystems/vcpkg.cmake
    CACHE STRING "CMake toolchain file")
