#!/usr/bin/env python3
"""Test the actual portable UI/batch code, then check native source wiring.

Python 3.9+ and g++/clang++ are required. No downloads or Unreal runtime needed.
These tests do not compile the production DLL or execute Windows/Unreal calls.
"""
from __future__ import annotations
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default=os.environ.get('CXX', 'g++'))
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--source-only', action='store_true', help='Skip portable C++ compilation and run native source-wiring assertions only')
    parser.add_argument('--json-include', type=Path, help='Optional directory containing the project nlohmann/json.hpp; enables real mailbox tests')
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if not args.source_only and not compiler:
        raise SystemExit(f'Compiler not found: {args.compiler}')
    tests = Path(__file__).resolve().parent
    raw = tests.parents[1]
    if not args.source_only:
      with tempfile.TemporaryDirectory(prefix='runeschema-gui-test-') as directory:
        binary = Path(directory) / ('ui_tests.exe' if os.name == 'nt' else 'ui_tests')
        command = [compiler, '-std=c++20', '-Wall', '-Wextra', '-Werror', '-pedantic',
                   '-I', str(raw/'include'), str(tests/'test_ui.cpp'), '-o', str(binary)]
        if args.sanitize:
            command[1:1] = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g']
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True)
      with tempfile.TemporaryDirectory(prefix='runeschema-refinement-test-') as directory:
        binary = Path(directory) / ('refinements.exe' if os.name == 'nt' else 'refinements')
        command = [compiler, '-std=c++20', '-Wall', '-Wextra', '-Werror', '-pedantic',
                   '-I', str(raw/'include'), str(tests/'test_refinements.cpp'), '-o', str(binary)]
        if args.sanitize:
            command[1:1] = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g']
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True)
    if not args.source_only and args.json_include:
        if not (args.json_include/'nlohmann/json.hpp').is_file():
            raise SystemExit('--json-include must contain nlohmann/json.hpp')
        with tempfile.TemporaryDirectory(prefix='runeschema-mailbox-test-') as directory:
            binary = Path(directory)/('mailbox.exe' if os.name == 'nt' else 'mailbox')
            command = [compiler, '-std=c++20', '-Wall', '-Wextra', '-Werror', '-pedantic',
                       '-I', str(raw/'include'), '-I', str(args.json_include), str(tests/'test_mailbox.cpp'), '-o', str(binary)]
            if args.sanitize:
                command[1:1] = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g']
            subprocess.run(command, check=True)
            subprocess.run([str(binary)], check=True)
    elif not args.source_only:
        print('SKIP: real JSON mailbox test (supply --json-include). Native catalog/spawn backend is source-checked, not compiled by this runner.')
    encoder = (raw/'include/Generator/QuickMenuSpawnRequest.h').read_text(encoding='utf-8')
    rules = (raw/'include/Generator/QuickMenuCatalogRules.h').read_text(encoding='utf-8')
    native = (raw/'src/Generator/InGameQuickMenu.cpp').read_text(encoding='utf-8')
    header = (raw/'include/Generator/InGameQuickMenu.h').read_text(encoding='utf-8')
    mailbox = (raw/'include/Generator/ToolRequest.h').read_text(encoding='utf-8')
    tools = (raw/'src/Loader/SpawnTools.inl').read_text(encoding='utf-8')
    index = (raw/'src/Loader/QuickMenuCatalog.inl').read_text(encoding='utf-8')
    settings = (raw/'include/Generator/ItemGridPicker.h').read_text(encoding='utf-8')
    loader = (raw/'src/Loader/DragonWildsSpawnLoader.cpp').read_text(encoding='utf-8')
    dll = (raw/'src/dllmain.cpp').read_text(encoding='utf-8')
    adapter = (raw/'src/Generator/QuickMenuInputAdapter.inl').read_text(encoding='utf-8')
    diagnostic = native[native.index('void InGameQuickMenu::RenderInputDiagnostic('):native.index('UObject* InGameQuickMenu::FindLocalPlayerController(')]
    checks = [
        ('object parameter access does not use the missing UE4SS virtuals', '->SetObjectPropertyValue(' not in adapter and '->GetObjectPropertyValue(' not in adapter and '->GetObjectPropertyValue(' not in native),
        ('only bounded plain by-value object arguments are copied', 'CPF_UObjectWrapper' in adapter and 'STR("ObjectProperty")' in adapter and 'ReadMenuObjectParameter' in adapter),
        ('argument fill precedes ownership before native dispatch', adapter.index('PopulateNativeMenuMode(controller, call, buffer);', adapter.index('void InvokeNativeMenuMode(')) < adapter.index('if (dispatchOwned) *dispatchOwned = true;')),
        ('both parameter packets are tested before acquiring controller locks', native.index('PreflightNativeMenuMode(controller,enter)') < native.index('CaptureControllerInput(controller);',native.index('void InGameQuickMenu::SyncViewportInput(')) and native.index('PreflightNativeMenuMode(controller,exit)') < native.index('CaptureControllerInput(controller);',native.index('void InGameQuickMenu::SyncViewportInput('))),
        ('all root-close paths use reasoned transition', 'Close();' not in native and 'm_open.store(next' not in native),
        ('diagnostic close summaries include frame counts and time', 'Log<LogLevel::Verbose>(STR("Helpy closed [' in native and 'last-frame-age={} ms' in native),
        ('close state is reported once per transition', 'if(!wasOpen)return;' in native),
        ('focus notifications suspend input while preserving the menu', 'message->message==WM_KILLFOCUS' in native and 'Keep the menu model open across alt-tab' in native and 'menu->m_inputReady.store(false,std::memory_order_release)' in native),
        ('other viewport cannot reset the active captured owner', native.index('if(LiveMenuObject(owned)&&viewport!=owned)return;') < native.index('auto* world=viewport?viewport->GetWorld():nullptr;')),
        ('owner safety checks and timeout safeguards retained', 'CloseReason::OwnerExpired' in native and 'CloseReason::FrameTimeout' in native and 'CloseReason::FirstFrameTimeout' in native),
        ('input mode uses native UMG entry and exit functions', 'WidgetBlueprintLibrary:SetInputMode_UIOnlyEx' in adapter and 'WidgetBlueprintLibrary:SetInputMode_GameOnly' in adapter),
        ('input adapter has no viewport offset-map or exported getter dependency', 'MemberOffsets' not in adapter and 'GetProcAddress' not in adapter and 'bIgnoreInput' not in adapter),
        ('engine mouse routing replaces manual cursor clipping', 'ClipCursor(' not in native and 'InvokeNativeMenuMode(controller,enter,&m_nativeModeOwned)' in native),
        ('entry validates exit before changing mode', native.index('PreflightNativeMenuMode(controller,exit)') < native.index('InvokeNativeMenuMode(controller,enter,&m_nativeModeOwned)')),
        ('later reflected mode requests are not overwritten on close', 'ObserveNativeInputMode' in native and 'CloseWithReason(CloseReason::NativeModeChange,requested)' in native),
        ('startup reports hook readiness without exposing module paths', 'Helpy Canvas and input hooks installed.' in native and 'GetModuleFileNameW' not in native),
        ('read-only diagnostic cannot submit commands or consume input', 'Submit(' not in diagnostic and 'RenderCanvas(' not in diagnostic and 'NOT INTERACTIVE' in diagnostic),
        ('submitted frame and interactive readiness have separate states', 'm_frameSubmitted.load(std::memory_order_acquire),m_openRequestedAt.load()' in native and 'm_inputDiagnostic.load' in native),
        ('native PostRender invokes the actual UI model', 'm_ui.Render(mx,my)' in native and 's_postRenderHook.call(viewport,canvas)' in native),
        ('validated native viewport query replaces SizeX-only dependency', 'PlayerController:GetViewportSize' in native and 'CPF_OutParm' in native and 'params==2' in native),
        ('same-process thread-scoped input hook', 'SetWindowsHookExW(WH_GETMESSAGE,&InputThunk,nullptr,thread)' in native and 'pid!=GetCurrentProcessId()' in native),
        ('input events are bounded and generation checked', 'm_events.size()<256' in native and 'event.generation!=generation' in native),
        ('text translation and UTF16 decoding wired', 'TranslateMessage(message)' in native and 'm_highSurrogate' in native),
        ('configured Helpy hotkey passes through input hook', 'static_cast<WPARAM>(HelpyHotkeys::Active.load())' in native),
        ('render failure closes and releases input', 'menu->AbortOpen(stage,e.what())' in native and 'const bool restored=ReleaseViewportInput()' in native),
        ('world ownership is weak and resettable', 'WeakObjectHandle m_lockedViewport,m_lockedController,m_world' in header and 'm_world.Reset();m_seenWorld=false' in native),
        ('independent input cleanup tick is installed', 'RegisterEngineTickPostCallback' in native and 'TickInputSafety();' in native and 'UnregisterCallback(m_inputSafetyHook)' in native),
        ('input waits for a successfully drawn first frame', 'if(!menu->m_inputReady.load' in native and native.index('stage="Canvas drawing";menu->RenderCanvas(canvas)') < native.index('m_inputReady.exchange(true')),
        ('world reset requests index cancellation', 'SpawnToolRequests::Cancel();' in native),
        ('old global arrows do not hijack text input', 'm_quickMenu->SelectPrevious()' not in dll and 'm_quickMenu->SelectNext()' not in dll),
        ('receipts consumed only for own completion', 'if(completed)' in native and 'm_ui.reportOpen=true' in native),
        ('one-shot receipts survive intervening settings work', 'TakeCompleted(m_pendingRequest,receipt)' in native and 'while(Completed.size()>16)' in mailbox),
        ('player picker refreshes the live roster', 'QuickPlayers' in native and 'Refreshed {} currently available authoritative player(s).' in tools),
        ('new requests discard stale grant receipts', 'Result.erase("GrantResults")' in mailbox),
        ('single-flight protocol scoped to spawn tools only', 'template<> struct ToolRequest<SpawnToolTag>' in mailbox),
        ('mutations retain single-flight but index only acknowledges start', 'Pending||InFlight' in mailbox and 'static void CatalogProgress(' in mailbox and 'PS::SpawnToolRequests::Publish(std::move(result));' in index and 'if(g_quickCatalogIndex.Tick())return;' not in tools),
        ('incremental index is on the game-thread pump', 'g_quickCatalogIndex.Tick()' in tools and 'PumpSpawnTools();' in loader),
        ('catalog epoch invalidates cancelled worlds', 'generation!=PS::SpawnToolRequests::Generation.load()' in index and 'PS::WeakObjectHandle world' in index),
        ('registry scanner uses bounded work', 'GetAllAssets(assets,true)' in index and 'while(attempted++<4)' in index and 'milliseconds(2)' in index),
        ('missing metadata uses candidate fallback and exposes uncertainty', 'BlueprintCandidate(name,type)' in index and 'MetadataUnavailable' in index and 'UnclassifiedRecords' in index),
        ('live catalogs include running definitions and the item roster', 'm_eventTemplates' in tools and 'm_spawns' in tools and 'HelpyDefinitions()' in tools and 'PS::AssetSearch::ItemRoster()' in tools),
        ('F2 catalog refresh is routed before optional saved authoring catalogs', tools.index('if(action=="QuickCatalog"||') < tools.index('ToolCatalog(catalogDirectory/"Helpy-catalog-ai.json"')),
        ('missing building catalog is optional', 'is_regular_file(catalogDirectory/"Helpy-catalog-buildings.json")' in tools),
        ('world-scoped authoritative controller lookup retained', 'request->value("World",std::string{})' in tools and 'if(!GetGameMode(controller->GetWorld()))continue;' in tools),
        ('batch gives reuse inventory-checked native backend', 'PS::QuickUI::ExecuteBatch' in tools and 'GiveToolItem(selected,grant.item,grant.count)' in tools),
        ('existing server authority guard retained', 'Only the authoritative server may run spawn tools' in tools or 'if(!GetGameMode(world))' in tools),
        ('unknown exceptions complete mailbox with an error', 'The runtime action stopped after an unknown error' in tools),
        ('additional loot is additive, not base table replacement', 'AppendAdditionalDrops' in encoder and 'additional.insert(additional.end(),extra.begin(),extra.end())' in tools),
        ('spawn safety limits and save exclusion retained', '100 tool actors reached' in tools and 'bSkipSpudStore' in tools and 'LineTraceGround' in tools),
        ('settings render rows rather than icon tiles', 'ImGuiListClipper' in settings and 'ImGui::Image' not in settings and 'ItemIconCache' not in settings),
        ('long setting paths are rejected rather than truncated', 'path.size()<selectedCapacity' in settings),
        ('registry rules header is included at global scope', loader.index('#include "Generator/QuickMenuCatalogRules.h"') < loader.index('namespace DragonWilds {') and '#include "Generator/QuickMenuCatalogRules.h"' not in index),
        ('opening Helpy does not trigger an index; full registry scans are explicit', 'm_catalogAttempted' not in native and 'if(fullScan || updateReference)' in index and '(void)LoadSettings(full)' in index and 'if(diagnosticCatalogs)' in index and 'MetadataVersion' in index),
        ('native submit uses tested encoder and same Spawn handler', 'QuickUI::EncodeSpawn(command' in native and 'if(action!="Spawn")' in tools and 'set("Action",std::string("Spawn"))' in encoder),
        ('discovered class descriptors persist in the catalog snapshot', 'described[key]=' in index and 'Describe(type);' in index and 'Definitions(nlohmann::json::array())' in index),
        ('class discovery does not require save-exclusion metadata', 'SpudGuid' not in index and 'bSkipSpudStore' not in index),
        ('ore and stone have explicit metadata-independent candidates', 'bp_orenode_' in rules and 'bp_miningrock_' in rules and 'MiningName(name)' in rules),
        ('spawn receipts are retained and consumed visibly', 'receipt["SpawnResult"]' in mailbox and 'ShowActionResult(ok?' in native and 'result["SpawnResult"]={{"State","Completed"}' in tools),
        ('spawn submit/failure logs have correlated request IDs', 'spawn #{} queued:' in native and 'spawn #{} completed:' in tools and 'spawn #{} stopped [{}]:' in tools),
        ('completed scan does not publish a mutation receipt', 'PS::SpawnToolRequests::Publish' not in index[index.index('bool Tick()'):]),
        ('catalog updates do not alter command ownership', all(token not in mailbox[mailbox.index('static void CatalogProgress('):mailbox.index('static nlohmann::json Read()',mailbox.index('static void CatalogProgress('))] for token in ['InFlight','ActiveRequest','Completed','Waiting','_RequestId'])),
        ('live index snapshots do not mutate saved settings catalogs', 'QuickCatalogData[key]=snapshot[key]' in mailbox and 'ReadQuickIfChanged(m_catalogRevisions[category],category,result)' in native),
        ('plugin catalog transfer is scoped to the selected tab', 'RemoteReadQuick(revision,category,out)' in mailbox and '"_Category"' in native),
        ('catalog exceptions do not abort shared command pump', 'void Stop(const std::string& reason) noexcept' in index and 'Stop("Index stopped after an unexpected error")' in index),
    ]
    for name, passed in checks:
        if not passed:
            raise AssertionError(name)
    print(f'PASS: {len(checks)} native source-wiring checks (not a Windows/Unreal build or runtime test).')


if __name__ == '__main__':
    main()
