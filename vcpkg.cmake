include(FetchContent)

FetchContent_Declare(vcpkg
    GIT_REPOSITORY https://github.com/microsoft/vcpkg.git
    GIT_TAG c7e96f2a5b73b3278b004aa88abec2f8ebfb43b5)
FetchContent_MakeAvailable(vcpkg)

set(VCPKG_VERBOSE ON)

set(CMAKE_TOOLCHAIN_FILE ${vcpkg_SOURCE_DIR}/scripts/buildsystems/vcpkg.cmake
    CACHE STRING "CMake toolchain file")
