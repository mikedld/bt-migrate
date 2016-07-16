cmake_minimum_required(VERSION 3.5)
project(bt-migrate)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
else()
    add_compile_options(-Wall -Wextra)
endif()

set(${PROJECT_NAME}_SOURCES
    Box.cpp
    BoxHelper.cpp
    DebugTorrentStateIterator.cpp
    Exception.cpp
    ImportHelper.cpp
    Logger.cpp
    MigrationTransaction.cpp
    SignalHandler.cpp
    TorrentClient.cpp
    TorrentInfo.cpp
    TorrentStateStoreFactory.cpp
    Util.cpp

    BencodeCodec.cpp
    JsonCodec.cpp
    PickleCodec.cpp

    DelugeStateStore.cpp
    rTorrentStateStore.cpp
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
    ImportHelper.h
    Intention.h
    Logger.h
    MigrationTransaction.h
    SignalHandler.h
    ThreadSafeIterator.h
    TorrentClient.h
    TorrentInfo.h
    TorrentStateStoreFactory.h
    Util.h

    BencodeCodec.h
    JsonCodec.h
    PickleCodec.h

    DelugeStateStore.h
    rTorrentStateStore.h
    TransmissionStateStore.h
    uTorrentStateStore.h
)

find_package(Boost 1.50 REQUIRED COMPONENTS date_time filesystem locale program_options system)
find_package(jsoncpp REQUIRED)

add_executable(${PROJECT_NAME}
    ${${PROJECT_NAME}_SOURCES}
    ${${PROJECT_NAME}_HEADERS}
)
target_link_libraries(${PROJECT_NAME}
    Boost::date_time
    Boost::filesystem
    Boost::locale
    Boost::program_options
    Boost::system
    Boost::disable_autolinking
    Boost::dynamic_linking
    jsoncpp_lib_static
)
