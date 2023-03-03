add_library(BtMigrateTorrent
    Box.cpp
    Box.h
    BoxHelper.cpp
    BoxHelper.h
    Intention.h
    TorrentClient.cpp
    TorrentClient.h
    TorrentInfo.cpp
    TorrentInfo.h)

target_link_libraries(BtMigrateTorrent
    PRIVATE
        BtMigrateCodec
        BtMigrateCommon)

target_link_libraries(BtMigrateTorrent
    PUBLIC
        jsoncons
    PRIVATE
        Boost::boost
        fmt::fmt)
