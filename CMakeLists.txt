cmake_minimum_required(VERSION 3.18)

option(USE_VCPKG "Use vcpkg to resolve dependencies" ON)

if(USE_VCPKG)
    include(vcpkg.cmake)
endif()

project(BtMigrate VERSION 0.1)

option(USE_FETCHCONTENT "Use FetchContent to resolve dependencies" ON)

if(USE_FETCHCONTENT)
    include(fetch.cmake)
endif()

find_package(Threads REQUIRED)

find_package(cxxopts REQUIRED)
find_package(fmt REQUIRED)
find_package(jsoncons REQUIRED)
find_package(pugixml REQUIRED)
find_package(SqliteOrm REQUIRED)

if(NOT TARGET digestpp::digestpp)
    find_package(digestpp REQUIRED)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_definitions(
    -DBOOST_ALL_NO_LIB
    -DBTMIGRATE_VERSION="${PROJECT_VERSION}")

if(WIN32)
    add_definitions(
        -DUNICODE
        -D_UNICODE
        -D_CRT_SECURE_NO_WARNINGS
        -D_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
        -DWIN32_LEAN_AND_MEAN
        -DNOMINMAX)
endif()

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

include_directories(.)

add_subdirectory(Codec)
add_subdirectory(Common)
add_subdirectory(Store)
add_subdirectory(Torrent)

add_executable(BtMigrate
    ImportHelper.cpp
    ImportHelper.h
    MigrationTransaction.cpp
    MigrationTransaction.h
    main.cpp)

target_link_libraries(BtMigrate
    PUBLIC
        BtMigrateCommon
    PRIVATE
        BtMigrateStore
        BtMigrateTorrent)

target_link_libraries(BtMigrate
    PRIVATE
        cxxopts::cxxopts
        fmt::fmt)
