cmake_minimum_required(VERSION 2.8)
project(bt-migrate)

if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
else()
    add_definitions(-std=c++11 -g -O2 -Wall -Wextra)
endif()

set(${PROJECT_NAME}_SOURCES
    Box.cpp
    BoxHelper.cpp
    DebugTorrentStateIterator.cpp
    Exception.cpp
    MigrationTransaction.cpp
    TorrentClient.cpp
    TorrentStateStoreFactory.cpp
    Util.cpp

    BencodeCodec.cpp
    JsonCodec.cpp
    PickleCodec.cpp

    DelugeStateStore.cpp
    TransmissionStateStore.cpp
    uTorrentStateStore.cpp

    main.cpp
)

set(${PROJECT_NAME}_HEADERS
    IFileStreamProvider.h
    IForwardIterator.h
    IStructuredDataCodec.h
    ITorrentStateStore.h

    Box.h
    BoxHelper.h
    DebugTorrentStateIterator.h
    Exception.h
    MigrationTransaction.h
    ThreadSafeIterator.h
    TorrentClient.h
    TorrentStateStoreFactory.h
    Util.h

    BencodeCodec.h
    JsonCodec.h
    PickleCodec.h

    DelugeStateStore.h
    TransmissionStateStore.h
    uTorrentStateStore.h
)

add_definitions(-DBOOST_ALL_NO_LIB=1 -DBOOST_ALL_DYN_LINK=1)

find_package(Boost 1.50 REQUIRED COMPONENTS date_time filesystem locale program_options system)

include_directories(
    ${PROJECT_SOURCE_DIR}
    3rd-party/jsoncpp/include
    ${Boost_INCLUDE_DIR}
)

set(JSONCPP_WITH_TESTS OFF CACHE BOOL "")
set(JSONCPP_WITH_POST_BUILD_UNITTEST OFF CACHE BOOL "")
add_subdirectory(3rd-party/jsoncpp)

# jsoncpp fucks up output directories
unset(CMAKE_ARCHIVE_OUTPUT_DIRECTORY CACHE)
unset(CMAKE_LIBRARY_OUTPUT_DIRECTORY CACHE)
unset(CMAKE_RUNTIME_OUTPUT_DIRECTORY CACHE)

add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_SOURCES} ${${PROJECT_NAME}_HEADERS})
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES} jsoncpp_lib)
