cmake_minimum_required(VERSION 3.5)
project(BtMigrate)

find_package(Boost 1.50 REQUIRED COMPONENTS date_time filesystem locale program_options system)
find_package(Threads REQUIRED)

add_library(jsoncons INTERFACE IMPORTED)
set_property(TARGET jsoncons PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/jsoncons/include")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
else()
    add_compile_options(-Wall -Wextra)
endif()

include_directories(.)

add_subdirectory(Codec)
add_subdirectory(Common)
add_subdirectory(Store)
add_subdirectory(Torrent)

set(${PROJECT_NAME}_SOURCES
    ImportHelper.cpp
    MigrationTransaction.cpp
    main.cpp
)

set(${PROJECT_NAME}_HEADERS
    ImportHelper.h
    MigrationTransaction.h
)

add_executable(${PROJECT_NAME}
    ${${PROJECT_NAME}_SOURCES}
    ${${PROJECT_NAME}_HEADERS}
)

target_link_libraries(${PROJECT_NAME}
    BtMigrateCommon
    BtMigrateStore
    BtMigrateTorrent
    Boost::date_time
    Boost::filesystem
    Boost::locale
    Boost::program_options
    Boost::disable_autolinking
    Boost::dynamic_linking
    Threads::Threads
)
