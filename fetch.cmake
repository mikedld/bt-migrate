include(FetchContent)

FetchContent_Declare(digestpp
    GIT_REPOSITORY https://github.com/kerukuro/digestpp.git
    GIT_TAG e5ace19f96f951772404d4587ea471b1be21b811)

FetchContent_MakeAvailable(digestpp)

add_library(digestpp::digestpp INTERFACE IMPORTED)
target_include_directories(digestpp::digestpp INTERFACE "${digestpp_SOURCE_DIR}")
