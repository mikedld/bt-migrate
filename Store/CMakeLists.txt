project(BtMigrateStore)

set(${PROJECT_NAME}_SOURCES
    DebugTorrentStateIterator.cpp
    TorrentStateStoreFactory.cpp
    DelugeStateStore.cpp
    rTorrentStateStore.cpp
    TransmissionStateStore.cpp
    uTorrentStateStore.cpp
)

set(${PROJECT_NAME}_HEADERS
    ITorrentStateStore.h
    DebugTorrentStateIterator.h
    TorrentStateStoreFactory.h
    DelugeStateStore.h
    rTorrentStateStore.h
    TransmissionStateStore.h
    uTorrentStateStore.h
)

add_library(${PROJECT_NAME}
    ${${PROJECT_NAME}_SOURCES}
    ${${PROJECT_NAME}_HEADERS}
)

target_link_libraries(${PROJECT_NAME}
    BtMigrateCodec
    BtMigrateCommon
    BtMigrateTorrent
    Boost::boost
    Boost::filesystem
    Boost::disable_autolinking
    Boost::dynamic_linking
    jsoncons
    pugixml
    Threads::Threads
)
