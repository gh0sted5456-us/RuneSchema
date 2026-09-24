cmake_minimum_required(VERSION 3.22)
project(RuneSchemaJsonCore LANGUAGES CXX)
if(NOT TARGET nlohmann_json::nlohmann_json)
    if(NOT EXISTS "${RUNESCHEMA_JSON_INCLUDE_DIR}/nlohmann/json.hpp")
        message(FATAL_ERROR "Set RUNESCHEMA_JSON_INCLUDE_DIR to the pinned nlohmann-json include directory")
    endif()
    add_library(runeschema_json_headers INTERFACE)
    target_include_directories(runeschema_json_headers INTERFACE "${RUNESCHEMA_JSON_INCLUDE_DIR}")
    add_library(nlohmann_json::nlohmann_json ALIAS runeschema_json_headers)
endif()
add_library(RuneSchemaJsonCore STATIC
    ../src/Core/ConfigFiles.cpp
    ../src/Core/JsonDocument.cpp
    ../src/Core/JsonArrayPatch.cpp
    ../src/Core/JsonLoadOrderMerge.cpp
    ../src/Core/JsonPatchDirective.cpp
    ../src/Core/UsmapIndex.cpp)
target_compile_features(RuneSchemaJsonCore PUBLIC cxx_std_23)
target_include_directories(RuneSchemaJsonCore PUBLIC ../include)
target_link_libraries(RuneSchemaJsonCore PUBLIC nlohmann_json::nlohmann_json)
if(MSVC)
    set_property(TARGET RuneSchemaJsonCore PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    target_compile_options(RuneSchemaJsonCore PRIVATE /utf-8 /Zc:preprocessor /experimental:deterministic
        "$<$<CONFIG:Release,MinSizeRel,Game__Shipping__Win64,CasePreserving__Shipping__Win64,LessEqual421__Shipping__Win64>:/O1;/Os;/Oi;/Gy;/Gw>")
endif()
if(PROJECT_IS_TOP_LEVEL)
    if(MSVC)
        add_compile_options(/EHsc)
    endif()
    string(REPLACE "/DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    enable_testing()
    foreach(test asset-aliases quest-definition quest-progress quest-native-contract quest-registry-plan spawn-radius quest-repeat-policy quest-save-ownership quest-receipt-journal quest-reward-tiers quest-stage-progress owned-content-ledger registry-patch-plan)
        add_executable(${test} ../tests/${test}.cpp)
        target_link_libraries(${test} PRIVATE RuneSchemaJsonCore)
        if(MSVC)
            set_property(TARGET ${test} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
        endif()
        add_test(NAME ${test} COMMAND ${test})
    endforeach()
    add_test(NAME quest-example COMMAND quest-definition "${CMAKE_CURRENT_SOURCE_DIR}/../../extras/examples/FetchQuest/quests/10-Supper.json")
    add_executable(quest-inventory-routing ../tests/quest-inventory-routing.cpp)
    target_link_libraries(quest-inventory-routing PRIVATE RuneSchemaJsonCore)
    if(MSVC)
        set_property(TARGET quest-inventory-routing PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    endif()
    add_test(NAME quest-inventory-routing COMMAND quest-inventory-routing
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/NpcQuestCompletion.inl"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/NpcQuestAcquisition.inl")
    add_test(NAME staged-quest-example COMMAND quest-definition "${CMAKE_CURRENT_SOURCE_DIR}/../../extras/examples/StagedQuest/quests/70-TwoPatrols.json")
    add_executable(tree-quest ../tests/tree-quest.cpp)
    target_link_libraries(tree-quest PRIVATE RuneSchemaJsonCore)
    if(MSVC)
        set_property(TARGET tree-quest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    endif()
    add_test(NAME tree-quest COMMAND tree-quest "${CMAKE_CURRENT_SOURCE_DIR}/../../extras/examples/TreeQuest/RuneSchema2VendorTest")
    add_test(NAME quest-netid-capture COMMAND quest-registry-plan "${CMAKE_CURRENT_SOURCE_DIR}/../../docs/evidence/Results_inspector_1789329932085256.json")
    add_executable(fetch-quest-example ../tests/fetch-quest-example.cpp)
    target_link_libraries(fetch-quest-example PRIVATE RuneSchemaJsonCore)
    if(MSVC)
        set_property(TARGET fetch-quest-example PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    endif()
    add_test(NAME fetch-quest-example COMMAND fetch-quest-example "${CMAKE_CURRENT_SOURCE_DIR}/../../extras/examples/FetchQuest")
    add_executable(quest-marker-example ../tests/quest-marker-example.cpp)
    target_link_libraries(quest-marker-example PRIVATE RuneSchemaJsonCore)
    if(MSVC)
        set_property(TARGET quest-marker-example PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    endif()
    add_test(NAME quest-marker-example COMMAND quest-marker-example "${CMAKE_CURRENT_SOURCE_DIR}/../../extras/examples/QuestMarker")
    add_executable(quest-area-example ../tests/quest-area-example.cpp)
    target_link_libraries(quest-area-example PRIVATE RuneSchemaJsonCore)
    if(MSVC)
        set_property(TARGET quest-area-example PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    endif()
    add_test(NAME quest-area-example COMMAND quest-area-example "${CMAKE_CURRENT_SOURCE_DIR}/../../extras/examples/QuestArea" "${CMAKE_CURRENT_SOURCE_DIR}/../../extras/examples/QuestMarker")
    add_executable(event-example ../tests/event-example.cpp)
    add_executable(event-cleanup-contract ../tests/event-cleanup-contract.cpp)
    target_link_libraries(event-cleanup-contract PRIVATE RuneSchemaJsonCore)
    if(MSVC)
        set_property(TARGET event-cleanup-contract PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    endif()
    add_test(NAME event-cleanup-contract COMMAND event-cleanup-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Loader/EventRuntime.h")
    target_link_libraries(event-example PRIVATE RuneSchemaJsonCore)
    if(MSVC)
        set_property(TARGET event-example PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    endif()
    add_test(NAME event-example COMMAND event-example "${CMAKE_CURRENT_SOURCE_DIR}/../../extras/examples")
    add_executable(grumble-waves ../tests/grumble-waves.cpp)
    target_link_libraries(grumble-waves PRIVATE RuneSchemaJsonCore)
    add_test(NAME grumble-waves COMMAND grumble-waves "${CMAKE_CURRENT_SOURCE_DIR}/../../extras/examples/GrumbleWaves/RuneSchema2VendorTest")
    add_test(NAME quest-captured-contract COMMAND quest-native-contract "${CMAKE_CURRENT_SOURCE_DIR}/../tests/evidence/quest-native-contract.json")
    add_executable(dialogue-definition ../tests/dialogue-definition.cpp)
    target_link_libraries(dialogue-definition PRIVATE RuneSchemaJsonCore)
    if(MSVC)
        set_property(TARGET dialogue-definition PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    endif()
    add_test(NAME dialogue-definition COMMAND dialogue-definition)
    add_test(NAME dialogue-story COMMAND dialogue-definition "${CMAKE_CURRENT_SOURCE_DIR}/../../extras/examples/JonesyStory/dialogue")
    add_test(NAME kill-quest-example COMMAND quest-definition "${CMAKE_CURRENT_SOURCE_DIR}/../../extras/examples/KillQuest/quests/60-GoblinPatrol.json")
    add_test(NAME kill-quest-dialogue COMMAND dialogue-definition "${CMAKE_CURRENT_SOURCE_DIR}/../../extras/examples/KillQuest/dialogue")
    add_executable(dialogue-progress ../tests/dialogue-progress.cpp)
    target_link_libraries(dialogue-progress PRIVATE RuneSchemaJsonCore)
    if(MSVC)
        set_property(TARGET dialogue-progress PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    endif()
    add_test(NAME dialogue-progress COMMAND dialogue-progress)
    foreach(test quest-kill-credit event-definition dialogue-lifecycle human-pose field-patches equipment-rules diagnostic-presets json-document native-contract diagnostic-export player-archetype player-activity-events activity-rule-template loader-schemas virtual-definition-id patch-conflicts trace-profile save-report asset-template loader-template string-replacements starter-preflight niagara-preset reference-export mod-order-policy vendor-offers vendor-spawn-gate vendor-stage-policy capture-traversal runtime-layout runtime-search-job recipe-unlock-policy npc-catalog)
        add_executable(${test} ../tests/${test}.cpp)
        target_link_libraries(${test} PRIVATE RuneSchemaJsonCore)
        if(MSVC)
            set_property(TARGET ${test} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
        endif()
        add_test(NAME ${test} COMMAND ${test})
    endforeach()
    add_executable(appearance-resolver ../tests/appearance-resolver.cpp)
    add_executable(quantum-banker ../tests/quantum-banker.cpp)
    add_executable(save-cleanup ../tests/save-cleanup.cpp)
    add_executable(diagnostic-library ../tests/diagnostic-library.cpp)
    target_link_libraries(diagnostic-library PRIVATE RuneSchemaJsonCore)
    add_test(NAME diagnostic-library COMMAND diagnostic-library)
    target_link_libraries(save-cleanup PRIVATE RuneSchemaJsonCore)
    add_test(NAME save-cleanup COMMAND save-cleanup)
    target_link_libraries(quantum-banker PRIVATE RuneSchemaJsonCore)
    add_test(NAME quantum-banker COMMAND quantum-banker "${CMAKE_CURRENT_SOURCE_DIR}/../../extras/examples/QuantumBanker")
    add_executable(journal-placement ../tests/journal-placement.cpp)
    target_link_libraries(journal-placement PRIVATE RuneSchemaJsonCore)
    add_test(NAME journal-placement COMMAND journal-placement)
    add_executable(journal-failure-isolation ../tests/journal-failure-isolation.cpp)
    add_executable(journal-save-ownership ../tests/journal-save-ownership.cpp)
    target_link_libraries(journal-save-ownership PRIVATE RuneSchemaJsonCore)
    add_test(NAME journal-save-ownership COMMAND journal-save-ownership)
    add_executable(npc-cleanup-contract ../tests/npc-cleanup-contract.cpp)
    add_executable(asset-provenance-contract ../tests/asset-provenance-contract.cpp)
    add_test(NAME asset-provenance-contract COMMAND asset-provenance-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/AssetProvenance.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsAssetModLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsRecipeModLoader.cpp")
    add_executable(journal-soft-reference-contract ../tests/journal-soft-reference-contract.cpp)
    add_test(NAME journal-soft-reference-contract COMMAND journal-soft-reference-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsJournalModLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/JournalGroupPlacement.inl"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/JournalHierarchy.inl"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsBuildingModLoader.cpp")
    add_executable(quest-marker-readiness ../tests/quest-marker-readiness.cpp)
    add_test(NAME quest-marker-readiness COMMAND quest-marker-readiness
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Loader/QuestService.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Loader/QuestStageRuntime.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Loader/NativeQuestStageProgress.h")
    add_executable(quest-gameplay-owner ../tests/quest-gameplay-owner.cpp)
    add_test(NAME quest-gameplay-owner COMMAND quest-gameplay-owner
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Loader/GameplayTestContext.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsNpcLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/NpcQuestPresentation.inl"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/NpcQuestActions.inl")
    add_executable(time-of-day-contract ../tests/time-of-day-contract.cpp)
    add_test(NAME time-of-day-contract COMMAND time-of-day-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Loader/TimeOfDayRuntime.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsNpcLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsSpawnLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Generator/LoaderSchemas.h")
    add_test(NAME npc-cleanup-contract COMMAND npc-cleanup-contract "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsNpcLoader.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/NpcCleanup.inl")
    add_test(NAME journal-failure-isolation COMMAND journal-failure-isolation "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsJournalModLoader.cpp")
    add_executable(resource-additional-drops ../tests/resource-additional-drops.cpp)
    add_test(NAME resource-additional-drops COMMAND resource-additional-drops
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsSpawnLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/SpawnAdditionalDrops.inl"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Generator/SpawnToolsPanel.h")
    add_executable(resource-scale-idempotence ../tests/resource-scale-idempotence.cpp)
    add_test(NAME resource-scale-idempotence COMMAND resource-scale-idempotence
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsSpawnLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/SpawnTools.inl"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Loader/DragonWildsSpawnLoader.h")
    add_executable(building-preview-safety ../tests/building-preview-safety.cpp)
    add_test(NAME building-preview-safety COMMAND building-preview-safety
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/SpawnTools.inl"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsSpawnLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Generator/SpawnToolsPanel.h")
    add_executable(building-clone-contract ../tests/building-clone-contract.cpp)
    add_test(NAME building-clone-contract COMMAND building-clone-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsBuildingModLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Generator/LoaderSchemas.h")
    add_executable(static-building-assembly-contract ../tests/static-building-assembly-contract.cpp)
    add_test(NAME static-building-assembly-contract COMMAND static-building-assembly-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsBuildingModLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsSpawnLoader.cpp")
    add_executable(owned-save-cleanup-contract ../tests/owned-save-cleanup-contract.cpp)
    add_test(NAME owned-save-cleanup-contract COMMAND owned-save-cleanup-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Misc/DragonWildsDataRegistrar.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/AssetProvenance.cpp")
    add_executable(item-grid-pagination ../tests/item-grid-pagination.cpp)
    add_test(NAME item-grid-pagination COMMAND item-grid-pagination
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Generator/SpawnToolsPanel.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Generator/ItemGridPicker.h")
    add_executable(item-icon-grid ../tests/item-icon-grid.cpp)
    add_test(NAME item-icon-grid COMMAND item-icon-grid
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Generator/SpawnToolsPanel.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Generator/ItemGridPicker.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Generator/ItemIconCache.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Generator/ItemIconThumbnail.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Generator/AssetSearch.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/SpawnTools.inl"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/SpawnItemIcons.inl"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Generator/ToolRequest.h")
    add_executable(item-grid-picker-reuse ../tests/item-grid-picker-reuse.cpp)
    add_test(NAME item-grid-picker-reuse COMMAND item-grid-picker-reuse
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Generator/SpawnToolsPanel.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Generator/ItemGridPicker.h")
    add_executable(ingame-quick-menu-shell ../tests/ingame-quick-menu-shell.cpp)
    add_test(NAME ingame-quick-menu-shell COMMAND ingame-quick-menu-shell
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/dllmain.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Generator/InGameQuickMenu.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/SDK/Helper/ActorHelper.cpp")
    add_executable(helpy-instant-open ../tests/helpy-instant-open.cpp)
    add_test(NAME helpy-instant-open COMMAND helpy-instant-open
        "${CMAKE_CURRENT_SOURCE_DIR}/../plugins/helpy/HelpyPlugin.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Generator/InGameQuickMenu.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Generator/QuickMenuUI.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Runtime/PluginHost.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/dllmain.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Runtime/Storefront.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Runtime/PluginCatalog.h")
    add_executable(plugin-catalog-compatibility ../tests/plugin-catalog-compatibility.cpp)
    target_link_libraries(plugin-catalog-compatibility PRIVATE RuneSchemaJsonCore)
    if(MSVC)
        set_property(TARGET plugin-catalog-compatibility PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    endif()
    add_test(NAME plugin-catalog-compatibility COMMAND plugin-catalog-compatibility)
    add_executable(documentation-contract ../tests/documentation-contract.cpp)
    add_test(NAME documentation-contract COMMAND documentation-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../../AUTHORING-GUIDE.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../LOADER-WALKTHROUGHS.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../plugins/plugins.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../clean-base/RuneSchema/plugins/plugins.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../API-REFERENCE.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Runtime/RuneSchemaPluginApi.h")
    add_executable(usmap-index ../tests/usmap-index.cpp)
    target_link_libraries(usmap-index PRIVATE RuneSchemaJsonCore)
    if(MSVC)
        set_property(TARGET usmap-index PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    endif()
    add_test(NAME usmap-index COMMAND usmap-index)
    add_executable(native-binding-resolution ../tests/native-binding-resolution.cpp)
    add_test(NAME native-binding-resolution COMMAND native-binding-resolution
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/SDK/DragonWildsSignatures.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsMainLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/ModLoadOrder.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Runtime/PluginHost.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Runtime/Storefront.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/SDK/UnrealOffsets.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/dllmain.cpp")
    add_executable(storefront-lanes ../tests/storefront-lanes.cpp)
    target_include_directories(storefront-lanes PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
    target_compile_features(storefront-lanes PRIVATE cxx_std_23)
    add_test(NAME storefront-lanes COMMAND storefront-lanes)
    add_executable(state-storage-contract ../tests/state-storage-contract.cpp)
    add_test(NAME state-storage-contract COMMAND state-storage-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Runtime/HostServices.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsMainLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsBuildingModLoader.cpp")
    add_executable(asset-patch-v2-contract ../tests/asset-patch-v2-contract.cpp)
    target_link_libraries(asset-patch-v2-contract PRIVATE RuneSchemaJsonCore)
    if(MSVC)
        set_property(TARGET asset-patch-v2-contract PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    endif()
    add_test(NAME asset-patch-v2-contract COMMAND asset-patch-v2-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../../schemas/asset-patch-v2.schema.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../examples/CharacterCustomization/assets/domains/character_customization/hair-menu.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../examples/CharacterCustomization/assets/domains/character_customization/unlock-vanilla-beards.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsAssetModLoader.cpp")
    add_executable(vendor-category-refresh-contract ../tests/vendor-category-refresh-contract.cpp)
    add_test(NAME vendor-category-refresh-contract COMMAND vendor-category-refresh-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsNpcLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/NpcNativeShopRefresh.inl")
    add_executable(preview-discovery-regression ../tests/preview-discovery-regression.cpp)
    add_executable(quest-restart-ownership ../tests/quest-restart-ownership.cpp)
    add_test(NAME quest-restart-ownership COMMAND quest-restart-ownership "${CMAKE_CURRENT_SOURCE_DIR}/../include/Loader/QuestNativeAdapter.h" "${CMAKE_CURRENT_SOURCE_DIR}/../src/SDK/Helper/ActorHelper.cpp")
    add_test(NAME preview-discovery-regression COMMAND preview-discovery-regression
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/PlayerGhost.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/SDK/WeakObjectHandle.h")
    target_link_libraries(appearance-resolver PRIVATE RuneSchemaJsonCore)
    add_test(NAME appearance-resolver COMMAND appearance-resolver)
    add_test(NAME packaged-trace-profiles COMMAND trace-profile "${CMAKE_CURRENT_SOURCE_DIR}/../../extras/runtime/jobs/searches/trace-profiles")
    add_test(NAME packaged-diagnostic-presets COMMAND diagnostic-presets "${CMAKE_CURRENT_SOURCE_DIR}/../../extras/runtime/jobs/searches/presets")
    add_test(NAME multiplayer-cow-example COMMAND npc-catalog "${CMAKE_CURRENT_SOURCE_DIR}/../../extras/examples/MultiplayerCow/RuneSchemaMultiplayerTest")
    if(EXISTS "${RUNESCHEMA_GLAZE_INCLUDE_DIR}/glaze/glaze.hpp")
        add_executable(stone-story ../tests/stone-story.cpp)
        target_link_libraries(stone-story PRIVATE RuneSchemaJsonCore)
        if(MSVC)
            set_property(TARGET stone-story PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
        endif()
        add_test(NAME stone-story COMMAND stone-story "${CMAKE_CURRENT_SOURCE_DIR}/../../extras/examples/StoneStory/RuneSchema2VendorTest")
        add_executable(config-settings ../tests/config-settings.cpp)
        target_link_libraries(config-settings PRIVATE RuneSchemaJsonCore)
        target_include_directories(config-settings PRIVATE "${RUNESCHEMA_GLAZE_INCLUDE_DIR}")
        if(MSVC)
            set_property(TARGET config-settings PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
        endif()
        add_test(NAME config-settings COMMAND config-settings)
    endif()
endif()
