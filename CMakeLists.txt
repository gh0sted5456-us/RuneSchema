cmake_minimum_required(VERSION 3.22)

project(RuneSchema LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(RUNESCHEMA_ENABLE_IPO "Enable interprocedural optimization for RuneSchema release targets" ON)
option(RUNESCHEMA_LIMIT_INLINING "Limit release inlining for smaller binaries" ON)

if(MSVC)
    # UE4SS inherits Shipping flags during configuration.
    get_filename_component(RUNESCHEMA_BUILD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
    set(CMAKE_CXX_FLAGS_RELEASE "/O1 /Os /Oi /Gy /DNDEBUG /experimental:deterministic")
endif()

include(FetchContent)

set(RUNESCHEMA_UE4SS_TAG "f6d5f9425a31f3e3a4fd185b40148f5aa4d81a44"
    CACHE STRING "Exact RE-UE4SS commit used to compile this RuneSchema binary")
FetchContent_Declare(
    UE4SS
    GIT_REPOSITORY https://github.com/UE4SS-RE/RE-UE4SS.git
    # UE4SS pins several public submodules with git@github.com URLs. Keep the
    # dependency reproducible without requiring every builder to configure an
    # SSH key or trust GitHub's SSH host key first.
    GIT_CONFIG "url.https://github.com/.insteadOf=git@github.com:"
    # C++ ABI pin for reproducible host-specific builds. UE4SS v4 does not
    # guarantee ABI compatibility with the 3.0.1 line, so release packaging
    # compiles one DLL per supported host from this shared RuneSchema source.
    GIT_TAG ${RUNESCHEMA_UE4SS_TAG}
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
list(FILTER SRC_FILES EXCLUDE REGEX "/src/Generator/InGameQuickMenu\\.cpp$")

add_library(${TARGET} SHARED ${SRC_FILES})

# API-1 Helpy native plugin. All Helpy UI/input ownership lives in this optional
# binary; multiplayer orchestration remains in the mandatory RuneSchema target.
add_library(RuneSchemaHelpyPlugin SHARED
    plugins/helpy/HelpyPlugin.cpp
    src/Generator/InGameQuickMenu.cpp
    src/Runtime/HostServices.cpp
    src/SDK/Classes/Custom/UObjectGlobals.cpp
    src/SDK/Classes/KismetSystemLibrary.cpp
    src/SDK/DragonWildsSignatures.cpp
    src/SDK/StaticClassStorage.cpp
    src/SDK/Helper/ActorHelper.cpp
    src/SDK/Helper/ArrayFieldPatch.cpp
    src/SDK/Helper/Memory.cpp
    src/SDK/Helper/PropertyHelper.cpp
    src/SDK/Structs/FSoftObjectPath.cpp
    src/SDK/Structs/Custom/FManagedValue.cpp
    src/SDK/Structs/Custom/FManagedStruct.cpp
    src/SDK/Structs/Custom/FScriptArrayHelper.cpp
    src/SDK/Structs/Custom/FScriptMapHelper.cpp
    src/SDK/Structs/Custom/FScriptSetHelper.cpp
    src/Utility/Config.cpp
    src/Utility/JsonHelpers.cpp)
set_target_properties(RuneSchemaHelpyPlugin PROPERTIES OUTPUT_NAME "RuneSchema.Helpy")
target_compile_definitions(RuneSchemaHelpyPlugin PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(RuneSchemaHelpyPlugin PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(RuneSchemaHelpyPlugin PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(RuneSchemaHelpyPlugin PRIVATE
    RuneSchemaJsonCore UE4SS nlohmann_json::nlohmann_json safetyhook glaze::glaze efsw Zydis Zycore)

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
if(RUNESCHEMA_ENABLE_IPO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT RUNESCHEMA_IPO_SUPPORTED OUTPUT RUNESCHEMA_IPO_ERROR LANGUAGES CXX)
    if(NOT RUNESCHEMA_IPO_SUPPORTED)
        message(FATAL_ERROR "Requested IPO is unavailable: ${RUNESCHEMA_IPO_ERROR}")
    endif()
    foreach(RUNESCHEMA_RELEASE_CONFIG RELEASE MINSIZEREL GAME__SHIPPING__WIN64 CASEPRESERVING__SHIPPING__WIN64 LESSEQUAL421__SHIPPING__WIN64)
        set_property(TARGET ${TARGET} RuneSchemaJsonCore RuneSchemaHelpyPlugin PROPERTY INTERPROCEDURAL_OPTIMIZATION_${RUNESCHEMA_RELEASE_CONFIG} TRUE)
    endforeach()
endif()
if(MSVC)
    # Pass path mapping as one target option, not a shell-split global flag.
    if(RUNESCHEMA_LIMIT_INLINING)
        foreach(RUNESCHEMA_SIZE_TARGET ${TARGET} RuneSchemaJsonCore RuneSchemaHelpyPlugin)
            target_compile_options(${RUNESCHEMA_SIZE_TARGET} PRIVATE
                "$<$<CONFIG:Game__Shipping__Win64,CasePreserving__Shipping__Win64,LessEqual421__Shipping__Win64,Release,MinSizeRel>:/Ob1>")
        endforeach()
    endif()
    target_compile_options(${TARGET} PRIVATE "/pathmap:${RUNESCHEMA_BUILD_ROOT}=RuneSchema")
    target_compile_options(RuneSchemaJsonCore PRIVATE "/pathmap:${RUNESCHEMA_BUILD_ROOT}=RuneSchema")
    target_compile_options(RuneSchemaHelpyPlugin PRIVATE "/pathmap:${RUNESCHEMA_BUILD_ROOT}=RuneSchema")
    target_link_options(RuneSchemaHelpyPlugin PRIVATE
        "$<$<CONFIG:Game__Shipping__Win64,CasePreserving__Shipping__Win64,LessEqual421__Shipping__Win64,Release,MinSizeRel>:/OPT:REF>"
        "$<$<CONFIG:Game__Shipping__Win64,CasePreserving__Shipping__Win64,LessEqual421__Shipping__Win64,Release,MinSizeRel>:/OPT:ICF>"
        "$<$<CONFIG:Game__Shipping__Win64,CasePreserving__Shipping__Win64,LessEqual421__Shipping__Win64,Release,MinSizeRel>:/INCREMENTAL:NO>"
        "$<$<CONFIG:Game__Shipping__Win64,CasePreserving__Shipping__Win64,LessEqual421__Shipping__Win64,Release,MinSizeRel>:/MAP>"
        "/PDBALTPATH:RuneSchema.Helpy.pdb")
    target_compile_options(${TARGET} PRIVATE
        "$<$<CONFIG:Game__Shipping__Win64,CasePreserving__Shipping__Win64,LessEqual421__Shipping__Win64,Release,MinSizeRel>:/O1>"
        "$<$<CONFIG:Game__Shipping__Win64,CasePreserving__Shipping__Win64,LessEqual421__Shipping__Win64,Release,MinSizeRel>:/Os>"
        "$<$<CONFIG:Game__Shipping__Win64,CasePreserving__Shipping__Win64,LessEqual421__Shipping__Win64,Release,MinSizeRel>:/Oi>"
        "$<$<CONFIG:Game__Shipping__Win64,CasePreserving__Shipping__Win64,LessEqual421__Shipping__Win64,Release,MinSizeRel>:/Gy>"
        "$<$<CONFIG:Game__Shipping__Win64,CasePreserving__Shipping__Win64,LessEqual421__Shipping__Win64,Release,MinSizeRel>:/Gw>"
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
