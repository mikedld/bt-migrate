add_library(BtMigrateStore
    DebugTorrentStateIterator.cpp
    DebugTorrentStateIterator.h
    DelugeStateStore.cpp
    DelugeStateStore.h
    ITorrentStateStore.h
    TorrentStateStoreFactory.cpp
    TorrentStateStoreFactory.h
    TransmissionStateStore.cpp
    TransmissionStateStore.h
    rTorrentStateStore.cpp
    rTorrentStateStore.h
    uTorrentStateStore.cpp
    uTorrentStateStore.h)

target_link_libraries(BtMigrateStore
    PUBLIC
        BtMigrateCodec
        BtMigrateCommon
        BtMigrateTorrent)

target_link_libraries(BtMigrateStore
    PRIVATE
        Boost::boost
        Boost::filesystem
        jsoncons::jsoncons
        pugixml::pugixml
        Threads::Threads)
