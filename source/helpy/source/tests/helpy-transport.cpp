// Exercise production serialization, push decoding, and query state together.
// The fixture renderer alone cannot catch a lost host/plugin message.
#include "../plugins/helpy/HelpyPlugin.cpp"
#include "Generator/SpawnToolsPanel.h"
#include "Generator/PlayerInventoryPanel.h"
#include "Core/MapProjection.h"
#include "Core/MapSpawnRequest.h"
#include <cassert>
namespace {
using json=nlohmann::json;
json LastQuery,LastSubmission,HostSnapshot={{"Players",json::array({{{"Path","/Player/Local"},{"Name","Local player"}}})}};
uint64_t HostGeneration=41,HostRevision=8;
bool ThrowHost=false;
unsigned SettingsWrites=0;
unsigned IconManifestReads=0;
unsigned CrashOptionReads=0;
std::map<std::string,unsigned> GuardedCalls;
json SettingsDoc={{"advancedLogging",false},{"persistence",{{"allowCorruptCharacterEntry",false}}},{"future",std::string(10000,'x')}};
PS::HelpyAuthoring::Session AuthoringHost;
int32_t RS_PLUGIN_CALL HostCall(const char*,const char* service,const char* input,char* output,uint32_t capacity,uint32_t* size){
    if(ThrowHost)throw std::runtime_error("host read failed");
    assert(std::string(service)=="runeschema.tools");
    const auto request=json::parse(input);json reply;
    const auto action=request.value("Action",std::string{});
    if(action=="ModsRead"||action=="ModsSave"||action=="ModsImport"||action=="QuickLoadSave"||action=="AdditionalLoggingSave"||action=="CrashDumpSave"){
        if(!output||capacity<RUNESCHEMA_PLUGIN_MAX_MESSAGE){*size=RUNESCHEMA_PLUGIN_MAX_MESSAGE;return RS_PLUGIN_BUFFER_TOO_SMALL;}
        ++GuardedCalls[action];reply={{"Ok",true},{"Root","D:/XboxGames/Active/RuneSchema"},{"Rows",json::array()},{"Padding",std::string(10000,'x')}};
    }
    else if(request["Action"]=="SettingsRead"||request["Action"]=="SettingsSave"){
        if(request["Action"]=="SettingsSave"){
            ++SettingsWrites;assert(output&&capacity==RUNESCHEMA_PLUGIN_MAX_MESSAGE);
            assert(request["ExpectedContents"]=="fixture-contents");
            for(const auto& [path,value]:request.at("Patch").items())SettingsDoc[json::json_pointer(path)]=value;
        }
        reply={{"Ok",true},{"Document",SettingsDoc},{"Editable",{{"/advancedLogging",false},{"/persistence/allowCorruptCharacterEntry",false}}},
            {"ExpectedContents","fixture-contents"},{"Status","Saved for next launch"}};
    }
    else if(action=="CrashDumpRead"){
        ++CrashOptionReads;reply={{"Ok",true},{"FullMemoryDump",false},{"EnableDumping",true},{"ExpectedContents",std::string(10000,'x')}};
    }
    else if(action=="IconCacheSources"){
        ++IconManifestReads;
        reply={{"Version",4},{"Ready",true},{"Authoritative",true},{"PlanRevision",1},{"ContentRevision",1},
            {"Key","0123456789abcdef"},{"Sources",{{"game","abcdef0123456789"}}},{"Padding",std::string(20000,'x')}};
    }
    else if(request["Action"]=="AuthoringSnapshot")reply=AuthoringHost.Snapshot(request.value("Revision",uint64_t{}));
    else if(request["Action"]=="AuthoringSubmit")reply=AuthoringHost.Submit(request.at("Request"));
    else if(request["Action"]=="AuthoringActivate")reply=AuthoringHost.Activate();
    else if(request["Action"]=="Read"){
        reply={{"Available",true},{"Waiting",false},{"Generation",HostGeneration},{"Revision",HostRevision}};
        if(request.value("Revision",uint64_t{})!=HostRevision)reply["Snapshot"]=HostSnapshot;
    }else{
        if(request["Action"]=="QuerySubmit")LastQuery=request.at("Request");
        if(request["Action"]=="Submit")LastSubmission=request.at("Request");
        reply={{"Accepted",true}};
    }
    const auto bytes=reply.dump();*size=static_cast<uint32_t>(bytes.size()+1);
    if(!output||capacity<*size)return RS_PLUGIN_BUFFER_TOO_SMALL;
    std::memcpy(output,bytes.c_str(),*size);return RS_PLUGIN_OK;
}
}
int main(){
    {
        auto preferences=PS::HelpyImGuiTheme::Theme{};preferences.runtimeTimeUnit=3;preferences.backgroundFile="Scene.png";preferences.backgroundStrength=.2f;
        preferences.showFps=true;preferences.helpyFpsLimit=45;preferences.fpsRefreshMs=500;
        preferences.showGameFps=true;preferences.fpsHotkey="F11";
        preferences.diagnosticToastsEnabled=false;preferences.diagnosticToastLimit=2;preferences.diagnosticHistoryDays=7;preferences.diagnosticSplitRatio=.6f;preferences.diagnosticDeveloperMode=true;
        PS::HelpyRuntimePreferences::CacheFirst=false;PS::HelpyRuntimePreferences::PassiveRefresh=false;
        const auto document=PS::HelpyImGuiTheme::DocumentFrom(preferences,200);
        assert(document["cache"]["loadOrder"]=="gameFirst"&&document["cache"]["passiveRefresh"]==false);
        assert(PS::HelpyImGuiTheme::DefaultDocument()["cache"]["loadOrder"]=="cacheFirst");
        const auto roundTrip=PS::HelpyBrowsePolicy::Read(document["cache"]);assert(!roundTrip.CacheFirst&&!roundTrip.PassiveRefresh);
        PS::HelpyRuntimePreferences::CacheFirst=true;PS::HelpyRuntimePreferences::PassiveRefresh=true;
        assert(document["runtime"]["timeUnit"]==3&&document["background"]["file"]=="Scene.png"&&document["background"]["strength"].get<float>()==.2f);
        assert(document["runtime"]["showFps"]==true&&document["runtime"]["helpyFpsLimit"]==45&&document["runtime"]["fpsRefreshMs"]==500);
        assert(document["runtime"]["showGameFps"]==true&&document["runtime"]["fpsHotkey"]=="F11");
        const auto defaults=PS::HelpyImGuiTheme::DefaultDocument()["runtime"];
        assert(defaults["showFps"]==false&&defaults["helpyFpsLimit"]==30&&defaults["fpsRefreshMs"]==1000);
        assert(defaults["showGameFps"]==false&&defaults["fpsHotkey"]=="F10");
        assert(!document["notifications"]["enabled"].get<bool>()&&document["notifications"]["visibleLimit"]==2&&document["diagnosticHistory"]["ageDays"]==7);
        assert(document["diagnosticPresentation"]["splitRatio"].get<float>()==.6f&&document["diagnosticPresentation"]["developerMode"].get<bool>());
        assert(PS::HelpyImGuiTheme::DefaultDocument()["runtime"]["timeUnit"]==0&&PS::HelpyImGuiTheme::DefaultDocument()["background"]["file"]=="");
    }
    PS::HelpyEnvironment::Configure({{"DedicatedServer",true}});
    {
        PS::ItemGridPicker::State picker;picker.Query="helm";picker.SearchInput="helm";picker.ActiveCategory="Armor";
        picker.FavoritesOnly=true;picker.PersonalTag="favorites";picker.SourceProvider="mods/dye";picker.PageIndex=4;
        PS::ItemGridPicker::SelectFamily(picker,"hard_leather_dyed_black_mirror_trim");
        assert(picker.Query.empty()&&picker.SearchInput.empty()&&!picker.FavoritesOnly&&picker.PersonalTag.empty());
        assert(picker.ActiveCategory=="All"&&picker.PageIndex==0&&picker.SourceProvider=="mods/dye"&&picker.ResetScroll);
    }
    RuneSchemaHostApi host{};host.CallService=HostCall;State state{&host};ConfigureTransport(&state);
    {json reply;assert(CallCore(&state,{{"Action","CrashDumpRead"}},reply)&&reply.at("ExpectedContents").get<std::string>().size()==10000&&CrashOptionReads==2);}
    {json manifest;assert(PS::HelpySourceCache::RemoteSnapshot(manifest)&&manifest.at("Padding").get<std::string>().size()==20000&&IconManifestReads==2);
        PS::HelpySourceCache::Current.Provisional=true;PS::HelpySourceCache::CachedSourcesAttempted=true;PS::HelpySourceCache::Next={};
        assert(PS::HelpySourceCache::Poll({})&&PS::HelpySourceCache::Ready()&&!PS::HelpySourceCache::Current.Provisional);
        assert(PS::HelpySourceCache::Fingerprint("/Game/UI/T_Power.T_Power")=="abcdef0123456789");
        PS::HelpySourceCache::Current={};}
    for(const auto* action:{"ModsRead","ModsSave","ModsImport","QuickLoadSave","AdditionalLoggingSave","CrashDumpSave"}){
        json reply;assert(CallCore(&state,{{"Action",action}},reply)&&reply.at("Ok")==true&&reply.at("Root")=="D:/XboxGames/Active/RuneSchema");assert(GuardedCalls[action]==1);
    }
    assert(state.ToolsGeneration==41&&state.ToolsAvailable&&state.Players.size()==1); // No bootstrap push required.
    const auto unchangedRevision=state.ToolsRevision;SyncHost(&state,true);SyncHost(&state,true);
    assert(state.ToolsRevision==unchangedRevision); // Heartbeats do not invalidate row/order caches.
    const auto serverGrant=json{{"Action","GiveItems"},{"Player","/Server/Player/Remote"},{"Items",json::array({{{"Item","/Game/Items/Log.Log"},{"Quantity",1}}})}};
    assert(PS::SpawnToolRequests::TrySubmit(serverGrant)&&LastSubmission==serverGrant);
    StoreReceipt(&state,{{"_RequestId",uint64_t(556)},{"JournalCatalog",{{"Entries",json::array({{{"Path","/Game/Journal/Native.Native"},{"Native",true},{"Pages",json::array({"native text"})}}})},{"HasMore",false}}}});
    json journalReceipt;assert(PS::SpawnToolRequests::TakeCompleted(556,journalReceipt)&&journalReceipt["JournalCatalog"]["Entries"][0]["Pages"][0]=="native text");
    StoreReceipt(&state,{{"_RequestId",uint64_t(557)},{"InventorySnapshot",{{"PlayerGuid","fixture"},{"Items",json::array({{{"Name","Ash Logs"},{"Count",12}}})}}}});
    json inventoryReceipt;assert(PS::SpawnToolRequests::TakeCompleted(557,inventoryReceipt)&&inventoryReceipt["InventorySnapshot"]["Items"][0]["Count"]==12);
    PS::RuneSchemaSettingsPanel::Load();assert(PS::RuneSchemaSettingsPanel::Loaded&&SettingsWrites==0);
    PS::RuneSchemaSettingsPanel::Draft["advancedLogging"]=true;PS::RuneSchemaSettingsPanel::Dirty=true;
    assert(PS::RuneSchemaSettingsPanel::Patch()==json({{"/advancedLogging",true}}));
    PS::RuneSchemaSettingsPanel::Save();assert(SettingsWrites==1&&!PS::RuneSchemaSettingsPanel::Dirty&&SettingsDoc["advancedLogging"]==true);
    PS::RuneSchemaSettingsPanel::Save();assert(SettingsWrites==1); // no duplicate write to fetch a large reply
    using namespace PS::LazyCatalog;
    ResetForGeneration(41);FirstOpen(41);
    assert(LastQuery["Operation"]=="CatalogWindow"&&LastQuery["Category"]==0&&LastQuery["ClientGeneration"]==41&&LastQuery["Mode"]=="Alphabetical");
    const auto push=[&](json rows,bool more,std::string cursor){
        const auto request=LastQuery;
        json window={{"RequestId",request["RequestId"]},{"CancelGeneration",request["CancelGeneration"]},{"RuntimeGeneration",HostGeneration},
            {"Category",0},{"Mode",request["Mode"]},{"Reset",request["Reset"]},{"Rows",rows},{"HasMore",more},{"NextCursor",cursor},
            {"Dictionary",{{"Icons",json::array({{{"Id",uint64_t(7)},{"Value","/Game/Icons/Log.Log"}}})}}}};
        const auto message=json({{"Kind","query"},{"Generation",HostGeneration},{"Available",true},{"Payload",{{"CatalogWindow",window}}}}).dump();
        uint32_t size=0;assert(ToolsPush(&state,message.c_str(),nullptr,0,&size)==RS_PLUGIN_OK);
        uint64_t revision=0;json snapshot;assert(PS::SpawnToolRequests::RemoteReadQuick(revision,0,snapshot));
        Apply(snapshot,revision);Overlay(snapshot,0);return snapshot;
    };
    PS::ItemGridPicker::State filterState;filterState.SourceFilter=PS::ItemSource::Kind::Modded;filterState.ActiveCategory="Armor";filterState.FamilyKey="fallen_hoplite_dyed_chrome";
    const auto sourceQuery=PS::ItemGridPicker::BackendQuery(filterState);
    assert(Search(0,sourceQuery)&&LastQuery["Query"]=="__helpysource:modded __helpycat:armor __helpyfamily:fallen_hoplite_dyed_chrome");
    auto sourceSnapshot=push(json::array({{{"Path","/Dye/Helm.Helm"},{"Name","Fallen Hoplite's Helm (Dyed Chrome)"},{"Category","Armor"},{"SourceKind","modded"},{"SourceName","Dye"}}}),false,"done");
    PS::ItemGridPicker::Rebuild(sourceSnapshot,filterState,1);
    assert(filterState.Matches.size()==1&&PS::ItemSource::Row(sourceSnapshot["Items"][0]).Name=="Dye");
    {
        const auto savedSources=PS::HelpySourceCache::Current;
        PS::HelpySourceCache::Current.DisplayNames={{"dye","Colorful Dye"},{"not-on-this-page","Unloaded Mod"}};
        PS::HelpySourceCache::Current.Containers={{"mods/dye/paks/Colors_P.utoc",{{"Owner","mods/dye"},{"Status","Indexed"}}}};
        const auto choices=PS::ItemGridPicker::ProviderChoices(sourceSnapshot);
        assert(std::find(choices.begin(),choices.end(),std::pair<std::string,std::string>{"mods/dye","Colorful Dye"})!=choices.end());
        assert(std::find(choices.begin(),choices.end(),std::pair<std::string,std::string>{"mods/not-on-this-page","Unloaded Mod"})!=choices.end());
        filterState.SourceProvider="mods/dye";
        assert(PS::ItemCatalogFilter::Query(PS::ItemGridPicker::BackendQuery(filterState))(sourceSnapshot["Items"][0]));
        PS::ItemGridPicker::Rebuild(sourceSnapshot,filterState,2);assert(filterState.Matches.size()==1);
        filterState.SourceProvider="mods/other";PS::ItemGridPicker::Rebuild(sourceSnapshot,filterState,3);assert(filterState.Matches.empty());
        filterState.SourceProvider.clear();PS::HelpySourceCache::Current=savedSources;
    }
    PS::ItemGridPicker::State orderedState;
    json unordered={{"Items",json::array({{{"Path","/Game/Z"},{"Name","Zinc"}},{{"Path","/Mod/B"},{"Name","apple"}},{{"Path","/Game/A"},{"Name","Apple"}},{{"Path","/Game/B"},{"Name","Bronze"}}})}};
    PS::ItemGridPicker::Rebuild(unordered,orderedState,2);
    assert((orderedState.Matches==std::vector<size_t>{2,1,3,0}));
    assert(Search(0,"test"));
    // Real host replies can be empty while still scanning toward ItemData.
    for(int i=0;i<8;++i){push(json::array(),true,"R:18:"+std::to_string((i+1)*2048));assert(For(0).Pending&&For(0).LastError.empty());}
    auto snapshot=push(json::array({{{"Path","/Game/Items/Log.Log"},{"Name","Ash logs"},{"Icon",nullptr},{"IconId",uint64_t(7)}}}),false,"done");
    assert(snapshot["Items"].size()==1&&snapshot["Items"][0]["Icon"]=="/Game/Icons/Log.Log");
    assert(snapshot["Players"].size()==1&&!For(0).Pending);
    // Replaying a snapshot must not duplicate a window.
    Apply(snapshot,999);assert(For(0).Rows.size()==1);
    assert(Retry(0));CheckTimeouts(For(0).RequestedAt+std::chrono::seconds(16));assert(!For(0).Pending&&!For(0).LastError.empty());
    assert(Retry(0));snapshot=push(json::array({{{"Path","/Game/Items/Stone.Stone"},{"Name","Stone"}}}),false,"done");
    assert(snapshot["Items"][0]["Name"]=="Stone");
    // A missed world-change push is recovered by the read-only state read.
    HostGeneration=42;++HostRevision;HostSnapshot["Players"]=json::array();SyncHost(&state,true);
    assert(state.ToolsGeneration==42&&state.Players.empty());ResetForGeneration(42);FirstOpen(42);assert(LastQuery["ClientGeneration"]==42);
    PS::SpawnToolsPanel::RefreshSnapshot(-1);
    PS::SpawnToolsPanel::DefinitionPending=777;
    state.Completed[777]={{"SpawnDefinition",{{"Key","npc:test"},{"Name","Test NPC"}}}};
    PS::SpawnToolsPanel::RefreshSnapshot(-1);
    assert(PS::SpawnToolsPanel::InspectedDefinitions.at("npc:test")["Name"]=="Test NPC");
    PS::SpawnToolsPanel::DefinitionPending=778;state.Completed[778]={{"SpawnDefinition",{{"Key",nullptr}}}};
    PS::SpawnToolsPanel::RefreshSnapshot(-1);assert(!PS::SpawnToolsPanel::DefinitionStatus.empty());
    PS::ItemDetails::Request("/Game/Items/Stone.Stone");const auto detailId=PS::ItemDetails::Pending;
    assert(detailId&&PS::ItemDetails::PendingPath=="/Game/Items/Stone.Stone");
    assert(LastSubmission["Action"]=="InspectClone"&&LastSubmission["InspectDetails"]==true&&!LastSubmission.contains("Player"));
    // Read-only item details travel over the same production request/receipt path.
    state.ToolsWaiting=false;
    const json nativeDetails={{"Source","/Game/Items/Stone.Stone"},{"Name","Stone"},{"Icon","/Custom/UI/ClonedHelm.ClonedHelm"},{"PowerLevel",4},
        {"Fields",json::array({{{"NativeName","Description"},{"Value","Native description"},{"HasValue",true}},{{"NativeName","BaseDamage"},{"Value",12},{"HasValue",true}}})}};
    const json detailMessage={{"Kind","result"},{"Available",true},{"Waiting",false},{"Generation",HostGeneration},{"Payload",{{"_RequestId",detailId},{"ItemDetails",nativeDetails}}}};
    uint32_t responseSize=0;const auto detailWire=detailMessage.dump();assert(ToolsPush(&state,detailWire.c_str(),nullptr,0,&responseSize)==RS_PLUGIN_OK);
    PS::ItemDetails::Poll();const auto* details=PS::ItemDetails::Find("/Game/Items/Stone.Stone");assert(details&&details->Error.empty());
    assert(PS::ItemPresentation::Description(details->Native)=="Native description");
    assert(PS::ItemPresentation::Facts(details->Native)[0].Value=="12");
    const json staleIcon={{"Path","/Game/Items/Stone.Stone"},{"Icon","/Game/UI/Source.Source"}};
    assert(PS::ItemGridPicker::IconForRow(staleIcon)=="/Custom/UI/ClonedHelm.ClonedHelm");
    PS::ItemGridPicker::ClearSharedCart();assert(PS::ItemGridPicker::AddToCart(staleIcon));
    assert(PS::ItemGridPicker::SharedCart.Entries()[0].Row["Icon"]=="/Custom/UI/ClonedHelm.ClonedHelm");PS::ItemGridPicker::ClearSharedCart();
    PS::ItemDetails::Request("/Game/Items/Stone.Stone");assert(!PS::ItemDetails::Pending); // Cached hover never resubmits.
    PS::ItemGridPicker::PinDetails(staleIcon);PS::ItemGridPicker::DetachedBay=true;
    assert(std::string(PS::ItemGridPicker::Bay.Selected)=="/Game/Items/Stone.Stone"&&PS::ItemGridPicker::Bay.DetailsExpanded);
    assert(PS::ItemPresentation::Description(PS::ItemDetails::Find(PS::ItemGridPicker::Bay.Selected)->Native)=="Native description");
    PS::ItemGridPicker::DetachedBay=false;assert(PS::ItemGridPicker::Bay.DetailsRow==staleIcon);
    HostGeneration=43;SyncHost(&state,true);PS::ItemDetails::Poll();assert(!PS::ItemDetails::Find("/Game/Items/Stone.Stone"));
    // Same cart, two authoritative recipients: one serialized request at a
    // time, immutable target snapshot, and clear only after BOTH confirmations.
    PS::SpawnToolsPanel::PollCartDelivery();state.ToolsWaiting=false;
    PS::SpawnToolsPanel::UiSnapshot["Players"]=json::array({{{"Path","/Game/Players/A.A"},{"Name","Alice"}},{{"Path","/Game/Players/B.B"},{"Name","Bob"}}});
    const auto batchItem=json{{"Path","/Game/Items/Stone.Stone"},{"Name","Stone"}};
    assert(PS::ItemGridPicker::AddToCart(batchItem));
    PS::SpawnToolsPanel::CartBatch.Begin(PS::ItemGridPicker::SharedCart.Payload(),{{"/Game/Players/A.A","Alice"},{"/Game/Players/B.B","Bob"}});
    const auto batchFirst=++PS::SpawnToolsPanel::CartSequence;assert(PS::ItemGridPicker::SharedCart.BeginDelivery(batchFirst));
    assert(PS::SpawnToolsPanel::SubmitNextCartRecipient(batchFirst));assert(LastSubmission["Player"]=="/Game/Players/A.A");
    const auto grant=json{{"GrantResults",json::array({{{"Item","/Game/Items/Stone.Stone"},{"Count",1},{"State","Confirmed"}}})}};
    state.Completed[batchFirst]=grant;state.ToolsWaiting=false;PS::SpawnToolsPanel::PollCartDelivery();
    assert(PS::ItemGridPicker::SharedCart.Entries().size()==1&&PS::ItemGridPicker::SharedCart.Delivery());
    assert(LastSubmission["Player"]=="/Game/Players/B.B");const auto batchSecond=PS::SpawnToolsPanel::CartBatch.PendingId();assert(batchSecond&&batchSecond!=batchFirst);
    state.Completed[batchSecond]=grant;state.ToolsWaiting=false;PS::SpawnToolsPanel::PollCartDelivery();
    assert(PS::ItemGridPicker::SharedCart.Entries().empty()&&!PS::ItemGridPicker::SharedCart.Delivery()&&!PS::SpawnToolsPanel::CartNeedsReview);
    assert(PS::ItemGridPicker::AddToCart(batchItem));PS::SpawnToolsPanel::CartBatch.Begin(PS::ItemGridPicker::SharedCart.Payload(),{{"/Game/Players/A.A","Alice"},{"/Game/Players/B.B","Bob"}});
    const auto failedFirst=++PS::SpawnToolsPanel::CartSequence;assert(PS::ItemGridPicker::SharedCart.BeginDelivery(failedFirst));assert(PS::SpawnToolsPanel::SubmitNextCartRecipient(failedFirst));
    state.Completed[failedFirst]={{"Status","Request unconfirmed"}};state.ToolsWaiting=false;PS::SpawnToolsPanel::PollCartDelivery();
    assert(LastSubmission["Player"]=="/Game/Players/A.A"&&PS::SpawnToolsPanel::CartNeedsReview&&PS::ItemGridPicker::SharedCart.Entries().size()==1);
    assert(!PS::ItemGridPicker::SharedCart.Delivery());PS::SpawnToolsPanel::PollCartDelivery();assert(LastSubmission["Player"]=="/Game/Players/A.A");
    PS::SpawnToolsPanel::CartNeedsReview=false;PS::ItemGridPicker::ClearSharedCart();
    // Map coordinates and artwork metadata survive production JSON transport.
    PS::MapProjection::Projection projection{{198000,91450},{0,1},{1,0},210000,210000,-90000,90000};
    constexpr uint64_t mapId=700;
    assert(PS::SpawnToolRequests::TrySubmit({{"Action","MapSnapshot"},{"_RequestId",mapId}}));
    assert(LastSubmission["Action"]=="MapSnapshot");
    json map={{"BridgeVersion",1},{"Projection",projection.ToJson()},{"Image",{{"File","native-map.rsicon"},{"Status","Ready"}}},
        {"Players",json::array({{{"Name","Local player"},{"Location",{198000,91450,100}}}})}};
    const auto mapWire=json({{"Kind","result"},{"Available",true},{"Waiting",false},{"Generation",HostGeneration},{"Payload",{{"_RequestId",mapId},{"MapSnapshot",map}}}}).dump();
    assert(ToolsPush(&state,mapWire.c_str(),nullptr,0,&responseSize)==RS_PLUGIN_OK);json mapReceipt;
    assert(PS::SpawnToolRequests::TakeCompleted(mapId,mapReceipt)&&mapReceipt["MapSnapshot"]==map);
    // Stage updates cannot masquerade as arrival or free the UI mutation slot.
    const auto travelProgress=json({{"Kind","result"},{"Available",true},{"Waiting",true},{"Generation",HostGeneration},
        {"Payload",{{"_RequestId",uint64_t(702)},{"_InProgress",true},{"Status","Loading destination"},
            {"MapTravel",{{"Phase","Preparing"},{"ElapsedSeconds",3},{"Committed",false}}}}}}).dump();
    assert(ToolsPush(&state,travelProgress.c_str(),nullptr,0,&responseSize)==RS_PLUGIN_OK);
    assert(PS::SpawnToolRequests::IsWaiting()&&!PS::SpawnToolRequests::TakeCompleted(702,mapReceipt));
    uint64_t travelRevision=0;json travelSnapshot;
    assert(PS::SpawnToolRequests::RemoteReadQuick(travelRevision,-1,travelSnapshot));
    assert(travelSnapshot["MapTravel"]["Phase"]=="Preparing"&&travelSnapshot.value("Players",json::array())==state.Players);
    const auto arrived=json({{"Kind","result"},{"Available",true},{"Waiting",false},{"Generation",HostGeneration},
        {"Payload",{{"_RequestId",uint64_t(702)},{"_InProgress",false},{"Status","Arrived"}}}}).dump();
    assert(ToolsPush(&state,arrived.c_str(),nullptr,0,&responseSize)==RS_PLUGIN_OK);
    assert(!PS::SpawnToolRequests::IsWaiting()&&PS::SpawnToolRequests::TakeCompleted(702,mapReceipt));
    assert(!PS::SpawnToolRequests::TakeCompleted(702,mapReceipt));
    // Height replies survive the production decoder without altering action receipts.
    const auto heightWire=json({{"Kind","mapground"},{"Available",true},{"Waiting",false},{"Generation",HostGeneration},
        {"Payload",{{"RequestId",uint64_t(703)},{"Generation",HostGeneration},{"GroundAvailable",true},{"Location",{198000,91450,1234}}}}}).dump();
    assert(ToolsPush(&state,heightWire.c_str(),nullptr,0,&responseSize)==RS_PLUGIN_OK);
    travelRevision=0;
    assert(PS::SpawnToolRequests::RemoteReadQuick(travelRevision,-1,travelSnapshot));
    assert(travelSnapshot.at("MapGround").at("Location")[2]==1234&&!PS::SpawnToolRequests::TakeCompleted(703,mapReceipt));
    const auto placement=PS::MapSpawnRequest::Build(projection,projection.ToWorld({0.25,0.75}),{{"Key","tree"},{"Class","/Game/Tree.Tree_C"},{"Type","Resource"}},"player",true,1,0,701);
    assert(PS::SpawnToolRequests::TrySubmit(placement)&&LastSubmission==placement);
    AuthoringHost.Configure(true,true);assert(PS::AuthoringClient::Activate());AuthoringHost.TickActivation();PS::AuthoringClient::Poll(true);
    assert(PS::AuthoringClient::Submit("Catalogue"));auto authoring=AuthoringHost.Take();assert(authoring);
    AuthoringHost.Complete((*authoring)["RequestId"],{{"Large",std::string(300000,'z')}},"Large snapshot");
    PS::AuthoringClient::Poll(true);assert(PS::AuthoringClient::Result("Catalogue")["Large"].get<std::string>().size()==300000);
    PS::AuthoringClient::Clear();
    // Production push/read route retains authoritative display roles, including
    // an empty roster. No role update is a mutation receipt or a client grant.
    const auto roleRow=json{{"Path","/Player/Admin"},{"Guid",std::string(32,'A')},{"Name","Same"},
        {"RoleStatus",PS::PlayerStatus::Describe(1,{1,2,4,8,128},false)}};
    const auto pushRoles=[&](const json& rows){const auto wire=json{{"Kind","catalog"},{"Available",true},{"Waiting",false},{"Generation",HostGeneration},
        {"Payload",{{"Players",rows},{"PlayerRoles",PS::PlayerStatus::Snapshot(rows,HostGeneration)}}}}.dump();
        assert(ToolsPush(&state,wire.c_str(),nullptr,0,&responseSize)==RS_PLUGIN_OK);PS::SpawnToolsPanel::RefreshSnapshot(-1);};
    pushRoles(json::array({roleRow}));assert(PS::PlayerBadges::Find(roleRow)->at("Role")=="admin");
    pushRoles(json::array());assert(!PS::PlayerBadges::Find(roleRow));assert(PS::SpawnToolsPanel::UiSnapshot["Players"].empty());
    // An empty dedicated roster still permits metadata-only AI/resource queries.
    assert(Search(1,"goblin")&&LastQuery["Category"]==1);
    assert(Search(2,"silver")&&LastQuery["Category"]==2);
    // Recipient editing never touches item quantities, including the legacy
    // single-player fallback. Clearing is explicit, not a hidden re-selection.
    namespace Cart=PS::SpawnToolsPanel;
    const auto roster=json::array({{{"Path","/Player/A"},{"Name","Alice"}},{{"Path","/Player/B"},{"Name","Bob"}}});
    pushRoles(roster);assert(PS::ItemGridPicker::AddToCart(batchItem));
    PS::ItemGridPicker::SharedCart.SetQuantity("/Game/Items/Stone.Stone",7);
    const auto staged=PS::ItemGridPicker::SharedCart.Payload();
    Cart::SelectedPlayer="/Player/A";Cart::CartRecipientsChosen=false;assert(Cart::EffectiveCartRecipients().size()==1);
    assert(Cart::RemoveCartRecipient("/Player/A")&&Cart::EffectiveCartRecipients().empty());
    assert(Cart::CartRecipientsChosen&&Cart::SelectedPlayer.empty()&&PS::ItemGridPicker::SharedCart.Payload()==staged);
    Cart::CartRecipients={{"/Player/A","Alice"},{"/Player/B","Bob"}};Cart::SelectedPlayer="/Player/B";
    assert(Cart::RemoveCartRecipient("/Player/B")&&Cart::EffectiveCartRecipients().size()==1&&Cart::SelectedPlayer=="/Player/A");
    assert(Cart::ClearCartRecipients()&&Cart::EffectiveCartRecipients().empty()&&PS::ItemGridPicker::SharedCart.Payload()==staged);
    assert(!Cart::ClearCartRecipients());
    Cart::CartRecipients={{"/Player/A","Alice"},{"/Player/B","Bob"}};Cart::SelectedPlayer="/Player/A";
    // Departure removes stale identities; matching display names do not retarget.
    pushRoles(json::array({{{"Path","/Player/NewA"},{"Name","Alice"}},{{"Path","/Player/B"},{"Name","Renamed Bob"}}}));
    assert(Cart::SelectedPlayer.empty()&&Cart::EffectiveCartRecipients().size()==1);
    assert(Cart::EffectiveCartRecipients()[0].Path=="/Player/B"&&Cart::EffectiveCartRecipients()[0].Name=="Renamed Bob");
    // A disconnected next recipient is never submitted from an immutable batch.
    Cart::CartBatch.Begin(staged,{{"/Player/B","Bob"},{"/Player/A","Alice"}});
    const auto disconnected=++Cart::CartSequence;assert(PS::ItemGridPicker::SharedCart.BeginDelivery(disconnected));state.ToolsWaiting=false;
    assert(Cart::SubmitNextCartRecipient(disconnected));assert(!Cart::ClearCartRecipients()&&!Cart::RemoveCartRecipient("/Player/B"));
    auto stagedReceipt=staged;for(auto& item:stagedReceipt)item["State"]="Confirmed";
    state.Completed[disconnected]={{"GrantResults",stagedReceipt}};state.ToolsWaiting=false;Cart::PollCartDelivery();
    assert(LastSubmission["Player"]=="/Player/B"&&!Cart::CartBatch.Pending()&&Cart::CartNeedsReview);
    assert(PS::ItemGridPicker::SharedCart.Payload()==staged&&!PS::ItemGridPicker::SharedCart.Delivery());
    Cart::CartNeedsReview=false;
    // Server -> menu -> single player, even when the clear push was missed.
    // Generation changes cancel UI batches, keep items, and reject old receipts.
    Cart::CartBatch.Begin(staged,{{"/Player/B","Bob"}});const auto interrupted=++Cart::CartSequence;
    assert(PS::ItemGridPicker::SharedCart.BeginDelivery(interrupted));state.ToolsWaiting=false;assert(Cart::SubmitNextCartRecipient(interrupted));
    HostGeneration=44;++HostRevision;HostSnapshot={{"Status","Menu"}};SyncHost(&state,true);Cart::RefreshSnapshot(-1);Cart::PollCartDelivery();
    assert(state.Players.empty()&&Cart::SelectedPlayer.empty()&&Cart::CartRecipients.empty()&&Cart::EffectiveCartRecipients().empty());
    assert(!PS::ItemGridPicker::SharedCart.Delivery()&&Cart::CartBatch.Receipts().empty()&&Cart::CartNeedsReview);
    assert(PS::ItemGridPicker::SharedCart.Payload()==staged);
    // A higher-generation result without a roster must not inherit any players.
    HostGeneration=45;++HostRevision;HostSnapshot={{"Status","Single player"}};
    const auto newWorld=json{{"Kind","result"},{"Generation",HostGeneration},{"Available",true},{"Waiting",false},{"Payload",{{"Status","New world"}}}}.dump();
    assert(ToolsPush(&state,newWorld.c_str(),nullptr,0,&responseSize)==RS_PLUGIN_OK);
    assert(state.Players.empty()&&state.Completed.empty()&&state.ToolsCatalog.empty());
    for(const auto* kind:{"catalog","result","clear","query","icon","bootstrap"}){
        const auto stale=json{{"Kind",kind},{"Generation",uint64_t(43)},{"Available",false},{"Waiting",true},
            {"Payload",{{"Players",roster},{"_RequestId",interrupted},{"GrantResults",stagedReceipt}}}}.dump();
        assert(ToolsPush(&state,stale.c_str(),nullptr,0,&responseSize)==RS_PLUGIN_OK);
    }
    assert(state.ToolsGeneration==45&&state.ToolsAvailable&&!state.ToolsWaiting&&state.Players.empty()&&state.Completed.empty()&&state.QueryResults.empty()&&state.IconResults.empty());
    Cart::RefreshSnapshot(-1);assert(Cart::EffectiveCartRecipients().empty()&&Cart::SelectedPlayer.empty());
    const auto single=json::array({{{"Path","/Player/Single"},{"Name","Alice"}}});HostSnapshot["Players"]=single;++HostRevision;pushRoles(single);
    assert(Cart::EffectiveCartRecipients().empty()); // Never auto-deliver to the new world.
    Cart::SelectedPlayer="/Player/Single";assert(Cart::EffectiveCartRecipients().size()==1);
    assert(Cart::EffectiveCartRecipients()[0].Path=="/Player/Single"&&PS::ItemGridPicker::SharedCart.Payload()==staged);
    json lateReceipt;assert(!PS::SpawnToolRequests::TakeCompleted(interrupted,lateReceipt));
    Cart::CartNeedsReview=false;assert(Cart::ClearCartRecipients());PS::ItemGridPicker::ClearSharedCart();
    // A busy read/validation does not require another click or submit a grant twice.
    Cart::SelectedPlayer="/Player/Single";Cart::CartRecipientsChosen=false;Cart::CartNeedsReview=false;
    assert(PS::ItemGridPicker::SharedCart.Add({{"Path","/Game/Items/Queued.Queued"}}));
    Cart::CartBatch.Begin(PS::ItemGridPicker::SharedCart.Payload(),Cart::EffectiveCartRecipients());
    const auto queued=++Cart::CartSequence;assert(PS::ItemGridPicker::SharedCart.BeginDelivery(queued));
    LastSubmission=json::object();state.ToolsWaiting=true;Cart::PollCartDelivery();Cart::PollCartDelivery();assert(LastSubmission.empty());
    state.ToolsWaiting=false;Cart::PollCartDelivery();const auto once=LastSubmission;
    assert(once["Action"]=="GiveItems"&&once["Player"]=="/Player/Single"&&Cart::CartBatch.Pending());
    Cart::PollCartDelivery();assert(LastSubmission==once);Cart::CartBatch.Stop();Cart::PollCartDelivery();PS::ItemGridPicker::ClearSharedCart();Cart::CartNeedsReview=false;
    ThrowHost=true;json ignored;assert(!CallCore(&state,{{"Action","Read"}},ignored));
    {namespace A=PS::AuthoringClient;A::Clear();bool busy=true;
        A::Remote=[&](const json&,json& reply){reply={{"Generation",77},{"Revision",busy?1:2},{"RequestId",uint64_t(778)},{"Operation","SaveEditorSlotItems"},{"Busy",busy},{"Result",{{"Rows",json::array({{{"Name","Cape"}}})}}}};return true;};
        A::Poll(true);assert(A::LastReceipt==0&&A::Result("SaveEditorSlotItems").empty());busy=false;A::Poll(true);assert(A::LastReceipt==778&&A::Result("SaveEditorSlotItems")["Rows"].size()==1);A::Clear();}
    {namespace A=PS::AuthoringClient;namespace P=PS::PlayerInventoryPanel;uint64_t requestId=0;bool complete=false;
        A::Snapshot={{"Generation",uint64_t(81)},{"Active",true},{"Enabled",true},{"Ready",true},{"Busy",false}};
        A::Remote=[&](const json& request,json& reply){if(request["Action"]=="AuthoringSubmit"){assert(request["Request"]["Operation"]=="SaveEditorSlotItems"&&request["Request"]["Arguments"]["Slot"]=="Cape"&&request["Request"]["Arguments"]["CacheFirst"]==true);requestId=request["Request"]["RequestId"].get<uint64_t>();reply={{"Accepted",true}};return true;}
            reply={{"Generation",uint64_t(81)},{"Revision",uint64_t(complete?3:2)},{"Active",true},{"Enabled",true},{"Ready",true},{"Busy",requestId&&!complete},{"RequestId",requestId},{"Operation","SaveEditorSlotItems"},{"Result",complete?json{{"Slot","Cape"},{"Rows",json::array({{{"Name","Cape"},{"NativeSlotVerified",true},{"SlotStrategy","Cape"}}})},{"NextOffset",1},{"HasMore",false}}:json::object()}};return true;};
        P::Open=true;P::Generation=PS::SpawnToolRequests::CurrentGeneration();P::ChangingGear=true;P::GearLocation="Cape";P::GearRows=json::array();P::GearCatalogRequest=0;P::GearMore=true;P::GearOffset=0;P::GearPending=0;P::Pending=0;P::Refresh=false;P::Bay.Query.clear();P::GearQuery.clear();
        P::Poll();assert(requestId&&P::GearCatalogRequest==requestId&&P::GearRows.empty());A::Poll(true);assert(P::GearCatalogRequest&&A::LastReceipt==0);
        complete=true;A::Poll(true);P::Poll();assert(P::GearRows.size()==1&&!P::GearMore&&P::GearCatalogRequest==0);P::Open=false;P::ChangingGear=false;A::Clear();}
    {namespace P=PS::PlayerInventoryPanel;auto& cache=PS::LazyCatalog::For(0);const auto saved=cache;
        cache={};cache.RuntimeGeneration=PS::SpawnToolRequests::CurrentGeneration();cache.Query="unrelated-global-filter";
        const json cape={{"Name","Native cape"},{"Path","/Game/Cape.Cape"},{"SlotStrategy","Cape"},{"NativeSlotVerified",true}};
        auto head=cape;head["Path"]="/Game/Head.Head";head["SlotStrategy"]="Head";
        auto unverified=cape;unverified["Path"]="/Game/Unknown.Unknown";unverified.erase("NativeSlotVerified");
        cache.Rows=json::array({cape,head,unverified});cache.Cache["duplicate"]={json::array({cape}),"",false,cache.RuntimeGeneration};
        auto stale=cape;stale["Path"]="/Game/Stale.Stale";cache.Cache["stale"]={json::array({stale}),"",false,cache.RuntimeGeneration+1};
        P::ChangeGear({{"Name","Player"}},{{"Location","Cape"}});
        assert(P::GearRows==json::array({cape})&&cache.Query=="unrelated-global-filter"&&P::Bay.ActiveCategory=="All");
        P::Open=false;P::ChangingGear=false;P::Refresh=false;cache=saved;}
    {namespace P=PS::PlayerInventoryPanel;
        ThrowHost=false;state.ToolsAvailable=true;state.ToolsWaiting=false;P::Open=false;P::Detached=false;P::Refresh=false;P::Target={{"Name","Keep inspector target"}};
        const json player={{"Path","/Player/Repair"},{"Guid","0123456789abcdef0123456789abcdef"}},item={{"Path","/Game/Gear.Gear"},{"Instance",{1,2,3,4}},{"Slot",2}};
        P::RestoreGear(player,item);const auto repair=P::RepairPending;
        assert(repair&&LastSubmission["Action"]=="RestorePlayerGear"&&LastSubmission["Player"]==player["Path"]&&LastSubmission["PlayerGuid"]==player["Guid"]&&LastSubmission["Instance"]==item["Instance"]&&LastSubmission["Slot"]==2);
        assert(!P::Open&&!P::Detached&&!P::Refresh&&P::Target["Name"]=="Keep inspector target");
        P::RestoreGear(player,item);assert(P::RepairPending==repair&&LastSubmission["_RequestId"]==repair);
        state.Completed[repair]={{"EquipmentAction",{{"Confirmed",true}}}};state.ToolsWaiting=false;P::Poll();assert(!P::RepairPending&&P::RepairStatus=="Durability restored."&&!P::Open);
        P::RestoreGear(player,item);assert(P::RepairPending);P::RepairStarted-=std::chrono::seconds(16);P::Poll();assert(!P::RepairPending&&P::RepairStatus.find("unconfirmed")!=std::string::npos&&!P::Open);
        state.ToolsWaiting=false;P::RestoreGear(player,item);assert(P::RepairPending);P::RepairGeneration=PS::SpawnToolRequests::CurrentGeneration()+1;P::Poll();assert(!P::RepairPending&&P::RepairStatus.find("World changed")!=std::string::npos&&!P::Open);state.ToolsWaiting=false;
    }
    PS::ItemIconCache::ClearRemote();ClearRemote();PS::SpawnToolRequests::ClearRemote();
    PS::RuneSchemaSettingsPanel::Remote={};
}
