add_library(BtMigrateCodec
    BencodeCodec.cpp
    BencodeCodec.h
    IStructuredDataCodec.cpp
    IStructuredDataCodec.h
    JsonCodec.cpp
    JsonCodec.h
    PickleCodec.cpp
    PickleCodec.h)

target_link_libraries(BtMigrateCodec
    PRIVATE
        BtMigrateCommon)

target_link_libraries(BtMigrateCodec
    PUBLIC
        jsoncons::jsoncons)
