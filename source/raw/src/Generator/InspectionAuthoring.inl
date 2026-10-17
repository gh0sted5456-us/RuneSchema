// Desktop facade over the same capture/export implementations as the legacy UI.
// Included after the private helpers. Called only from InspectionTools::Tick.
namespace {
uint64_t desktopLegacyId=0,desktopQuestId=0;
auto desktopQuestDeadline=std::chrono::steady_clock::time_point{};
bool desktopRequestSucceeded=true;
std::string desktopOperation;
json DesktopCaptured(const std::string& operation={}) {
    // Unrelated previous traces must not consume the next capture's IPC budget.
    if(operation=="AssetSearch")return {{"AssetMatches",assetMatches}};
    if(operation=="AssetCapture")return {{"Fields",assetFields},{"AvailableFields",assetHints},{"Entry",assetRecord}};
    if(operation=="TraceTarget"||operation=="TraceSelect")return {{"TraceTarget",traceTargetPath}};
    if(operation=="TraceBrowse")return {{"TraceMatches",traceTargetMatches}};
    if(operation=="Browse")return {{"Matches",matches}};
    if(operation=="SavedItems")return {{"SavedItems",saveReports[0]}};
    if(operation=="SavedWorld")return {{"SavedWorld",saveReports[1]}};
    if(operation=="PlayerTraceStop")return {{"Events",lastEventTrace}};
    if(!operation.empty())return {{"Snapshot",snapshot}};
    return {{"Snapshot",snapshot},{"Matches",matches},{"AssetMatches",assetMatches},{"Fields",assetFields},{"AvailableFields",assetHints},
        {"Entry",assetRecord},{"TraceTarget",traceTargetPath},{"TraceMatches",traceTargetMatches},{"Events",lastEventTrace},
        {"SavedItems",saveReports[0]},{"SavedWorld",saveReports[1]}};
}
json DesktopBuild(const json& args,bool nameplate) {
    const auto id=args.at("Id").get<std::string>();if(!ActivityIdentifier(id))throw std::runtime_error("Id must contain letters, numbers or underscores");
    auto body=args.value("Fields",json::object());
    if(nameplate){
        const auto mode=body.value("Mode",std::string("Name"));if(mode!="Name"&&mode!="Icon"&&mode!="Hidden")throw std::runtime_error("Invalid nameplate Mode");
        if(mode=="Icon"&&body.value("Icon",std::string{}).empty())throw std::runtime_error("Icon mode requires a cooked icon");
        if(body.contains("Events"))ValidateActivityEvents(body["Events"],body.value("States",json::object()));
        return json::array({{{"Id",id},{"Nameplate",body}}});
    }
    const auto player=args.at("Player").get<std::string>();if(player.empty()||player.size()>128)throw std::runtime_error("A player selector is required");
    for(const auto* key:{"Id","$Id","PlayerName","PlayerNames","PlayerGuid","PlayerGuids"})body.erase(key);
    body["Id"]=id;body["PlayerName"]=player;
    const auto definition=args.value("Definition",std::string{});if(!definition.empty())body["Nameplate"]["Definition"]=definition;
    return json::array({body});
}
bool DesktopLegacy(const std::string& op,const json& args,uint64_t id) {
    Action action=Action::None;
    const std::pair<const char*,Action> actions[]{
        {"AssetSearch",Action::AssetSearch},{"AssetCapture",Action::AssetCapture},{"AssetReference",Action::AssetReference},{"StarterCheck",Action::StarterCheck},
        {"Target",Action::Target},{"Player",Action::Player},{"Browse",Action::Browse},{"Object",Action::Object},{"Preset",Action::Preset},
        {"TraceTarget",Action::TraceTarget},{"TraceBrowse",Action::TraceBrowse},{"TraceSelect",Action::TraceSelect},{"PlayerTraceStart",Action::PlayerTraceStart},
        {"PlayerTraceStop",Action::PlayerTraceStop},{"PlayerTraceMarker",Action::PlayerTraceMarker},{"TraceStart",Action::TraceStart},{"TraceStop",Action::TraceStop},
        {"SavedItems",Action::SavedItems},{"SavedWorld",Action::SavedWorld},{"Clear",Action::Clear},
        {"NiagaraAttach",Action::NiagaraAttach},{"NiagaraRemove",Action::NiagaraRemove},{"NiagaraInspect",Action::NiagaraInspect},{"NiagaraArmorStart",Action::NiagaraArmorStart},{"NiagaraArmorStop",Action::NiagaraArmorStop}};
    for(const auto& pair:actions)if(op==pair.first){action=pair.second;break;}if(action==Action::None)return false;
    std::string text;
    if(op.starts_with("Asset")||op=="StarterCheck") {Capability(args.at("Loader").get<std::string>());text=args.dump();}
    else if(op=="Preset") {ValidatePreset(args.at("Document"),args.value("Extended",false));text=args.at("Document").dump();}
    else if(op=="PlayerTraceStart") {auto options=args.at("Options");PlayerTrace::ValidateOptions(options);text=options.dump();}
    else if(op=="NiagaraAttach")text=NiagaraPreset::Validate(args.at("Document")).dump();
    else if(op=="NiagaraArmorStart") {const int slots=args.value("ArmorSlots",7);if(slots<1||slots>7)throw std::runtime_error("Armor slot mask must be 1..7");text=std::to_string(slots);}
    else if(op=="Browse"||op=="TraceBrowse")text=args.at("Query").get<std::string>();
    else if(op=="Object"||op=="TraceSelect")text=args.at("Path").get<std::string>();
    else if(op=="PlayerTraceMarker")text=args.at("Marker").get<std::string>();
    std::lock_guard lock(mutex);if(busy)throw std::runtime_error("A legacy authoring operation is still running");
    if(op.starts_with("Asset")||op=="StarterCheck")assetLoader=args.at("Loader");
    rangeMeters=args.value("RangeCm",10000.0f)/100.0f;playerCaptureDelay=args.value("DelaySeconds",0);
    extendedCaptureDepth=args.value("Extended",false);delay=args.value("DelayCrosshair",true);Queue(action,std::move(text));
    pending.objectLimit=args.value("Limit",250);
    if(args.contains("SelectedObject")){pending.selectedObject=args.at("SelectedObject");if(op=="PlayerTraceStart"&&!args.value("UseProfileTarget",false))selectedTargetResolver=nullptr;}
    desktopLegacyId=id;desktopOperation=op;desktopRequestSucceeded=true;return true;
}
json DesktopGuide() {
    auto guide=json::parse(ConfigFiles::Read(HostServices::SearchesDirectory()/"diagnostic-guide.json",16384));
    if(!guide.is_object()||guide.value("Version",json{})!=1)throw std::runtime_error("Unsupported diagnostic guide version");
    for(const auto* section:{"Presets","Traces","Libraries"}){
        if(!guide.contains(section)||!guide[section].is_array()||guide[section].size()>32)throw std::runtime_error("Invalid guide section");
        for(const auto& line:guide[section])if(!line.is_string()||line.get_ref<const std::string&>().size()>2048)throw std::runtime_error("Invalid guide text");
    }return guide;
}
void DesktopExportOptions(const json& args){
    if(!args.contains("Export"))return;const auto& output=args["Export"];
    const auto name=output.value("Name",std::string{});DiagnosticExportName(name,"Report","check",true);
    std::lock_guard lock(exportMutex);std::snprintf(exportName,sizeof(exportName),"%s",name.c_str());exportJsonc=output.value("Jsonc",false);
}
json DesktopOperation(const std::string& op,const json& args) {
    if(op=="Catalogue") {
        json loaders=json::array();for(const auto& c:LoaderCapabilities)loaders.push_back({{"Name",c.Name},{"Title",c.DisplayName},{"Patch",c.StarterPatch},{"Clone",c.Clone},{"Append",c.Append},{"AuthoredSearch",c.AuthoredSearch},{"Help",c.Purpose}});
        bool player=false;try{player=Pawn(Controller())!=nullptr;}catch(...){}
        return {{"Loaders",loaders},{"PlayerAvailable",player},{"Exports",HostServices::ExportsDirectory().string()},{"Status",player?"Authoring ready.":"No local player. File authoring and explicitly selected loaded-object tools remain available."}};
    }
    if(op=="DiagnosticGuide")return {{"Document",DesktopGuide()},{"Folder",HostServices::SearchesDirectory().string()}};
    if(op=="TraceTargetClear"){std::lock_guard lock(mutex);traceTargetPath.clear();selectedTargetResolver=nullptr;return {{"TraceTarget",""},{"Status","Trace target cleared. Recording unchanged."}};}
    if(op=="PlayerPresetSave"){
        const auto preset=json{{"Name","PlayerCapture"},{"IncludePlayer",true},{"CaptureDelaySeconds",args.value("DelaySeconds",0)}};ValidatePreset(preset);
        const auto stamp=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        const auto path=PresetFolder()/("PlayerCapture_"+std::to_string(stamp)+".jsonc");
        if(std::filesystem::exists(path))throw std::runtime_error("Player preset already exists; retry");
        ConfigFiles::Write(path,preset.dump(2));presetsLoaded=false;return {{"File",path.string()},{"Document",preset},{"Status","Player capture preset saved in the existing shared library."}};
    }
    if(op=="NiagaraPresets") {
        auto result=DiagnosticLibrary::Load(HostServices::PresetsDirectory()/"loaders"/"niagara",{256,1024,16384},[](const auto& text){return NiagaraPreset::Validate(NiagaraPreset::Parse(text));});
        json documents=json::array();for(size_t i=0;i<4;++i)documents.push_back(NiagaraPreset::Builtin(i));for(const auto& item:result.Documents)documents.push_back(item);
        return {{"Documents",documents},{"Errors",result.Errors}};
    }
    if(op=="Schema") {const auto loader=args.at("Loader").get<std::string>();Capability(loader);return {{"Document",JsonSchemaGenerator::LoaderSchemas().at(loader)},{"Loader",loader},{"Status","Structural schema; runtime validation is still required."}};}
    if(op=="SchemaExport") {
        if(args.value("Loader",std::string{})=="all")return {{"File",JsonSchemaGenerator::GenerateSchemaFiles(args.value("IncludeLoadedTables",false),args.value("Name",std::string{}))}};
        const auto loader=args.at("Loader").get<std::string>();Capability(loader);const auto document=JsonSchemaGenerator::LoaderSchemas().at(loader);return {{"Document",document},{"File",Export(document,"Schema-"+loader)}};
    }
    if(op=="StarterBuild") {const auto loader=args.at("Loader").get<std::string>();const auto mode=args.value("Mode",0);auto document=LoaderTemplate::Build(loader,args.at("Entry"),args.at("Fields"),mode,args.value("Mod",std::string("MyMod")),args.value("Name",std::string("NewItem")),args.value("Id",std::string{}));return {{"Document",document},{"Baseline",document},{"Loader",loader},{"Jsonc",LoaderTemplate::Jsonc(loader,mode,document)}};}
    if(op=="PlayerBuild"||op=="NameplateBuild")return {{"Document",DesktopBuild(args,op=="NameplateBuild")},{"Loader",op=="NameplateBuild"?"nameplates":"players"},{"Status","Create-new preview; review before exporting. No content installed."}};
    if(op=="EventSuggestions")return {{"Suggestions",ActivityParameterSuggestions(args.at("Event").at("parameters"))},{"Event",args.at("Event")}};
    if(op=="EventRuleBuild")return {{"Document",BuildActivityRuleTemplate(args.at("Event"),args.at("Conditions"),args.at("State"),args.at("Icon"),args.value("Timeout",3.0),args.value("Player",std::string("*")))},{"Loader","players"}};
    if(op=="ExportDocument") {
        const auto kind=args.value("Kind",std::string("Report"));auto document=args.at("Document");
        if(kind=="Niagara")document=NiagaraPreset::Validate(document);
        if(kind!="Report"&&kind!="Starter"&&kind!="Rule"&&kind!="Niagara")throw std::runtime_error("Unsupported document export kind");
        {std::lock_guard settingsLock(exportMutex);const auto name=args.value("Name",std::string{});DiagnosticExportName(name,"Export","check",true);std::snprintf(exportName,sizeof(exportName),"%s",name.c_str());exportJsonc=args.value("Jsonc",true);}
        std::lock_guard lock(mutex);if(kind=="Starter"){assetLoader=args.at("Loader");Capability(assetLoader);}
        return {{"File",Export(document,"Authoring",kind=="Rule",kind=="Starter",args.value("Mode",0),kind=="Niagara")},{"Status","Export saved; not installed or activated."}};
    }
    if(op=="ExportCaptured"){
        std::lock_guard lock(mutex);const auto kind=args.value("Kind",std::string("Captured"));json report;
        if(kind=="Captured")report=DesktopCaptured();
        else if(kind=="Snapshot")report=snapshot;
        else if(kind=="SavedItems")report=saveReports[0];
        else if(kind=="SavedWorld")report=saveReports[1];
        else if(kind=="Events")report=lastEventTrace;
        else if(kind=="AssetSearch")report=SearchReport(assetSearchQuery,AuthoredStarters::Supports(assetLoader)?"Installed authored files, including inactive mods":"Loaded game records",assetLoader,assetMatches,100);
        else if(kind=="ObjectSearch")report=SearchReport(objectSearchQuery,"Loaded object paths (case-insensitive substring)","",json(matches),completedObjectResultLimit);
        else throw std::runtime_error("Unsupported capture export kind");
        if(report.is_null())throw std::runtime_error("Capture this report before exporting it");
        return {{"File",Export(report,kind)},{"Status","Complete current capture exported."}};
    }
    if(op=="PresetsList") {std::lock_guard lock(mutex);extendedCaptureDepth=args.value("Extended",false);ReloadPresets();return {{"Documents",presets},{"Errors",presetErrors},{"Status",status}};}
    if(op=="PresetSave") {const auto document=args.at("Document");ValidatePreset(document,args.value("Extended",false));const auto path=PresetFolder()/("user-"+document.at("Name").get<std::string>()+".json");ConfigFiles::Write(path,document.dump(2));return {{"File",path.string()},{"Status","Preset saved. A matching user preset name is replaced."}};}
    if(op=="TraceProfilesList") {const auto result=DiagnosticLibrary::Load(HostServices::TraceProfilesDirectory(),{128,512,32768},[](const auto& text){return ValidateTraceProfile(ParseTraceProfile(text));});return {{"Documents",result.Documents},{"Errors",result.Errors}};}
    if(op=="TraceProfileSave"||op=="TraceProfileLoad") {
        const auto profile=ValidateTraceProfile(args.at("Document"));
        if(op=="TraceProfileLoad") {const auto target=profile.contains("Target")?ResolveProfileTarget(profile["Target"]):std::string{};std::lock_guard lock(mutex);selectedTargetResolver=profile.value("Target",json{});traceTargetPath=TraceTargetAfterProfileLoad(profile,traceTargetPath,target);return {{"Document",profile},{"Options",profile.at("Trace")},{"Export",profile.at("Export")},{"TraceTarget",traceTargetPath},{"Status","Profile loaded. Recording unchanged."}};}
        const auto stamp=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();const auto path=HostServices::TraceProfilesDirectory()/DiagnosticExportName(profile.at("Name"),"Trace",std::to_string(stamp),true);
        if(std::filesystem::exists(path))throw std::runtime_error("Profile export exists; retry");ConfigFiles::Write(path,"// Loads controls only.\n"+profile.dump(2));return {{"File",path.string()}};
    }
    if(op=="Imports"||op.starts_with("Npc")||op.starts_with("Cleanup"))return SaveViewer::Authoring(op,args);
    throw std::runtime_error("Authoring operation has no handler");
}
void DesktopAuthoringTick() {
    if(auto cancel=HelpyAuthoring::Bridge.TakeCancellation()){
        {std::lock_guard lock(mutex);pending={};hasRequest=false;busy=false;desktopLegacyId=0;desktopQuestId=0;}
        HelpyAuthoring::Bridge.Complete(cancel->at("RequestId"),json::object(),"Pending authoring request cancelled. Active recordings unchanged.");return;
    }
    if(desktopQuestId){
        if(QuestStatusRequests::Waiting&&std::chrono::steady_clock::now()<desktopQuestDeadline)return;
        const bool ready=!QuestStatusRequests::Waiting;
        HelpyAuthoring::Bridge.Complete(desktopQuestId,ready?QuestStatusRequests::Read():json{{"Error","Quest status request timed out."}},ready?"Quest status refreshed.":"Quest status request timed out.",ready);desktopQuestId=0;
    }
    if(desktopLegacyId) {
        std::lock_guard lock(mutex);if(busy)return;
        HelpyAuthoring::Bridge.Complete(desktopLegacyId,DesktopCaptured(desktopOperation),status,desktopRequestSucceeded);desktopLegacyId=0;
    }
    auto request=HelpyAuthoring::Bridge.Take();if(!request)return;const auto id=request->at("RequestId").get<uint64_t>();const auto op=request->at("Operation").get<std::string>();const auto& args=request->at("Arguments");
    try {
        DesktopExportOptions(args);
        if(op=="QuestStatus"){
            if(!QuestStatusRequests::Available||QuestStatusRequests::Waiting)throw std::runtime_error("Quest status is unavailable or another request is pending");
            QuestStatusRequests::Submit({{"Action","Status"}});desktopQuestId=id;desktopQuestDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);return;
        }
        if(DesktopLegacy(op,args,id))return;auto result=DesktopOperation(op,args);const auto message=result.value("Status",std::string("Authoring operation completed."));HelpyAuthoring::Bridge.Complete(id,std::move(result),message);}
    catch(const std::exception& error){HelpyAuthoring::Bridge.Complete(id,{{"Error",error.what()}},error.what(),false);}
}
}
