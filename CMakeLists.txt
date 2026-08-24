cmake_minimum_required(VERSION 3.22)

project(RuneSchema LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    UE4SS
    GIT_REPOSITORY https://github.com/UE4SS-RE/RE-UE4SS.git
    GIT_TAG 0bfec09e
)
FetchContent_MakeAvailable(UE4SS)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
    safetyhook
    GIT_REPOSITORY https://github.com/cursey/safetyhook.git
    GIT_TAG 8a975dee560c424329dcb0e151f3649fa4b19146
)
FetchContent_MakeAvailable(safetyhook)

FetchContent_Declare(
    glaze
    GIT_REPOSITORY https://github.com/stephenberry/glaze.git
    GIT_TAG ea748038ac4f1579e0603ed1b0405cb6d573a4ea
)
FetchContent_MakeAvailable(glaze)

FetchContent_Declare(
    efsw
    GIT_REPOSITORY https://github.com/SpartanJ/efsw.git
    GIT_TAG 22f17a0bcdf3a4edf61f8b14328391463389e548
)
FetchContent_MakeAvailable(efsw)

FetchContent_Declare(
    zydis
    GIT_REPOSITORY https://github.com/zyantific/zydis.git
    GIT_TAG master
)
FetchContent_MakeAvailable(zydis)

set(TARGET RuneSchema)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(${TARGET} SHARED ${SRC_FILES})

target_include_directories(${TARGET} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(${TARGET} PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
)

target_link_libraries(${TARGET} PRIVATE
    UE4SS
    nlohmann_json::nlohmann_json
    safetyhook
    glaze::glaze
    efsw
    Zydis
    Zycore
)
