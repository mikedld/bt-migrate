project(BtMigrateCommon)

set(${PROJECT_NAME}_SOURCES
    Exception.cpp
    Logger.cpp
    SignalHandler.cpp
    Util.cpp
)

set(${PROJECT_NAME}_HEADERS
    Exception.h
    IFileStreamProvider.h
    IForwardIterator.h
    Logger.h
    SignalHandler.h
    ThreadSafeIterator.h
    Util.h
)

add_library(${PROJECT_NAME}
    ${${PROJECT_NAME}_SOURCES}
    ${${PROJECT_NAME}_HEADERS}
)

target_link_libraries(${PROJECT_NAME}
    Boost::boost
    Boost::date_time
    Boost::filesystem
    Boost::disable_autolinking
    Boost::dynamic_linking
    Threads::Threads
)
