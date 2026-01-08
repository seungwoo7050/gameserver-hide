cmake_minimum_required(VERSION 3.20)

project(DungeonHub LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(dungeonhub
    src/main.cpp
)

target_include_directories(dungeonhub
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)
