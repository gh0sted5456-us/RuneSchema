#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

static std::string Read(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) std::exit(2);
    std::ostringstream value;
    value << file.rdbuf();
    return value.str();
}

static void Check(bool value, const char* message)
{
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

int main(int argc, char** argv)
{
    Check(argc == 8, "seven source inputs supplied");
    const auto manager = Read(argv[1]);
    const auto loader = Read(argv[2]);
    const auto loadOrder = Read(argv[3]);
    const auto host = Read(argv[4]);
    const auto storefront = Read(argv[5]);
    const auto offsets = Read(argv[6]);
    const auto entrypoint = Read(argv[7]);
    Check(manager.find("ResolveUObjectVirtual") != std::string::npos, "vtable provider exists");
    Check(manager.find("UObject::VTableLayoutMap") != std::string::npos, "UE4SS vtable metadata used");
    Check(manager.find("VirtualQuery") != std::string::npos, "resolved target memory validated");
    Check(manager.find("live-vtable:") != std::string::npos, "provider provenance retained");
    Check(loader.find("Serialize__Ref_FArchive") != std::string::npos, "FArchive overload selected");
    Check(loader.find("if (DatatableSerializeCallbacks.empty())") != std::string::npos,
        "late Unreal-ready binding retry exists");
    Check(loader.find("normal GameInstance loading continues") != std::string::npos,
        "missing early binding is feature-scoped");
    Check(loader.find("IsDebugLoggingEnabled") == std::string::npos,
        "advanced logging never gates loader startup or execution");
    const auto autoReloadSetup = loader.find("void DragonWildsMainLoader::SetupAutoReload()");
    const auto autoReloadGate = loader.find("IsAutoReloadEnabled()");
    Check(autoReloadSetup != std::string::npos && autoReloadGate != std::string::npos
        && autoReloadGate > autoReloadSetup,
        "auto reload setting gates only watcher setup, not initial loader startup");
    Check(loader.find("WarnAboutUnknownFolders") == std::string::npos,
        "unknown mod-manager folders are not linted during normal mod loading");
    Check(loader.find("[CORE][READY] RuneSchema loaders initialized.") != std::string::npos,
        "core readiness is visible with standard logging");
    Check(loader.find("Unable to initialize RuneSchema core, signature for UDataTable::Serialize") == std::string::npos,
        "obsolete fatal diagnostic removed");
    Check(loadOrder.find("ModOrderState::WriteExpected(path,baseline,contents)") != std::string::npos,
        "load-order writes use the snapshot-checked atomic configuration writer");
    Check(host.find("runeschema.bindings") != std::string::npos, "binding discovery service registered");
    Check(host.find("binding.resolve") != std::string::npos, "binding capability registered");
    Check(host.find("ResolveBinding") != std::string::npos, "host binding resolver exported");
    Check(manager.find("CurrentNativeLane") != std::string::npos,
        "embedded executable signatures are selected by the active native lane");
    Check(manager.find("[BINDING:{}][UNAVAILABLE]") != std::string::npos,
        "a missing optional executable pattern is reported as a scoped warning");
    Check(storefront.find("NativeLane::SteamNative") != std::string::npos
        && storefront.find("NativeLane::GamePassNative") != std::string::npos
        && storefront.find("NativeLane::SharedOnly") != std::string::npos,
        "storefront selects one explicit native lane");
    Check(storefront.find("lane == NativeLane::SteamNative || lane == NativeLane::GamePassNative") != std::string::npos,
        "embedded AOBs are restricted to explicit storefront lanes");
    Check(manager.find("SteamSignatures") != std::string::npos
        && manager.find("GamePassSignatures") != std::string::npos,
        "Steam and WinGDK signatures are selected from isolated maps");
    for (const auto* oldGamePassFatal : {
            "Unable to initialize RuneSchema core, signature for UDataTable::Serialize is outdated",
            "Failed to find signature for FFieldClass::GetNameToFieldClassMap",
            "Failed to find signature for GetObjectsOfClass",
            "Failed to find signature for FName::ToString_Wchar"})
        Check(loader.find(oldGamePassFatal) == std::string::npos
                && manager.find(oldGamePassFatal) == std::string::npos,
            "0.7.5 Game Pass AOB failure cannot return");
    Check(offsets.find("if (FNameConstructorAddress)") != std::string::npos
        && offsets.find("if (FNameToStringAddress)") != std::string::npos,
        "missing storefront override preserves UE4SS host bindings");
    Check(entrypoint.find("Native binding lane:") != std::string::npos,
        "selected lane is announced for diagnostics");
    Check(storefront.find("DedicatedServer") != std::string::npos,
        "dedicated server executable detection is explicit");
    Check(entrypoint.find("[SERVER][SAFE-MODE]") != std::string::npos
        && entrypoint.find("Desktop Helpy is allowed; other client plugins remain suppressed") != std::string::npos
        && host.find("if(Storefront::IsDedicatedServer()&&manifest.Id!=\"RuneSchema.Helpy\")continue;") != std::string::npos
        && entrypoint.find("if (m_startupFailed.load(std::memory_order_acquire)) return;") != std::string::npos
        && entrypoint.find("if (!PS::Storefront::IsDedicatedServer())") != std::string::npos
        && entrypoint.find("m_pluginHost.OnUiInit();") != std::string::npos
        && entrypoint.find("register_tab(STR(\"RuneSchema\")") != std::string::npos
        && entrypoint.find("m_pluginHost.OnUnrealInit();") != std::string::npos,
        "dedicated server permits only standalone Helpy while keeping other client plugins suppressed");
    const auto rawRoot=std::filesystem::weakly_canonical(argv[7]).parent_path().parent_path();
    const auto desktopRoot=rawRoot.parent_path()/"helpy"/"source";
    const auto navigation=Read(desktopRoot/"include/Core/HelpyNavigation.h");
    const auto desktop=Read(desktopRoot/"src/Generator/HelpyStandaloneWindow.cpp");
    const auto panel=Read(desktopRoot/"include/Generator/AuthoringPanel.h");
    const auto protocol=Read(rawRoot/"include/Core/HelpyAuthoringProtocol.h");
    Check(entrypoint.find("BeginRuneSchemaTab(\"Authoring & Tools\"")==std::string::npos
        && navigation.find("case Page::Authoring:return \"Authoring Tools\";")!=std::string::npos
        && desktop.find("case Page::Authoring:")!=std::string::npos
        && desktop.find("AuthoringPanel::Render();")!=std::string::npos,
        "desktop Authoring Tools replaces the duplicate legacy authoring tab");
    const auto serverStart=desktop.find("if(HelpyEnvironment::Server())",desktop.find("void HelpyStandaloneWindow::ThreadMain()"));
    Check(serverStart!=std::string::npos
        && desktop.find("CurrentPage=Page::Authoring;",serverStart)!=std::string::npos
        && desktop.find("ShowWindow(m_hwnd,SW_SHOWNOACTIVATE)",serverStart)!=std::string::npos,
        "dedicated server opens the desktop authoring route without a local-player dependency");
    for(const auto* operation:{"AuthoringActivate","AuthoringSubmit","AuthoringSnapshot"})
        Check(host.find(operation)!=std::string::npos,"native host exposes all desktop authoring service routes");
    const auto bridgeStart=entrypoint.find("void PrepareAuthoringBridge()");
    Check(bridgeStart!=std::string::npos,"native authoring bridge is initialized");
    const auto bridge=entrypoint.substr(bridgeStart,entrypoint.find("\nprivate:",bridgeStart)-bridgeStart);
    Check(bridge.find("IsDedicatedServer")==std::string::npos
        && bridge.find("Bridge.Configure(settings.authoringTools,false)")!=std::string::npos
        && bridge.find("if (!settings.authoringTools || m_apiExportCallbackId != Hook::ERROR_ID) return;")!=std::string::npos
        && bridge.find("RegisterEngineTickPostCallback")!=std::string::npos
        && bridge.find("PS::InspectionTools::Tick();")!=std::string::npos
        && bridge.find("if(!PS::HelpyAuthoring::Bridge.TickActivation())return;")<bridge.find("PS::InspectionTools::Tick();")
        && bridge.find("Bridge.Configure(settings.authoringTools,m_apiExportCallbackId!=Hook::ERROR_ID)")!=std::string::npos
        && bridge.find("Bridge.WorldChanged();")!=std::string::npos,
        "dedicated-server backend preserves config, ready-hook, game-thread activation and world-reset gates");
    Check(entrypoint.find("PrepareAuthoringBridge();")<entrypoint.find("m_pluginHost.OnUnrealInit();")
        && protocol.find("if(!enabled||!ready)")!=std::string::npos
        && protocol.find("if(activation&&enabled&&ready)")!=std::string::npos
        && protocol.find("if(!enabled||!ready||!active)")!=std::string::npos
        && panel.find("AuthoringClient::Activate()")!=std::string::npos,
        "desktop uses explicit session activation after native readiness; requests cannot bypass it");
    Check(panel.find("ImGui::Combo(\"Workflow\"")!=std::string::npos
        && panel.find("Export all loader schemas")!=std::string::npos
        && panel.find("SchemaExport")!=std::string::npos,
        "desktop authoring keeps workflow navigation and full schema export");
    Check(entrypoint.find("DllMain") == std::string::npos
        && entrypoint.find("return new RuneSchema();") != std::string::npos,
        "RuneSchema performs startup only when UE4SS calls start_mod (mods.txt authority)");
    Check(entrypoint.find("[RuneSchema][DID-NOT-START][CONSTRUCTION]") != std::string::npos
        && entrypoint.find("MainLoader.AbortStartup(\"unreal-init\"") != std::string::npos,
        "fatal construction and Unreal lifecycle failures are not annotated");
    Check(loader.find("[RuneSchema][DID-NOT-START][CORE]") != std::string::npos
        && loader.find("All RuneSchema runtime hooks and services were stopped") != std::string::npos
        && loader.find("[DEGRADED][SERVICE:data-registrar]") != std::string::npos
        && loader.find("Dedicated-server building cleanup will still check its live registries independently") != std::string::npos
        && loader.find("if (PS::Storefront::IsDedicatedServer()") != std::string::npos,
        "fatal core shutdown or degraded persistence startup is not correctly classified");
    Check(loader.find("[SERVER][REGISTRIES-READY]") != std::string::npos,
        "dedicated server registry readiness is announced explicitly");
    Check(loader.find("DedicatedServerRegistryReady") != std::string::npos
        && loader.find("[SERVER][REGISTRY-DEFERRED]") != std::string::npos
        && loader.find("m_dedicatedServerWorldReady.load") != std::string::npos,
        "dedicated server cloning and registration wait for InitGameState readiness");
    std::cout << "Native binding resolution contract passed.\n";
}
