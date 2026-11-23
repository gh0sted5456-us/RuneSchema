#include "Runtime/PluginCatalog.h"
#include "Runtime/PluginActivation.h"
#include "Runtime/PluginEvents.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

namespace fs = std::filesystem;

static void Check(bool value, const char* message)
{
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

static void Write(const fs::path& path, const std::string& value = {})
{
    fs::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    Check(static_cast<bool>(stream), "test file opens");
    stream << value;
}

static void AddPakTriplet(const fs::path& plugin)
{
    const auto package = plugin / "paks" / "CompatibilityContent";
    Write(package / "CompatibilityContent.pak");
    Write(package / "CompatibilityContent.ucas");
    Write(package / "CompatibilityContent.utoc");
}

int main() try
{
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("runeschema-plugin-contract-" + std::to_string(nonce));
    struct Cleanup {
        fs::path Path;
        ~Cleanup() { std::error_code error; fs::remove_all(Path, error); }
    } cleanup{root};

    const auto alpha = root / "Alpha";
    const auto beta = root / "Beta";
    Write(alpha / "plugin.json", R"({
        "SchemaVersion": 1,
        "Id": "Alpha",
        "Name": "Alpha",
        "Version": "99.0.0",
        "ApiVersion": 999,
        "BuiltForRuneSchema": "0.1.0",
        "EntryPoint": "missing-alpha.dll",
        "MountPaks": false,
        "Dependencies": { "Beta": "1.0.0" }
    })");
    Write(beta / "plugin.json", R"({
        "SchemaVersion": 1,
        "Id": "Beta",
        "Name": "Beta",
        "Version": "2.0.0",
        "ApiVersion": 1,
        "Dependencies": { "Alpha": "1.0.0", "NotInstalled": "*" }
    })");
    AddPakTriplet(alpha);
    AddPakTriplet(beta);
    Write(alpha / "paks" / "__folder_managed_by_vortex");
    Write(alpha / "paks" / "README.txt", "ignored");
    Write(alpha / "paks" / "CompatibilityContent" / "meta.ini", "ignored");
    Write(root / "random-nexus-file.txt", "ignored");
    Write(root / "UnrelatedFolder" / "readme.md", "ignored");
    Write(root / "plugins.txt", "Alpha:1\nBeta:1\n");

    std::vector<std::string> diagnostics;
    const auto plugins = PS::PluginCatalog::Discover(root, &diagnostics);
    Check(plugins.size() == 2, "mismatched and cyclic plugins remain discoverable");
    Check(!plugins[0].EntryPoint.empty() || !plugins[1].EntryPoint.empty(),
        "missing optional native entry point remains represented");
    Check(diagnostics.empty() || std::none_of(diagnostics.begin(), diagnostics.end(), [](const auto& line) {
        return line.find("vortex") != std::string::npos || line.find("README.txt") != std::string::npos
            || line.find("random-nexus-file") != std::string::npos || line.find("UnrelatedFolder") != std::string::npos;
    }), "mod-manager debris is silently ignored");
    for (const auto& plugin : plugins) {
        const auto packs=PS::PluginCatalog::PakDirectories(plugin);
        if(plugin.Id=="Alpha") Check(packs.empty(),"MountPaks false ignores stale native-plugin package content");
        else Check(packs.size()==1,"complete PAK triplet remains independently mountable");
    }
    {
        PS::PluginCatalog::Plugin linked;linked.Id="VortexLinked";linked.PaksRoot=root/"vortex"/"paks";
        const auto stage=root/"staging",package=linked.PaksRoot/"RegistryBridge";
        fs::create_directories(package);
        for(const auto& name:{"RegistryBridge_P.PAK","registrybridge_p.ucas","RegistryBridge_P.UTOC"}){
            Write(stage/name,"fixture");fs::create_hard_link(stage/name,package/name);
        }
        Write(linked.PaksRoot/"metadata-only"/"__folder_managed_by_vortex","ignored");
        Check(PS::PluginCatalog::PakDirectories(linked).size()==1,"mixed-case hardlinked triplet accepts Windows filename semantics");
        fs::remove(package/"registrybridge_p.ucas");bool rejected=false;
        try{PS::PluginCatalog::PakDirectories(linked);}catch(const std::exception& error){
            const std::string message=error.what();rejected=message.find("missing")!=message.npos&&message.find(".ucas")!=message.npos&&message.find("found")!=message.npos;
        }
        Check(rejected,"missing hardlink is rejected with the exact absent part and observed filenames");
        fs::create_hard_link(stage/"registrybridge_p.ucas",package/"registrybridge_p.ucas");
        Check(PS::PluginCatalog::PakDirectories(linked).size()==1,"redeployed complete triplet recovers without renaming staging files");
    }

    bool missingDependency = false;
    bool cycle = false;
    for (const auto& diagnostic : diagnostics) {
        missingDependency = missingDependency || diagnostic.find("dependency NotInstalled is unavailable") != std::string::npos;
        cycle = cycle || diagnostic.find("dependency cycle detected") != std::string::npos;
    }
    Check(missingDependency, "missing dependency is diagnosed without suppressing discovery");
    Check(cycle, "dependency cycle falls back to deterministic best-effort order");

    // plugins.txt is the explicit preferred order. Dependencies may move a
    // provider earlier, but otherwise the file order must be preserved.
    const auto orderRoot=root/"order";
    for(const auto* id:{"AlphaOrder","BetaOrder","GammaOrder"}) {
        const auto folder=orderRoot/id;
        Write(folder/"plugin.json",
            std::string("{\"SchemaVersion\":1,\"Id\":\"")+id+"\",\"Version\":\"1.0.0\"}");
    }
    Write(orderRoot/"GammaOrder/plugin.json",std::string(300*1024,' ')+R"({"Id":"GammaOrder","Version":"1.0.0"})");
    Write(orderRoot/"plugins.txt","\xEF\xBB\xBF" "GammaOrder:1\n#"+std::string(300*1024,' ')
        +"\ninvalid entry\nAlphaOrder:1\nBetaOrder:1\n");
    std::vector<std::string> orderDiagnostics;
    const auto ordered=PS::PluginCatalog::Discover(orderRoot,&orderDiagnostics);
    Check(orderDiagnostics.size()==1&&orderDiagnostics.front().find("line 3")!=std::string::npos,
        "only the malformed order line is ignored and reported");
    Check(ordered.size()==3,"ordered plugin fixture is complete");
    Check(ordered[0].Id=="GammaOrder"&&ordered[1].Id=="AlphaOrder"&&ordered[2].Id=="BetaOrder",
        "plugins.txt top-to-bottom order is preserved when dependencies do not constrain it");

    const auto dependencyRoot=root/"dependency-order";
    Write(dependencyRoot/"Consumer"/"plugin.json",
        R"({"SchemaVersion":1,"Id":"Consumer","Version":"1.0.0","Dependencies":{"Provider":"1.0.0"}})");
    Write(dependencyRoot/"Provider"/"plugin.json",
        R"({"SchemaVersion":1,"Id":"Provider","Version":"1.0.0"})");
    Write(dependencyRoot/"plugins.txt","Consumer:1\nProvider:1\n");
    const auto dependencyOrdered=PS::PluginCatalog::Discover(dependencyRoot);
    Check(dependencyOrdered.size()==2,"dependency-order plugin fixture is complete");
    Check(dependencyOrdered[0].Id=="Provider"&&dependencyOrdered[1].Id=="Consumer",
        "plugin dependencies are the only supported reason to override preferred plugins.txt order");

    const auto migrationRoot=root/"migration";
    const auto legacy=migrationRoot/"RuneSchema.Networking";
    const auto renamed=migrationRoot/"RSNetworking";
    Write(legacy/"plugin.json",R"({"SchemaVersion":1,"Id":"RuneSchema.Networking","Version":"0.7.8"})");
    Write(renamed/"plugin.json",R"({"SchemaVersion":1,"Id":"RSNetworking","Version":"0.7.9"})");
    AddPakTriplet(legacy);AddPakTriplet(renamed);
    Write(migrationRoot/"plugins.txt","RuneSchema.Networking:0\nRSNetworking:1\n");
    const auto migrated=PS::PluginCatalog::Discover(migrationRoot);
    Check(migrated.size()==1&&migrated[0].Id=="RSNetworking","renamed networking plugin suppresses leftover legacy folder");
    Check(migrated[0].Enabled,"new RSNetworking order entry wins after legacy alias normalization");
    const auto sharedRoot=root/"shared-provider",network=sharedRoot/"RSNetworking",shared=sharedRoot/"RuneSchema.RegistryBridge";
    Write(network/"plugin.json",R"({"SchemaVersion":99,"Id":"RSNetworking","Version":"0.8.0.3"})");
    Write(shared/"plugin.json",R"({"Id":"RuneSchema.RegistryBridge","Version":"0.8.0.3"})");
    AddPakTriplet(network);AddPakTriplet(shared);
    Write(network/"paks"/"RegistryBridge"/"RegistryBridge_P.pak","obsolete partial copy");
    const auto preferred=PS::PluginCatalog::Discover(sharedRoot);
    const auto current=std::find_if(preferred.begin(),preferred.end(),[](const auto& plugin){return plugin.Id=="RSNetworking";});
    Check(current!=preferred.end()&&current->PreferSharedRegistry&&PS::PluginCatalog::PakDirectories(*current).size()==1,"valid shared provider supersedes an incomplete retired networking copy");
    Write(sharedRoot/"plugins.txt","RuneSchema.RegistryBridge:0\nRSNetworking:1\n");
    const auto fallback=PS::PluginCatalog::Discover(sharedRoot);
    const auto legacyFallback=std::find_if(fallback.begin(),fallback.end(),[](const auto& plugin){return plugin.Id=="RSNetworking";});
    Check(legacyFallback!=fallback.end()&&!legacyFallback->PreferSharedRegistry,"disabled shared provider retains legacy validation");

    const auto modern=root/"modern";
    Write(modern/"Helpy/plugins/GrandExchange/plugin.json",R"({"Id":"GrandExchange","Version":"0.2.6","Host":"helpy","EntryPoint":"GrandExchange.Helpy.dll","MountPaks":false})");
    Write(modern/"Helpy/plugins/GrandExchange/dll/GrandExchange.Helpy.dll");
    Write(modern/"Helpy/plugin.json",R"({"Id":"RuneSchema.Helpy","Version":"0.8.1.0","EntryPoint":"Helpy.dll"})");
    Write(modern/"Helpy/plugins/RuneSchema.HUD/plugin.json",R"({"Id":"RuneSchema.HUD","Version":"0.8.1.0","EntryPoint":"RuneSchema.HUD.dll"})");
    Write(modern/"dlls/plugins/RSNetworking/plugin.json",R"({"Id":"RSNetworking","Version":"0.8.1.0"})");
    Write(modern/"plugins/RuneSchema.Helpy/plugin.json",R"({"Id":"RuneSchema.Helpy","Version":"0.8.0.3"})");
    Write(modern/"plugins/ThirdParty/plugin.json",R"({"Id":"ThirdParty","Version":"1.0.0"})");
    Write(modern/"settings/plugins.txt","GrandExchange:1\nRuneSchema.HUD:0\nRSNetworking:1\n");
    Write(modern/"Helpy/plugins/RuneSchema.ModMenu2/plugin.json",R"({"Id":"RuneSchema.ModMenu2","Version":"0.8.1.0","Host":"helpy","EntryPoint":"RuneSchema.ModMenu2.dll"})");
    Write(modern/"Helpy/plugins/InvalidHost/plugin.json",R"({"Id":"InvalidHost","Version":"1","Host":"unknown"})");
    const auto combined=PS::PluginCatalog::DiscoverRuntime(modern,&diagnostics);
    Check(combined.size()==5,"canonical/legacy roots combine once, including Grand Exchange, and canonical order disables HUD");
    const auto app=std::find_if(combined.begin(),combined.end(),[](const auto& p){return p.Id=="RuneSchema.Helpy";});
    Check(app!=combined.end()&&app->Version=="0.8.1.0"&&app->EntryPoint==modern/"Helpy/Helpy.dll","canonical Helpy wins over old manifest and loads flat binary");
    Check(std::any_of(combined.begin(),combined.end(),[](const auto& p){return p.Id=="RuneSchema.ModMenu2"&&p.Host=="helpy";}),"frontend host routing is explicit");
    Check(std::any_of(combined.begin(),combined.end(),[](const auto& p){return p.Id=="ThirdParty";}),"legacy third-party plugin stays discoverable");
    const auto owned=modern/"mods/Example/plugins/Example.Native";
    Write(owned/"plugin.json",R"({"Id":"Example.Native","Version":"1","Host":"game","EntryPoint":"Example.dll"})");
    Write(owned/"dll/Example.dll");
    Write(modern/"mods/Disabled/plugins/Disabled.Native/plugin.json",R"({"Id":"Disabled.Native","Version":"1","EntryPoint":"Disabled.dll"})");
    Write(modern/"mods/Example/ue4ss/Unrelated/dlls/main.dll");
    const auto exchange=PS::PluginCatalog::DiscoverRuntime(modern,&diagnostics);
    const auto ge=std::find_if(exchange.begin(),exchange.end(),[](const auto& plugin){return plugin.Id=="GrandExchange";});
    Check(ge!=exchange.end()&&ge->Host=="helpy"&&ge->Enabled&&ge->EntryPoint==modern/"Helpy/plugins/GrandExchange/dll/GrandExchange.Helpy.dll"&&!ge->MountPaks,"Grand Exchange uses the owned Helpy child DLL and plugin order");
    const auto native=PS::PluginCatalog::DiscoverOwned(modern,{modern/"mods/Example"},&diagnostics);
    Check(native.size()==1&&native[0].ModOwner=="Example"&&native[0].Id=="Example.Native"&&!native[0].MountPaks,"native plugin binds its actual enabled parent and never mounts a second PAK copy");
    Check(PS::PluginCatalog::DiscoverOwned(modern,{},&diagnostics).empty(),"disabled parent cannot load a native plugin");
    Check(PS::PluginCatalog::DiscoverOwned(modern,{root/"elsewhere/Example"},&diagnostics).empty(),"nonruntime owner root rejected");
    Write(owned/"plugin.json",R"({"Id":"Other.Native","Version":"1","EntryPoint":"Example.dll"})");
    Check(PS::PluginCatalog::DiscoverOwned(modern,{modern/"mods/Example"},&diagnostics).empty(),"native plugin cannot claim another owner's namespace");
    // Dedicated Helpy plugin: one folder with a Helpy UI DLL, an in-game half and content.
    const auto unit=root/"Unit";
    Write(unit/"Helpy/plugins/Exchange/plugin.json",R"({"Id":"Exchange","Version":"1.0.0","Host":"helpy","EntryPoint":"Exchange.Helpy.dll",
        "GameEntryPoint":"Exchange.Game.dll","Capabilities":["helpy.nav-tab"],"GameCapabilities":["exchange.vault"],"Content":true})");
    Write(unit/"Helpy/plugins/Exchange/runeschema/raw/70-Loot.jsonc","{}");
    Write(unit/"Helpy/plugins/BadHost/plugin.json",R"({"Id":"BadHost","Version":"1","Host":"game","EntryPoint":"A.dll","GameEntryPoint":"B.dll"})");
    Write(unit/"Helpy/plugins/BadCaps/plugin.json",R"({"Id":"BadCaps","Version":"1","Host":"helpy","EntryPoint":"A.dll","GameCapabilities":["x"]})");
    Write(unit/"Helpy/plugins/SameDll/plugin.json",R"({"Id":"SameDll","Version":"1","Host":"helpy","EntryPoint":"A.dll","GameEntryPoint":"A.dll"})");
    Write(unit/"Helpy/plugins/NestedDll/plugin.json",R"({"Id":"NestedDll","Version":"1","Host":"helpy","EntryPoint":"A.dll","GameEntryPoint":"x/B.dll"})");
    Write(unit/"Helpy/plugins/GameContent/plugin.json",R"({"Id":"GameContent","Version":"1","EntryPoint":"A.dll","Content":true})");
    Write(unit/"Helpy/plugins/BadContent/plugin.json",R"({"Id":"BadContent","Version":"1","Host":"helpy","EntryPoint":"A.dll","Content":"yes"})");
    std::vector<std::string> unitNotes;
    auto units=PS::PluginCatalog::DiscoverRuntime(unit,&unitNotes);
    const auto exchangeUnit=std::find_if(units.begin(),units.end(),[](const auto& p){return p.Id=="Exchange";});
    Check(exchangeUnit!=units.end()&&exchangeUnit->Host=="helpy"&&exchangeUnit->EntryPoint==unit/"Helpy/plugins/Exchange/dll/Exchange.Helpy.dll"
        &&exchangeUnit->GameEntryPoint==unit/"Helpy/plugins/Exchange/dll/Exchange.Game.dll"&&exchangeUnit->GameCapabilities==std::vector<std::string>{"exchange.vault"}
        &&exchangeUnit->Capabilities==std::vector<std::string>{"helpy.nav-tab"}&&exchangeUnit->ContentRoot==unit/"Helpy/plugins/Exchange/runeschema"&&exchangeUnit->MountPaks,
        "Helpy plugin declares its in-game half, game capabilities and content root in one folder");
    for(const auto* rejected:{"BadHost","BadCaps","SameDll","NestedDll","GameContent","BadContent"})
        Check(std::none_of(units.begin(),units.end(),[&](const auto& p){return p.Id==rejected;})&&std::any_of(unitNotes.begin(),unitNotes.end(),[&](const auto& note){return note.find(std::string(rejected)+": rejected")==0;}),
            "invalid game-half or content declarations reject only that manifest");
    std::vector<std::string> contentNotes;
    auto roots=PS::PluginCatalog::ContentRoots(units,unit/"mods",&contentNotes);
    Check(roots.size()==1&&roots[0].Id=="Exchange"&&roots[0].Path==unit/"Helpy/plugins/Exchange/runeschema","enabled Helpy plugin content is enrolled under its Id");
    Write(unit/"mods/Exchange/ID.txt","ModID: Exchange");
    contentNotes.clear();
    Check(PS::PluginCatalog::ContentRoots(units,unit/"mods",&contentNotes).empty()&&!contentNotes.empty(),"an ordinary RuneSchema mod of the same name takes precedence");
    fs::remove_all(unit/"mods/Exchange");
    Write(unit/"Helpy/plugins/Exchange/runeschema/paks/Exchange/Exchange.pak");
    Check(PS::PluginCatalog::ContentRoots(units,unit/"mods").empty(),"PAKs belong in the plugin paks folder, not runeschema/paks");
    fs::remove_all(unit/"Helpy/plugins/Exchange/runeschema/paks");
    auto disabled=units;for(auto& plugin:disabled)plugin.Enabled=false;
    Check(PS::PluginCatalog::ContentRoots(disabled,unit/"mods").empty(),"a disabled plugin contributes no content");
    fs::remove_all(unit/"Helpy/plugins/Exchange/runeschema");
    Check(PS::PluginCatalog::ContentRoots(units,unit/"mods").empty(),"a declared but missing runeschema folder is skipped");
    Check(PS::PluginCatalog::OwnedServiceAllowed("Exchange","exchange.vault","Exchange"),"a frontend plugin reaches its own in-game service");
    Check(!PS::PluginCatalog::OwnedServiceAllowed("Exchange","other.service","Other"),"a frontend plugin cannot reach another plugin's service");
    Check(!PS::PluginCatalog::OwnedServiceAllowed("RuneSchema.Helpy","runeschema.tools","RuneSchema.Helpy")&&!PS::PluginCatalog::OwnedServiceAllowed("RuneSchema.Core","x","RuneSchema.Core"),"core and Helpy never use the owned-service route");
    Check(!PS::PluginCatalog::OwnedServiceAllowed("bad id","exchange.vault","bad id"),"caller identity must be a token");
    // Interrupted whole-plugin activation: recover before discovery, never delete.
    const auto act=root/"Activation";
    const auto updates=act/"Helpy/plugin-updates/Exchange";
    const auto manifest=[](const std::string& version){return std::string(R"({"Id":"Exchange","Version":")")+version+R"(","Host":"helpy","EntryPoint":"E.dll"})";};
    const auto record=[&](const std::string& state){Write(updates/"activation.json",std::string(R"({"Schema":"runeschema.plugin-activation/1","Id":"Exchange","State":")")+state+R"(","Staged":"staging","Previous":"previous","Version":"2.0.0","PreviousVersion":"1.0.0"})");};
    const auto stateOf=[&]{std::ifstream stream(updates/"activation.json");nlohmann::json data;stream>>data;return data.value("State",std::string{});};
    fs::create_directories(act/"Helpy/plugins");
    Write(updates/"staging/plugin.json",manifest("2.0.0"));Write(updates/"previous/plugin.json",manifest("1.0.0"));record("Swapping");
    auto notes=PS::PluginActivation::Recover(act);
    Check(notes.size()==1&&fs::is_regular_file(act/"Helpy/plugins/Exchange/plugin.json")&&!fs::exists(updates/"staging")&&fs::exists(updates/"previous")&&stateOf()=="RecoveredForward",
        "power loss between renames rolls forward to the verified staged generation");
    Check(PS::PluginActivation::Recover(act).empty()&&stateOf()=="RecoveredForward","recovery is idempotent");
    fs::remove_all(act/"Helpy/plugins/Exchange");Write(updates/"staging/plugin.json",manifest("1.9.9"));record("Swapping");
    notes=PS::PluginActivation::Recover(act);
    Check(notes.size()==1&&fs::exists(act/"Helpy/plugins/Exchange/plugin.json")&&fs::exists(updates/"staging")&&!fs::exists(updates/"previous")&&stateOf()=="RecoveredBack",
        "a staged folder that does not match the record rolls back to the previous generation");
    fs::remove_all(act/"Helpy/plugins/Exchange");Write(updates/"previous/plugin.json",manifest("0.1"));record("Swapping");
    notes=PS::PluginActivation::Recover(act);
    Check(notes.size()==1&&!fs::exists(act/"Helpy/plugins/Exchange")&&fs::exists(updates/"staging")&&fs::exists(updates/"previous")&&stateOf()=="Swapping",
        "with no matching generation nothing moves and the plugin stays absent");
    Write(act/"Helpy/plugins/Exchange/plugin.json",manifest("1.0.0"));Write(updates/"staging/plugin.json",manifest("2.0.0"));
    Check(PS::PluginActivation::Recover(act).empty()&&fs::exists(updates/"staging")&&stateOf()=="Swapping","a present live folder is left for the activator");
    fs::remove_all(act/"Helpy/plugins/Exchange");record("Activated");
    Check(PS::PluginActivation::Recover(act).empty()&&!fs::exists(act/"Helpy/plugins/Exchange"),"only a Swapping record is acted on");
    Write(updates/"activation.json",R"({"Schema":"runeschema.plugin-activation/1","Id":"Exchange","State":"Swapping","Staged":"../x","Previous":"previous","Version":"2.0.0","PreviousVersion":"1.0.0"})");
    notes=PS::PluginActivation::Recover(act);
    Check(notes.size()==1&&!fs::exists(act/"Helpy/plugins/Exchange"),"path-like folder names in the record are refused");
    // E8 owned plugin events: shape, owner-only Open, bounded payload, reserved service, rate.
    namespace E=PS::PluginEvents;
    auto built=E::Build("GrandExchange",R"({"Event":"VaultOpened","Payload":{"CharacterGuid":"g","Revision":7},"Open":"GrandExchange.exchange"})",3);
    Check(built["Kind"]=="plugin-event"&&built["Owner"]=="GrandExchange"&&built["Event"]=="VaultOpened"&&built["Sequence"]==3
        &&built["Payload"]["Revision"]==7&&built["Open"]=="GrandExchange.exchange"&&E::Deliverable(built),"a valid event is tagged with its caller");
    Check(E::Build("GrandExchange",R"({"Event":"InventoryChanged"})",4)["Payload"].is_object(),"Payload defaults to an empty object");
    const auto refused=[&](const std::string& request){try{(void)E::Build("GrandExchange",request,1);return false;}catch(const std::exception&){return true;}};
    Check(refused(R"({"Event":"VaultOpened","Open":"OtherMod.page"})")&&refused(R"({"Event":"VaultOpened","Open":"GrandExchange."})")
        &&refused(R"({"Event":"VaultOpened","Open":"GrandExchange.../x"})"),"Open must name one of the caller's own routes");
    Check(refused(R"({"Payload":{}})")&&refused(R"({"Event":"bad event"})")&&refused(R"([])")&&refused(R"({"Event":"X","Payload":[1]})"),"malformed events are refused");
    Check(refused(std::string(R"({"Event":"X","Payload":{"Blob":")")+std::string(17*1024,'a')+R"("}})"),"payloads over 16 KiB are refused");
    auto spoof=built;spoof["Owner"]="RuneSchema.Helpy";
    auto foreign=built;foreign["Open"]="OtherMod.page";
    Check(!E::Deliverable(spoof)&&!E::Deliverable(foreign)&&!E::Deliverable(nlohmann::json{{"Kind","other"}}),"Helpy refuses spoofed owners and foreign routes");
    Check(E::ServiceNameAllowed("GrandExchange","GrandExchange.events")&&!E::ServiceNameAllowed("OtherMod","GrandExchange.events")&&E::ServiceNameAllowed("OtherMod","OtherMod.vault"),
        "<Id>.events is reserved for its owner");
    E::Bucket bucket;int accepted=0;for(int i=0;i<40;++i)accepted+=E::Take(bucket,1000)?1:0;
    Check(accepted==16&&bucket.Sequence==16,"a burst is capped per owner and refused without blocking");
    Check(!E::Take(bucket,1050)&&E::Take(bucket,1200)&&bucket.Sequence==17,"tokens refill at the documented rate");
    // Fail closed: when the update lock was unavailable, Helpy/plugins is never scanned.
    PS::PluginCatalog::UpdatablePluginsBlocked().store(true);
    std::vector<std::string> blockedNotes;
    const auto blocked=PS::PluginCatalog::DiscoverRuntime(modern,&blockedNotes);
    PS::PluginCatalog::UpdatablePluginsBlocked().store(false);
    Check(std::none_of(blocked.begin(),blocked.end(),[](const auto& p){return p.Id=="GrandExchange"||p.Id=="RuneSchema.ModMenu2";})
        &&std::any_of(blocked.begin(),blocked.end(),[](const auto& p){return p.Id=="RuneSchema.Helpy";})
        &&std::any_of(blocked.begin(),blocked.end(),[](const auto& p){return p.Id=="RSNetworking";})
        &&std::any_of(blocked.begin(),blocked.end(),[](const auto& p){return p.Id=="ThirdParty";})
        &&std::any_of(blockedNotes.begin(),blockedNotes.end(),[](const auto& note){return note.starts_with("Helpy/plugins: skipped");}),
        "a blocked startup skips only the externally updated Helpy/plugins folders");
#ifdef _WIN32
    {
        // Activator vs host concurrent start, with a private lock name.
        const std::wstring lockName=L"Local\\RuneSchema.PluginActivation.Test."+std::to_wstring(GetCurrentProcessId());
        const auto race=root/"Race";
        const auto raceUpdates=race/"Helpy/plugin-updates/Exchange";
        fs::create_directories(race/"Helpy/plugins");
        Write(raceUpdates/"2.0.0/plugin.json",R"({"Id":"Exchange","Version":"2.0.0","Host":"helpy","EntryPoint":"E.dll"})");
        HANDLE ready=CreateEventW(nullptr,TRUE,FALSE,nullptr),release=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        std::thread activator([&]{
            const auto gate=CreateMutexW(nullptr,FALSE,lockName.c_str());
            WaitForSingleObject(gate,INFINITE);SetEvent(ready);
            WaitForSingleObject(release,INFINITE);
            fs::rename(raceUpdates/"2.0.0",race/"Helpy/plugins/Exchange"); // the swap finishes while guarded
            ReleaseMutex(gate);CloseHandle(gate);
        });
        WaitForSingleObject(ready,INFINITE);
        {
            PS::PluginActivation::StartupLease early(race,200,lockName.c_str());
            Check(!early.Held()&&PS::PluginCatalog::UpdatablePluginsBlocked().load()&&!early.Notes().empty(),"a host that cannot take the lock fails closed");
        }
        PS::PluginCatalog::UpdatablePluginsBlocked().store(false);
        std::thread finish([&]{Sleep(300);SetEvent(release);});
        {
            PS::PluginActivation::StartupLease late(race,10000,lockName.c_str());
            Check(late.Held()&&!PS::PluginCatalog::UpdatablePluginsBlocked().load()&&fs::is_regular_file(race/"Helpy/plugins/Exchange/plugin.json"),
                "a host that waits starts only after the guarded swap completed");
            bool activatorExcluded=false;
            std::thread second([&]{const auto gate=CreateMutexW(nullptr,FALSE,lockName.c_str());activatorExcluded=WaitForSingleObject(gate,100)==WAIT_TIMEOUT;CloseHandle(gate);});
            second.join();
            Check(activatorExcluded,"an activator cannot take the lock while a host scans and loads plugins");
        }
        finish.join();activator.join();CloseHandle(ready);CloseHandle(release);
    }
#endif
    std::cout << "Plugin catalog compatibility contract passed.\n";
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
