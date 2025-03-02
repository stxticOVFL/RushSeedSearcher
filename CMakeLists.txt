cmake_minimum_required(VERSION 3.12)
project(RushSeedSearcher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(RushSeedSearcher src/main.cpp)

target_include_directories(RushSeedSearcher PUBLIC
    src/
    include/
)

option(TRACY_ENABLE "" ON)

if(TRACY_ENABLE)
    include(FetchContent)
    FetchContent_Declare(
        tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG v0.11.1
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(tracy)

    add_compile_definitions(RushSeedSearcher USE_TRACY)
    target_link_libraries(RushSeedSearcher PUBLIC TracyClient)
endif()