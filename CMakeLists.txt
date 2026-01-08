cmake_minimum_required(VERSION 3.20)

project(DungeonHub LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

add_executable(dungeonhub
    src/main.cpp
    src/dungeon/instance_manager.cpp
    src/match/match_queue.cpp
    src/net/auth.cpp
    src/net/codec.cpp
    src/net/protocol.cpp
    src/net/server.cpp
    src/net/session.cpp
    src/party/party.cpp
)

target_include_directories(dungeonhub
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

if(BUILD_TESTING)
    add_executable(dungeonhub_tests
        tests/net_tests.cpp
        src/dungeon/instance_manager.cpp
        src/match/match_queue.cpp
        src/net/auth.cpp
        src/net/codec.cpp
        src/net/protocol.cpp
        src/net/server.cpp
        src/net/session.cpp
        src/party/party.cpp
    )
    add_executable(dungeonhub_party_tests
        tests/party_match_tests.cpp
        src/dungeon/instance_manager.cpp
        src/match/match_queue.cpp
        src/party/party.cpp
    )
    add_executable(dungeonhub_dungeon_tests
        tests/dungeon_instance_tests.cpp
        src/dungeon/instance_manager.cpp
        src/party/party.cpp
    )
    target_include_directories(dungeonhub_tests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/src
    )
    target_include_directories(dungeonhub_party_tests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/src
    )
    target_include_directories(dungeonhub_dungeon_tests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/src
    )
    add_test(NAME dungeonhub_unit_tests COMMAND dungeonhub_tests)
    add_test(NAME dungeonhub_party_tests COMMAND dungeonhub_party_tests)
    add_test(NAME dungeonhub_dungeon_tests COMMAND dungeonhub_dungeon_tests)
    add_test(NAME dungeonhub_runs COMMAND dungeonhub)
endif()
