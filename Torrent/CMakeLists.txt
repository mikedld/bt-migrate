project(BtMigrateTorrent)

set(${PROJECT_NAME}_SOURCES
    Box.cpp
    BoxHelper.cpp
    TorrentClient.cpp
    TorrentInfo.cpp
)

set(${PROJECT_NAME}_HEADERS
    Box.h
    BoxHelper.h
    Intention.h
    TorrentClient.h
    TorrentInfo.h
)

add_library(${PROJECT_NAME}
    ${${PROJECT_NAME}_SOURCES}
    ${${PROJECT_NAME}_HEADERS}
)

target_link_libraries(${PROJECT_NAME}
    Boost::boost
    Boost::filesystem
    Boost::disable_autolinking
    Boost::dynamic_linking
)

target_link_libraries(${PROJECT_NAME}
    BtMigrateCodec
    BtMigrateCommon
    JsonCppLib
)
