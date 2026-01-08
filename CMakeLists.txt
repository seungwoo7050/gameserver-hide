cmake_minimum_required(VERSION 3.20)

project(DungeonHub LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

add_executable(dungeonhub
    src/main.cpp
    src/net/auth.cpp
    src/net/codec.cpp
    src/net/protocol.cpp
    src/net/server.cpp
    src/net/session.cpp
)

target_include_directories(dungeonhub
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

if(BUILD_TESTING)
    add_executable(dungeonhub_tests
        tests/net_tests.cpp
        src/net/auth.cpp
        src/net/codec.cpp
        src/net/protocol.cpp
        src/net/server.cpp
        src/net/session.cpp
    )
    target_include_directories(dungeonhub_tests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/src
    )
    add_test(NAME dungeonhub_unit_tests COMMAND dungeonhub_tests)
    add_test(NAME dungeonhub_runs COMMAND dungeonhub)
endif()
