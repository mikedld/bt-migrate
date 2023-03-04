include(FetchContent)

FetchContent_Declare(vcpkg
    GIT_REPOSITORY https://github.com/microsoft/vcpkg.git
    GIT_TAG 2023.02.24)
FetchContent_MakeAvailable(vcpkg)

set(VCPKG_VERBOSE ON CACHE INTERNAL "")

set(CMAKE_TOOLCHAIN_FILE ${vcpkg_SOURCE_DIR}/scripts/buildsystems/vcpkg.cmake
    CACHE STRING "CMake toolchain file")
