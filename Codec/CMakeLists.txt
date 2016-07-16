project(BtMigrateCodec)

set(${PROJECT_NAME}_SOURCES
    BencodeCodec.cpp
    JsonCodec.cpp
    PickleCodec.cpp
)

set(${PROJECT_NAME}_HEADERS
    BencodeCodec.h
    IStructuredDataCodec.h
    JsonCodec.h
    PickleCodec.h
)

add_library(${PROJECT_NAME}
    ${${PROJECT_NAME}_SOURCES}
    ${${PROJECT_NAME}_HEADERS}
)

target_link_libraries(${PROJECT_NAME}
    BtMigrateCommon
)

if(TARGET jsoncpp_lib_static)
    target_link_libraries(${PROJECT_NAME}
        jsoncpp_lib_static
    )
else()
    target_link_libraries(${PROJECT_NAME}
        jsoncpp_lib
    )
endif()
