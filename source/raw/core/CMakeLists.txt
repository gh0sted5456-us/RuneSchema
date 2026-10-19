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
    ../src/Core/ModOrderState.cpp
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
    add_executable(event-contract-integration ../tests/event-contract-integration.cpp)
    target_link_libraries(event-contract-integration PRIVATE RuneSchemaJsonCore)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/event-contract-fixture/docs/schemas")
    add_test(NAME event-contract-integration COMMAND event-contract-integration "${CMAKE_CURRENT_BINARY_DIR}/event-contract-fixture")
    add_executable(quest-ownership-contract ../tests/quest-ownership-contract.cpp)
    target_link_libraries(quest-ownership-contract PRIVATE RuneSchemaJsonCore)
    add_test(NAME quest-ownership-contract COMMAND quest-ownership-contract "${CMAKE_CURRENT_SOURCE_DIR}/..")
    foreach(test buffered-trace item-catalog-filter diagnostic-log-file work-slice metadata-window)
        add_executable(${test} ../tests/${test}.cpp)
        target_link_libraries(${test} PRIVATE RuneSchemaJsonCore)
        add_test(NAME ${test} COMMAND ${test})
    endforeach()
    add_executable(logging-routing ../tests/logging-routing.cpp)
    target_include_directories(logging-routing BEFORE PRIVATE ../tests/logging/stubs)
    target_link_libraries(logging-routing PRIVATE RuneSchemaJsonCore)
    add_test(NAME logging-routing COMMAND logging-routing)
    add_executable(alphabetical-item-catalog ../tests/alphabetical-item-catalog.cpp)
    target_link_libraries(alphabetical-item-catalog PRIVATE RuneSchemaJsonCore)
    add_test(NAME alphabetical-item-catalog COMMAND alphabetical-item-catalog
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Core/ItemNameOrder.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../helpy/source/include/Core/ItemNameOrder.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/SpawnTools.inl")
    add_executable(slow-work ../tests/slow-work.cpp)
    target_link_libraries(slow-work PRIVATE RuneSchemaJsonCore)
    add_test(NAME slow-work COMMAND slow-work)
    add_executable(dedicated-helpy-contract ../tests/dedicated-helpy-contract.cpp)
    target_compile_features(dedicated-helpy-contract PRIVATE cxx_std_23)
    add_test(NAME dedicated-helpy-contract COMMAND dedicated-helpy-contract "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    add_executable(helpy-query-dispatch ../tests/helpy-query-dispatch.cpp ../src/Generator/HelpyQueryService.cpp ../src/Generator/ToolRequestState.cpp)
    target_link_libraries(helpy-query-dispatch PRIVATE RuneSchemaJsonCore)
    add_test(NAME helpy-query-dispatch COMMAND helpy-query-dispatch)
    add_executable(ue4ss-runtime-identity ../tests/ue4ss-runtime-identity.cpp)
    target_link_libraries(ue4ss-runtime-identity PRIVATE RuneSchemaJsonCore)
    add_test(NAME ue4ss-runtime-identity COMMAND ue4ss-runtime-identity "${CMAKE_CURRENT_SOURCE_DIR}/../include/Runtime/UE4SSCompatibility.h")
    add_executable(ue4ss-packages ../tests/ue4ss-packages.cpp)
    target_link_libraries(ue4ss-packages PRIVATE RuneSchemaJsonCore)
    target_compile_definitions(ue4ss-packages PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    add_test(NAME ue4ss-packages COMMAND ue4ss-packages "${CMAKE_CURRENT_BINARY_DIR}/ue4ss-package-fixtures")
    add_executable(building-menu-contract ../tests/building-menu-contract.cpp)
    target_compile_features(building-menu-contract PRIVATE cxx_std_23)
    add_test(NAME building-menu-contract COMMAND building-menu-contract "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    add_executable(placement-cache-regression ../tests/placement-cache-regression.cpp)
    target_compile_features(placement-cache-regression PRIVATE cxx_std_23)
    add_test(NAME placement-cache-regression COMMAND placement-cache-regression "${CMAKE_CURRENT_SOURCE_DIR}/..")
    add_executable(recipe-stations-contract ../tests/recipe-stations-contract.cpp)
    target_compile_features(recipe-stations-contract PRIVATE cxx_std_23)
    add_test(NAME recipe-stations-contract COMMAND recipe-stations-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/HelpyItemCompanions.inl"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../helpy/source/include/Generator/RecipeStudioPanel.h")
    add_executable(map-travel ../tests/map-travel.cpp)
    target_link_libraries(map-travel PRIVATE RuneSchemaJsonCore)
    add_test(NAME map-travel COMMAND map-travel)
    add_executable(map-travel-contract ../tests/map-travel-contract.cpp)
    target_compile_features(map-travel-contract PRIVATE cxx_std_23)
    add_test(NAME map-travel-contract COMMAND map-travel-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/MapTravel.inl"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/SpawnTools.inl"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsSpawnLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../helpy/source/include/Generator/MapPanel.h")
    add_executable(registry-reflection ../tests/registry-reflection.cpp)
    target_link_libraries(registry-reflection PRIVATE RuneSchemaJsonCore)
    add_test(NAME registry-reflection COMMAND registry-reflection)
    add_executable(native-function-discovery ../tests/native-function-discovery.cpp)
    target_link_libraries(native-function-discovery PRIVATE RuneSchemaJsonCore)
    if(WIN32)
        target_compile_definitions(native-function-discovery PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    endif()
    add_test(NAME native-function-discovery COMMAND native-function-discovery)
    add_executable(mod-order-state ../tests/mod-order-state.cpp)
    add_executable(icon-disk-maintenance ../tests/icon-disk-maintenance.cpp)
    target_link_libraries(icon-disk-maintenance PRIVATE RuneSchemaJsonCore)
    target_compile_definitions(icon-disk-maintenance PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    add_test(NAME icon-disk-maintenance COMMAND icon-disk-maintenance "${CMAKE_CURRENT_BINARY_DIR}/icon-disk-fixtures")
    target_link_libraries(mod-order-state PRIVATE RuneSchemaJsonCore)
    add_test(NAME mod-order-state COMMAND mod-order-state "${CMAKE_CURRENT_BINARY_DIR}/mod-order-state-fixtures")
    foreach(test recipe-batch-work player-carry-weight item-paging quest-region quest-definition-identity item-bay-layout ground-surface nearby-placement authored-file item-transfer grounding-retry item-cart recipe-workspace mounted-catalog character-entry-bypass-plan asset-preload-plan icon-capture-schedule helpy-kit-files consume-queue ordered-callbacks authoring-policy unreal-readiness ghost-tools-options surge-evade-rules)
        add_executable(${test} ../tests/${test}.cpp)
        target_link_libraries(${test} PRIVATE RuneSchemaJsonCore)
        add_test(NAME ${test} COMMAND ${test})
    endforeach()
    add_executable(ModFolderLayoutTests ../tests/ModFolderLayoutTests.cpp)
    target_link_libraries(ModFolderLayoutTests PRIVATE RuneSchemaJsonCore)
    add_test(NAME mod-folder-layout COMMAND ModFolderLayoutTests "${CMAKE_CURRENT_BINARY_DIR}/mod-folder-fixtures")
    add_executable(example-mods ../tests/example-mods.cpp)
    target_link_libraries(example-mods PRIVATE RuneSchemaJsonCore)
    add_test(NAME example-mods COMMAND example-mods "${CMAKE_CURRENT_SOURCE_DIR}/../../../docs/examples/RSv16/ExampleMods/RuneSchema2VendorTest")
    add_executable(startup-trace-optin ../tests/startup-trace-optin.cpp)
    target_compile_features(startup-trace-optin PRIVATE cxx_std_23)
    add_test(NAME startup-trace-optin COMMAND startup-trace-optin "${CMAKE_CURRENT_SOURCE_DIR}/../src/Utility/StartupTrace.cpp")
    add_executable(helpy-popup-input ../tests/helpy-popup-input.cpp)
    target_link_libraries(helpy-popup-input PRIVATE RuneSchemaJsonCore)
    add_test(NAME helpy-popup-input COMMAND helpy-popup-input "${CMAKE_CURRENT_SOURCE_DIR}/../src/Generator/HelpyStandaloneWindow.cpp")
    add_executable(npc-journal-unlocks ../tests/npc-journal-unlocks.cpp)
    target_link_libraries(npc-journal-unlocks PRIVATE RuneSchemaJsonCore)
    add_test(NAME npc-journal-unlocks COMMAND npc-journal-unlocks
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsNpcLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsJournalModLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsMainLoader.cpp")
    add_executable(helpy-lazy-thread ../tests/helpy-lazy-thread.cpp ../src/Generator/ToolRequestState.cpp)
    target_link_libraries(helpy-lazy-thread PRIVATE RuneSchemaJsonCore)
    add_test(NAME helpy-lazy-thread COMMAND helpy-lazy-thread)
    add_executable(alpha-ui-contract ../tests/alpha-ui-contract.cpp)
    target_compile_features(alpha-ui-contract PRIVATE cxx_std_23)
    add_test(NAME alpha-ui-contract COMMAND alpha-ui-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Generator/ItemGridPicker.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Generator/SpawnToolsPanel.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Generator/HelpyStandaloneWindow.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Generator/LazyCatalog.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Loader/NativeAssetPrefetch.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsRecipeModLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsMainLoader.cpp")
    add_executable(world-building-registry-validation ../tests/world-building-registry-validation.cpp)
    target_include_directories(world-building-registry-validation PRIVATE ../include)
    target_compile_features(world-building-registry-validation PRIVATE cxx_std_20)
    add_test(NAME world-building-registry-validation COMMAND world-building-registry-validation)
    add_executable(world-cleanup-policy ../tests/world-cleanup-policy.cpp)
    target_include_directories(world-cleanup-policy PRIVATE ../include)
    target_compile_features(world-cleanup-policy PRIVATE cxx_std_20)
    add_test(NAME world-cleanup-policy COMMAND world-cleanup-policy)
    foreach(test asset-aliases quest-definition quest-progress quest-native-contract quest-registry-plan spawn-radius quest-repeat-policy quest-save-ownership quest-receipt-journal quest-scroll quest-scroll-identity quest-reward-tiers quest-stage-progress quest-alternative-exchange npc-equipment-conditions registry-patch-plan cooked-pak-registry-manifest registry-manifest-summary recipe-discovery cooked-registry-shards)
        add_executable(${test} ../tests/${test}.cpp)
        target_link_libraries(${test} PRIVATE RuneSchemaJsonCore)
        if(MSVC)
            set_property(TARGET ${test} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
        endif()
        add_test(NAME ${test} COMMAND ${test})
    endforeach()
    add_executable(quest-cancellations ../tests/quest-cancellations.cpp)
    add_executable(questboard-walkthrough ../tests/questboard-walkthrough.cpp)
    target_link_libraries(questboard-walkthrough PRIVATE RuneSchemaJsonCore)
    add_test(NAME questboard-walkthrough COMMAND questboard-walkthrough "${CMAKE_CURRENT_SOURCE_DIR}/../../../docs/examples/QuestBoardWalkthrough")
    target_link_libraries(quest-cancellations PRIVATE RuneSchemaJsonCore)
    add_test(NAME quest-cancellations COMMAND quest-cancellations)
    add_executable(quest-scroll-routing ../tests/quest-scroll-routing.cpp)
    add_executable(bundled-quest-scroll ../tests/bundled-quest-scroll.cpp)
    target_link_libraries(bundled-quest-scroll PRIVATE RuneSchemaJsonCore)
    add_test(NAME bundled-quest-scroll COMMAND bundled-quest-scroll "${CMAKE_CURRENT_SOURCE_DIR}/../../plugins")
    add_test(NAME quest-scroll-routing COMMAND quest-scroll-routing
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/NpcQuestScroll.inl"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/NpcQuestActions.inl"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Loader/QuestScrollUseContract.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsAssetModLoader.cpp")
    add_executable(quest-inventory-routing ../tests/quest-inventory-routing.cpp)
    target_link_libraries(quest-inventory-routing PRIVATE RuneSchemaJsonCore)
    if(MSVC)
        set_property(TARGET quest-inventory-routing PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    endif()
    add_test(NAME quest-inventory-routing COMMAND quest-inventory-routing
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/NpcQuestCompletion.inl"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/NpcQuestAcquisition.inl"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/NpcQuestActions.inl"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/NpcDialogue.inl")
    add_executable(event-cleanup-contract ../tests/event-cleanup-contract.cpp)
    target_link_libraries(event-cleanup-contract PRIVATE RuneSchemaJsonCore)
    if(MSVC)
        set_property(TARGET event-cleanup-contract PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    endif()
    add_test(NAME event-cleanup-contract COMMAND event-cleanup-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Loader/EventRuntime.h")
    add_test(NAME quest-captured-contract COMMAND quest-native-contract "${CMAKE_CURRENT_SOURCE_DIR}/../tests/evidence/quest-native-contract.json")
    add_executable(dialogue-definition ../tests/dialogue-definition.cpp)
    target_link_libraries(dialogue-definition PRIVATE RuneSchemaJsonCore)
    if(MSVC)
        set_property(TARGET dialogue-definition PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    endif()
    add_test(NAME dialogue-definition COMMAND dialogue-definition)
    add_test(NAME documented-kill-quest COMMAND quest-definition "${CMAKE_CURRENT_SOURCE_DIR}/../../../docs/examples/RSv16/quests/60-GoblinPatrol.json")
    add_test(NAME documented-kill-dialogue COMMAND dialogue-definition "${CMAKE_CURRENT_SOURCE_DIR}/../../../docs/examples/RSv16/dialogue")
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
    add_executable(save-cleanup ../tests/save-cleanup.cpp)
    add_executable(diagnostic-library ../tests/diagnostic-library.cpp)
    target_link_libraries(diagnostic-library PRIVATE RuneSchemaJsonCore)
    add_test(NAME diagnostic-library COMMAND diagnostic-library)
    target_link_libraries(save-cleanup PRIVATE RuneSchemaJsonCore)
    add_test(NAME save-cleanup COMMAND save-cleanup)
    add_executable(identity-only-cleanup ../tests/identity-only-cleanup.cpp)
    target_link_libraries(identity-only-cleanup PRIVATE RuneSchemaJsonCore)
    add_test(NAME identity-only-cleanup COMMAND identity-only-cleanup)
    add_executable(persistence-diagnostic-ledger ../tests/persistence-diagnostic-ledger.cpp)
    target_link_libraries(persistence-diagnostic-ledger PRIVATE RuneSchemaJsonCore)
    add_test(NAME persistence-diagnostic-ledger COMMAND persistence-diagnostic-ledger)
    add_executable(journal-placement ../tests/journal-placement.cpp)
    target_link_libraries(journal-placement PRIVATE RuneSchemaJsonCore)
    add_test(NAME journal-placement COMMAND journal-placement)
    add_executable(journal-entry-identity ../tests/journal-entry-identity.cpp)
    target_link_libraries(journal-entry-identity PRIVATE RuneSchemaJsonCore)
    add_test(NAME journal-entry-identity COMMAND journal-entry-identity)
    add_executable(journal-unlock-condition ../tests/journal-unlock-condition.cpp)
    target_link_libraries(journal-unlock-condition PRIVATE RuneSchemaJsonCore)
    add_test(NAME journal-unlock-condition COMMAND journal-unlock-condition)
    add_executable(journal-failure-isolation ../tests/journal-failure-isolation.cpp)
    add_executable(journal-wingdk-lane ../tests/journal-wingdk-lane.cpp)
    add_test(NAME journal-wingdk-lane COMMAND journal-wingdk-lane
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/JournalHierarchy.inl"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Loader/JournalWinGDKContract.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Generator/AppearanceWinGDKSignatures.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/JournalJsonBridge.inl"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsJournalModLoader.cpp")
    add_executable(main-menu-log-budget ../tests/main-menu-log-budget.cpp)
    add_test(NAME main-menu-log-budget COMMAND main-menu-log-budget
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsRecipeModLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsAssetModLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsBlueprintModLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsMainLoader.cpp")
    add_executable(runtime-widget-contract ../tests/runtime-widget-contract.cpp)
    add_test(NAME runtime-widget-contract COMMAND runtime-widget-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Loader/DragonWildsBlueprintModLoader.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsBlueprintModLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../docs/examples/RestoreAppearance/blueprints/10-RestoreAppearance.jsonc")
    add_executable(ue4ss-bootstrap-contract ../tests/ue4ss-bootstrap-contract.cpp)
    add_test(NAME ue4ss-bootstrap-contract COMMAND ue4ss-bootstrap-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../build/build.ps1"
        "${CMAKE_CURRENT_SOURCE_DIR}/../CMakeLists.txt")
    add_executable(logic-pak-bridge-contract ../tests/logic-pak-bridge-contract.cpp)
    target_link_libraries(logic-pak-bridge-contract PRIVATE RuneSchemaJsonCore)
    add_test(NAME logic-pak-bridge-contract COMMAND logic-pak-bridge-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Runtime/LogicPakBridge.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../docs/PAK-CONTENT-GUIDE.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Runtime/BPModLoaderIntegration.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../ue4ss/scripts/logicmods-loader.lua")
    add_executable(ue4ss-companion-bootstrap-contract ../tests/ue4ss-companion-bootstrap-contract.cpp)
    target_compile_features(ue4ss-companion-bootstrap-contract PRIVATE cxx_std_23)
    add_test(NAME ue4ss-companion-bootstrap-contract COMMAND ue4ss-companion-bootstrap-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../../ue4ss/scripts/main.lua"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../build/build.ps1"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../docs/AUTHORING-GUIDE.md")
    add_executable(load-order-authority-contract ../tests/load-order-authority-contract.cpp)
    add_test(NAME load-order-authority-contract COMMAND load-order-authority-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/ModLoadOrder.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsMainLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Runtime/PluginCatalog.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Runtime/PluginHost.cpp")
    add_executable(persistence-mode-contract ../tests/persistence-mode-contract.cpp)
    add_test(NAME persistence-mode-contract COMMAND persistence-mode-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Utility/Config.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsJournalModLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsRecipeModLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/NpcQuestActions.inl"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Loader/NativeDialogueSave.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/Spawn/PlayerRules.cpp")
    add_executable(loader-lifecycle-contract ../tests/loader-lifecycle-contract.cpp)
    add_test(NAME loader-lifecycle-contract COMMAND loader-lifecycle-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsModLoaderBase.cpp")
    add_executable(recipe-reference-contract ../tests/recipe-reference-contract.cpp)
    add_test(NAME recipe-reference-contract COMMAND recipe-reference-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsRecipeModLoader.cpp")
    add_executable(preview-refresh-contract ../tests/preview-refresh-contract.cpp)
    add_test(NAME preview-refresh-contract COMMAND preview-refresh-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/PlayerGhost.cpp")
    add_executable(journal-save-ownership ../tests/journal-save-ownership.cpp)
    target_link_libraries(journal-save-ownership PRIVATE RuneSchemaJsonCore)
    add_test(NAME journal-save-ownership COMMAND journal-save-ownership)
    add_executable(journal-player-selection ../tests/journal-player-selection.cpp)
    target_link_libraries(journal-player-selection PRIVATE RuneSchemaJsonCore)
    add_test(NAME journal-player-selection COMMAND journal-player-selection
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsJournalModLoader.cpp)
    add_executable(npc-cleanup-contract ../tests/npc-cleanup-contract.cpp)
    add_executable(asset-provenance-contract ../tests/asset-provenance-contract.cpp)
    add_executable(wearable-schema ../tests/wearable-schema.cpp)
    add_executable(recipe-catalog ../tests/recipe-catalog.cpp)
    target_link_libraries(recipe-catalog PRIVATE RuneSchemaJsonCore)
    add_test(NAME recipe-catalog COMMAND recipe-catalog)
    target_compile_features(wearable-schema PRIVATE cxx_std_23)
    target_link_libraries(wearable-schema PRIVATE RuneSchemaJsonCore)
    add_test(NAME wearable-schema COMMAND wearable-schema
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsAssetModLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Loader/EquipmentWearableBridge.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/LiveCloneAuthoring.inl")
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
    add_executable(summon-kill-credit ../tests/summon-kill-credit.cpp)
    target_include_directories(summon-kill-credit PRIVATE ../include)
    target_compile_features(summon-kill-credit PRIVATE cxx_std_20)
    add_test(NAME summon-kill-credit COMMAND summon-kill-credit)
    add_executable(summon-kill-credit-contract ../tests/summon-kill-credit-contract.cpp)
    add_test(NAME summon-kill-credit-contract COMMAND summon-kill-credit-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Loader/SummonAttribution.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Loader/QuestDamageCredit.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Loader/NativeSummonKillCredit.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsNpcLoader.cpp")
    add_executable(time-of-day-contract ../tests/time-of-day-contract.cpp)
    add_test(NAME time-of-day-contract COMMAND time-of-day-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Loader/TimeOfDayRuntime.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsNpcLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsSpawnLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Generator/LoaderSchemas.h")
    add_executable(quest-building-lifecycle-contract ../tests/quest-building-lifecycle-contract.cpp)
    add_test(NAME quest-building-lifecycle-contract COMMAND quest-building-lifecycle-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsSpawnLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Generator/LoaderSchemas.h")
    add_executable(temporary-resource-lifecycle-contract ../tests/temporary-resource-lifecycle-contract.cpp)
    add_test(NAME temporary-resource-lifecycle-contract COMMAND temporary-resource-lifecycle-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsSpawnLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsNpcLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Loader/EventRuntime.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsMainLoader.cpp"
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
    add_executable(registry-bridge-lifecycle-contract ../tests/registry-bridge-lifecycle-contract.cpp)
    add_test(NAME registry-bridge-lifecycle-contract COMMAND registry-bridge-lifecycle-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Runtime/RegistryBridge.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Runtime/RegistryBridge.h")
    add_executable(presentation-transport-contract ../tests/presentation-transport-contract.cpp)
    add_test(NAME presentation-transport-contract COMMAND presentation-transport-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Runtime/RegistryBridge.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Runtime/RegistryBridge.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsRegistryLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../plugins/RSNetworking/paks/Networking/Networking_P.pak"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/SDK/Helper/ActorHelper.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/SDK/Helper/ActorHelper.cpp")
    add_executable(summoning-authority-contract ../tests/summoning-authority-contract.cpp)
    add_test(NAME summoning-authority-contract COMMAND summoning-authority-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Runtime/RegistryBridge.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Runtime/RegistryBridge.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsRegistryLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../docs/SUMMONING-AUTHORITY-BRIDGE.md")
    add_executable(bridge-diagnostics ../tests/bridge-diagnostics.cpp)
    target_link_libraries(bridge-diagnostics PRIVATE RuneSchemaJsonCore)
    add_test(NAME bridge-diagnostics COMMAND bridge-diagnostics)
    add_executable(cooked-registry-discovery-contract ../tests/cooked-registry-discovery-contract.cpp)
    add_executable(cooked-loader-enrollment-contract ../tests/cooked-loader-enrollment-contract.cpp)
    add_executable(recipe-discovery-contract ../tests/recipe-discovery-contract.cpp)
    add_executable(cooked-registry-shards-contract ../tests/cooked-registry-shards-contract.cpp)
    target_compile_features(cooked-registry-shards-contract PRIVATE cxx_std_23)
    add_test(NAME cooked-registry-shards-contract COMMAND cooked-registry-shards-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsRegistryLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Core/CookedPakRegistryManifest.h")
    target_compile_features(recipe-discovery-contract PRIVATE cxx_std_23)
    add_test(NAME recipe-discovery-contract COMMAND recipe-discovery-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsRecipeModLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/NativeRecipeDiscovery.cpp")
    target_compile_features(cooked-loader-enrollment-contract PRIVATE cxx_std_23)
    add_test(NAME cooked-loader-enrollment-contract COMMAND cooked-loader-enrollment-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsMainLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsRecipeModLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsJournalModLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsRegistryLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Misc/DragonWildsDataRegistrar.cpp")
    add_test(NAME cooked-registry-discovery-contract COMMAND cooked-registry-discovery-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsRegistryLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsMainLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Core/MountedModRegistryOwners.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../docs/SHARED-REGISTRY-BRIDGE-PAK.md")
    add_executable(static-building-assembly-contract ../tests/static-building-assembly-contract.cpp)
    add_test(NAME static-building-assembly-contract COMMAND static-building-assembly-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsBuildingModLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsSpawnLoader.cpp")
    add_executable(owned-save-cleanup-contract ../tests/owned-save-cleanup-contract.cpp)
    add_test(NAME owned-save-cleanup-contract COMMAND owned-save-cleanup-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Misc/DragonWildsDataRegistrar.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Core/RegistryProvenance.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Loader/QuestService.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsMainLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Core/PersistencePruner.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsAssetModLoader.cpp")
    add_executable(network-identity-plan ../tests/network-identity-plan.cpp)
    add_executable(networking-content-contract ../tests/networking-content-contract.cpp)
    target_link_libraries(networking-content-contract PRIVATE RuneSchemaJsonCore)
    target_compile_features(networking-content-contract PRIVATE cxx_std_23)
    add_test(NAME networking-content-contract COMMAND networking-content-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../../UnrealAuthoring/RuneSchemaBridgeAuthoring/Source/RuneSchemaBridgeAuthoring/Private/RuneSchemaBridgeAuthoringModule.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../UnrealAuthoring/RuneSchemaBridgeAuthoring/Source/RuneSchemaBridgeAuthoring/Public/RuneSchemaBridgeAuthoringLibrary.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../build/build.ps1"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../content/RuneSchemaNetworking/CONTENT-RECEIPT.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../UnrealAuthoring/RuneSchemaBridgeAuthoringModule.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../UnrealAuthoring/RuneSchemaBridgeAuthoringLibrary.h")
    target_compile_features(network-identity-plan PRIVATE cxx_std_23)
    target_include_directories(network-identity-plan PRIVATE ../include)
    add_test(NAME network-identity-plan COMMAND network-identity-plan)
    add_executable(recipe-item-routing ../tests/recipe-item-routing.cpp)
    target_link_libraries(recipe-item-routing PRIVATE RuneSchemaJsonCore)
    add_test(NAME recipe-item-routing COMMAND recipe-item-routing)
    add_executable(item-catalog-layout ../tests/item-catalog-layout.cpp)
    target_link_libraries(item-catalog-layout PRIVATE RuneSchemaJsonCore)
    add_test(NAME item-catalog-layout COMMAND item-catalog-layout)
    add_executable(registration-diagnostics ../tests/registration-diagnostics.cpp)
    target_link_libraries(registration-diagnostics PRIVATE RuneSchemaJsonCore)
    add_test(NAME registration-diagnostics COMMAND registration-diagnostics
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Loader/EquipmentWearableBridge.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Misc/DragonWildsDataRegistrar.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Generator/AssetSearch.cpp")
    add_executable(character-entry-recovery-contract ../tests/character-entry-recovery-contract.cpp)
    add_test(NAME character-entry-recovery-contract COMMAND character-entry-recovery-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Core/CharacterEntryRecovery.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/SDK/DragonWildsSignatures.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsMainLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Core/CharacterEntryReturnGate.h")
    add_executable(appearance-defaults-contract ../tests/appearance-defaults-contract.cpp)
    add_test(NAME appearance-defaults-contract COMMAND appearance-defaults-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Core/AppearanceDefaults.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Core/PersistencePruner.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/Spawn/PlayerRules.cpp")
    add_executable(item-grid-pagination ../tests/item-grid-pagination.cpp)
    add_test(NAME item-grid-pagination COMMAND item-grid-pagination
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Generator/SpawnToolsPanel.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Generator/ItemGridPicker.h")
    add_executable(icon-draw-context ../tests/icon-draw-context.cpp)
    target_compile_features(icon-draw-context PRIVATE cxx_std_23)
    target_include_directories(icon-draw-context PRIVATE ../include)
    add_test(NAME icon-draw-context COMMAND icon-draw-context)
    add_executable(ground-surface-contract ../tests/ground-surface-contract.cpp)
    target_compile_features(ground-surface-contract PRIVATE cxx_std_23)
    add_test(NAME ground-surface-contract COMMAND ground-surface-contract "${CMAKE_CURRENT_SOURCE_DIR}/..")
    add_executable(native-placement-contract ../tests/native-placement-contract.cpp)
    target_compile_features(native-placement-contract PRIVATE cxx_std_23)
    add_test(NAME native-placement-contract COMMAND native-placement-contract "${CMAKE_CURRENT_SOURCE_DIR}/..")
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
    add_executable(helpy-standalone-only ../tests/helpy-standalone-only.cpp)
    target_link_libraries(helpy-standalone-only PRIVATE RuneSchemaJsonCore)
    add_test(NAME helpy-standalone-only COMMAND helpy-standalone-only
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
        "${CMAKE_CURRENT_SOURCE_DIR}/../../helpy")
    add_executable(helpy-instant-open ../tests/helpy-instant-open.cpp)
    add_test(NAME helpy-instant-open COMMAND helpy-instant-open
        "${CMAKE_CURRENT_SOURCE_DIR}/../../helpy/source/plugins/helpy/HelpyPlugin.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../helpy/source/src/Generator/HelpyStandaloneWindow.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../helpy/source/include/Generator/LazyCatalog.h"
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
    add_executable(recipe-placement-contract ../tests/recipe-placement-contract.cpp)
    target_compile_features(recipe-placement-contract PRIVATE cxx_std_23)
    add_test(NAME recipe-placement-contract COMMAND recipe-placement-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsRecipeModLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../docs/LOADER-WALKTHROUGHS.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/SDK/Helper/PropertyHelper.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Core/RecipeItemRouting.h")
    add_executable(raw-datatable-contract ../tests/raw-datatable-contract.cpp)
    add_test(NAME raw-datatable-contract COMMAND raw-datatable-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsRawTableLoader.cpp")
    add_executable(trace-job-contract ../tests/trace-job-contract.cpp)
    target_link_libraries(trace-job-contract PRIVATE RuneSchemaJsonCore)
    add_test(NAME trace-job-contract COMMAND trace-job-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsRawTableLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/Utility/Config.h")
    add_executable(loader-folder-case ../tests/loader-folder-case.cpp)
    target_link_libraries(loader-folder-case PRIVATE RuneSchemaJsonCore)
    add_test(NAME loader-folder-case COMMAND loader-folder-case)
    add_executable(usmap-index ../tests/usmap-index.cpp)
    target_link_libraries(usmap-index PRIVATE RuneSchemaJsonCore)
    if(MSVC)
        set_property(TARGET usmap-index PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    endif()
    add_test(NAME usmap-index COMMAND usmap-index)
    add_executable(native-binding-resolution ../tests/native-binding-resolution.cpp)
    target_compile_features(native-binding-resolution PRIVATE cxx_std_17)
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
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsBuildingModLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/Spawn/PlayerRules.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Misc/DragonWildsDataRegistrar.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Generator/SaveViewer.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Generator/SaveCleanupPanel.inl"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Core/PersistencePruner.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Core/AppearanceDefaults.cpp")
    add_executable(equipment-storefront-lane ../tests/equipment-storefront-lane.cpp)
    add_test(NAME equipment-storefront-lane COMMAND equipment-storefront-lane
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/DragonWildsEquipmentLoader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/Loader/EquipmentShadowveil.cpp")
    add_executable(asset-patch-v2-contract ../tests/asset-patch-v2-contract.cpp)
    target_link_libraries(asset-patch-v2-contract PRIVATE RuneSchemaJsonCore)
    if(MSVC)
        set_property(TARGET asset-patch-v2-contract PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    endif()
    add_test(NAME asset-patch-v2-contract COMMAND asset-patch-v2-contract
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../docs/schemas/asset-patch-v2.schema.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../docs/examples/CharacterCustomization/assets/domains/character_customization/hair-menu.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../docs/examples/CharacterCustomization/assets/domains/character_customization/unlock-vanilla-beards.json"
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
    add_test(NAME documented-trace-profiles COMMAND trace-profile "${CMAKE_CURRENT_SOURCE_DIR}/../../../docs/examples/RSv16/TraceProfiles")
    add_test(NAME documented-diagnostic-presets COMMAND diagnostic-presets "${CMAKE_CURRENT_SOURCE_DIR}/../../../docs/examples/RSv16/DiagnosticPresets")
    if(EXISTS "${RUNESCHEMA_GLAZE_INCLUDE_DIR}/glaze/glaze.hpp")
        add_executable(runeschema-settings-editor ../tests/runeschema-settings-editor.cpp)
        target_link_libraries(runeschema-settings-editor PRIVATE RuneSchemaJsonCore)
        target_include_directories(runeschema-settings-editor PRIVATE "${RUNESCHEMA_GLAZE_INCLUDE_DIR}")
        add_test(NAME runeschema-settings-editor COMMAND runeschema-settings-editor)
        add_executable(config-compatibility ../tests/config-compatibility.cpp)
        target_link_libraries(config-compatibility PRIVATE RuneSchemaJsonCore)
        target_include_directories(config-compatibility PRIVATE "${RUNESCHEMA_GLAZE_INCLUDE_DIR}")
        add_test(NAME config-compatibility COMMAND config-compatibility)
        add_executable(config-settings ../tests/config-settings.cpp)
        target_link_libraries(config-settings PRIVATE RuneSchemaJsonCore)
        target_include_directories(config-settings PRIVATE "${RUNESCHEMA_GLAZE_INCLUDE_DIR}")
        if(MSVC)
            set_property(TARGET config-settings PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
        endif()
        add_test(NAME config-settings COMMAND config-settings)
        add_executable(config-migration ../tests/config-migration.cpp)
        target_link_libraries(config-migration PRIVATE RuneSchemaJsonCore)
        target_include_directories(config-migration PRIVATE "${RUNESCHEMA_GLAZE_INCLUDE_DIR}")
        if(MSVC)
            set_property(TARGET config-migration PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
        endif()
        add_test(NAME config-migration COMMAND config-migration "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
    endif()
endif()
