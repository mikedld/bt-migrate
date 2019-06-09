cmake_minimum_required(VERSION 3.5)
project(BtMigrate)

find_package(Boost 1.50 REQUIRED COMPONENTS date_time filesystem locale program_options system)
find_package(Threads REQUIRED)

add_library(jsoncons::jsoncons INTERFACE IMPORTED)
set_property(TARGET jsoncons::jsoncons PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/jsoncons/include")

add_library(pugixml::pugixml INTERFACE IMPORTED)
set_property(TARGET pugixml::pugixml PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/pugixml/src")
set_property(TARGET pugixml::pugixml PROPERTY INTERFACE_COMPILE_DEFINITIONS PUGIXML_HEADER_ONLY)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_definitions(-DBOOST_ALL_NO_LIB)

if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
else()
    add_compile_options(-Wall -Wextra)
    add_compile_options(-Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-padded -Wno-conversion -Wno-global-constructors -Wno-exit-time-destructors -Wno-return-std-move-in-c++11)
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
    PUBLIC
        Boost::filesystem
    PRIVATE
        Boost::date_time
        Boost::locale
        Boost::program_options
        Threads::Threads)
