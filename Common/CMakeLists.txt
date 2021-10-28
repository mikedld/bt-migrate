add_library(BtMigrateCommon
    Exception.cpp
    Exception.h
    IFileStreamProvider.cpp
    IFileStreamProvider.h
    IForwardIterator.h
    Logger.cpp
    Logger.h
    Sha1.cpp
    Sha1.h
    SignalHandler.cpp
    SignalHandler.h
    ThreadSafeIterator.h
    Util.cpp
    Util.h)

target_link_libraries(BtMigrateCommon PUBLIC jsoncons)
