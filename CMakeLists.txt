cmake_minimum_required(VERSION 3.22)

project(RuneSchema LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    # UE4SS derives its Shipping flags from CMAKE_CXX_FLAGS_RELEASE while it is
    # configured, so establish RuneSchema's size policy before adding UE4SS.
    get_filename_component(RUNESCHEMA_BUILD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
    set(CMAKE_CXX_FLAGS_RELEASE "/O1 /Os /Oi /Gy /DNDEBUG /experimental:deterministic /pathmap:${RUNESCHEMA_BUILD_ROOT}=RuneSchema")
endif()

include(FetchContent)

FetchContent_Declare(
    UE4SS
    GIT_REPOSITORY https://github.com/UE4SS-RE/RE-UE4SS.git
    GIT_TAG 0bfec09e
)
FetchContent_MakeAvailable(UE4SS)

if(MSVC AND TARGET UE4SS)
    target_link_options(UE4SS PRIVATE "/PDBALTPATH:UE4SS.pdb")
endif()

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)
add_subdirectory(core)

FetchContent_Declare(
    safetyhook
    GIT_REPOSITORY https://github.com/cursey/safetyhook.git
    GIT_TAG 8a975dee560c424329dcb0e151f3649fa4b19146
)
FetchContent_MakeAvailable(safetyhook)

FetchContent_Declare(
    glaze
    GIT_REPOSITORY https://github.com/stephenberry/glaze.git
    GIT_TAG 3a850807501d98d23bab4bdc5af64d8d4e83e6bc
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
    GIT_TAG a2278f1d254e492f6a6b39f6cb5d1f5d515659dc
)
FetchContent_MakeAvailable(zydis)

set(TARGET RuneSchema)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
list(FILTER SRC_FILES EXCLUDE REGEX "/src/Core/")

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
    RuneSchemaJsonCore
    UE4SS
    nlohmann_json::nlohmann_json
    safetyhook
    glaze::glaze
    efsw
    Zydis
    Zycore
)

# Apply size optimization to UE4SS's custom Shipping configurations.
if(MSVC)
    target_compile_options(${TARGET} PRIVATE
        "$<$<CONFIG:Game__Shipping__Win64,CasePreserving__Shipping__Win64,LessEqual421__Shipping__Win64,Release,MinSizeRel>:/O1>"
        "$<$<CONFIG:Game__Shipping__Win64,CasePreserving__Shipping__Win64,LessEqual421__Shipping__Win64,Release,MinSizeRel>:/Os>"
        "$<$<CONFIG:Game__Shipping__Win64,CasePreserving__Shipping__Win64,LessEqual421__Shipping__Win64,Release,MinSizeRel>:/Oi>"
        "$<$<CONFIG:Game__Shipping__Win64,CasePreserving__Shipping__Win64,LessEqual421__Shipping__Win64,Release,MinSizeRel>:/Gy>"
    )
    target_link_options(${TARGET} PRIVATE
        "$<$<CONFIG:Game__Shipping__Win64,CasePreserving__Shipping__Win64,LessEqual421__Shipping__Win64,Release,MinSizeRel>:/OPT:REF>"
        "$<$<CONFIG:Game__Shipping__Win64,CasePreserving__Shipping__Win64,LessEqual421__Shipping__Win64,Release,MinSizeRel>:/OPT:ICF>"
        "$<$<CONFIG:Game__Shipping__Win64,CasePreserving__Shipping__Win64,LessEqual421__Shipping__Win64,Release,MinSizeRel>:/INCREMENTAL:NO>"
        "$<$<CONFIG:Game__Shipping__Win64,CasePreserving__Shipping__Win64,LessEqual421__Shipping__Win64,Release,MinSizeRel>:/MAP>"
        "$<$<CONFIG:Game__Shipping__Win64,CasePreserving__Shipping__Win64,LessEqual421__Shipping__Win64,Release,MinSizeRel>:/MAPINFO:EXPORTS>"
        "/PDBALTPATH:RuneSchema.pdb"
    )
endif()
