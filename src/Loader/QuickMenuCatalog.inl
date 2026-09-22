// Included by SpawnTools.inl. All entry points run on the loader's game-thread pump.
// Asset records and weak world identities are retained, never borrowed FAssetData
// buffers or strong world/actor references. Indexing does not spawn actors.
namespace {
std::string ToolReadableName(std::string path) {
    const auto last=path.find_last_of("./");if(last!=std::string::npos)path=path.substr(last+1);
    if(path.starts_with("BP_"))path.erase(0,3);
    if(path.ends_with("_C"))path.resize(path.size()-2);
    std::replace(path.begin(),path.end(),'_',' ');return path;
}
bool ToolResourceClass(UClass* type,UClass* actorType,UClass* aiType) {
    if(!type||!type->IsChildOf(actorType)||type->IsChildOf(aiType))return false;
    // Discovery is not spawn permission. Keep real resource classes visible;
    // the shared spawn backend still validates save exclusion before mutation.
    // Persistence alone is not evidence that an actor is a resource: prefer
    // resource data/components, then exclude buildings before loose ancestry names.
    for(const auto* field:{TEXT("ResourceData"),TEXT("ResourceNodeData"),TEXT("HarvestData"),TEXT("ResourceConfig"),TEXT("HarvestableData"),TEXT("GatherableData"),
        TEXT("FishingSpotData"),TEXT("FishingData"),TEXT("TreeData"),TEXT("MiningData")})
        if(PropertyHelper::GetPropertyByName(type,field))return true;
    for(auto* property:TFieldRange<FProperty>(type,EFieldIterationFlags::Default)) {
        if(auto* component=CastField<FObjectProperty>(property)) {
            auto* componentType=component->GetPropertyClass().Get();if(!componentType)continue;
            const auto n=PS::QuickUI::Lower(RC::to_string(componentType->GetName()));
            for(const auto* token:{"resourcecomponent","harvestcomponent","harvestablecomponent","miningcomponent","fishingcomponent","gatherablecomponent"})
                if(n.find(token)!=n.npos)return true;
        }
    }
    // A genuine resource component/data field takes priority over a building binding.
    if(PropertyHelper::GetPropertyByName(type,TEXT("BuildingPieceDataIndex")))return false;
    size_t depth=0;
    for(UStruct* base=type;base&&depth++<64;base=base->GetSuperStruct()) {
        const auto name=PS::QuickUI::Lower(RC::to_string(base->GetName()));
        if(PS::QuickCatalogRules::MiningName(name)||PS::QuickCatalogRules::ResourceAncestry(name))return true;
    }
    return false;
}
nlohmann::json ToolDefinitionRow(UClass* type,const std::string& key,bool ai,const std::string& kind={}) {
    const auto path=RC::to_string(type->GetPathName());const bool concrete=!ActorHelper::IsAbstract(type);
    nlohmann::json row={{"Key",key},{"Name",ToolReadableName(path)},{"Class",path},{"Type",kind.empty()?(ai?"AI":"Resource"):kind},
        {"Available",concrete},{"Loaded",true},{"Reason",concrete?"":"Abstract base definition; not spawnable"}};
    row["Packaged"]=PS::CookedAssets::VerifiedLoadedObject(type);
    const auto declared=PS::AssetMetadata::Lookup(type);
    row["RuneSchemaManaged"]=PS::AssetMetadata::IsManaged(type);row["DeclaredModded"]=declared.modded.value_or(false);row["DeclaredCooked"]=declared.cooked.value_or(false);
    if(kind=="NPC") {
        row["TemporaryAllowed"]=false;row["PermanentAllowed"]=false;
        row["TemporaryReason"]="Vanilla NPC definition: browse-only. Use an authored RuneSchema NPC for timed portable spawning.";
        row["PermanentReason"]="Vanilla narrative NPC copying is not enabled; its native quest/store lifecycle must not be guessed.";
    }else if(!ai) {
        std::string family;
        std::size_t depth=0;
        for(UStruct* base=type;base&&depth++<64;base=base->GetSuperStruct()) {
            const auto name=PS::QuickUI::Lower(RC::to_string(base->GetName()));
            if(name.find("fellabletree")!=name.npos||name.find("felledtree")!=name.npos||name.find("splittablelog")!=name.npos||name.find("sapling")!=name.npos){family="Tree";break;}
            if(PS::QuickCatalogRules::MiningName(name))family="Mineral";
        }
        row["ResourceFamily"]=family;
    }
    // Presentation only: no writes and no fabricated default level.
    if(auto* defaults=type->GetClassDefaultObject().Get();defaults&&!defaults->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed|RF_NeedLoad|RF_NeedPostLoad))) {
        if(auto* power=CastField<FNumericProperty>(PropertyHelper::GetPropertyByName(type,TEXT("PowerLevel")));power&&power->GetArrayDim()==1) {
            const auto offset=power->GetOffset_Internal(),size=power->GetElementSize();
            if(offset>=0&&size>0&&offset<=type->GetPropertiesSize()&&size<=type->GetPropertiesSize()-offset) {
                auto* ptr=power->ContainerPtrToValuePtr<void>(defaults);
                const double value=power->IsFloatingPoint()?power->GetFloatingPointPropertyValue(ptr):static_cast<double>(power->GetSignedIntPropertyValue(ptr));
                if(std::isfinite(value)&&value>=0)row["PowerLevel"]=value;
            }
        }
    }
    return row;
}
nlohmann::json ToolLiveDefinitions(const nlohmann::json& authored) {
    auto* classType=ActorHelper::ResolveClass(TEXT("/Script/CoreUObject.Class"));
    auto* actorType=ActorHelper::ResolveClass(TEXT("/Script/Engine.Actor"));
    auto* aiType=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionAICharacter"));
    auto* npcType=ActorHelper::ResolveClass(TEXT("/Script/Dominion.InteractableNPC"));
    if(!classType||!actorType||!aiType)throw std::runtime_error("The runtime actor classes are not ready");
    TArray<UObject*> objects;UECustom::UObjectGlobals::GetObjectsOfClass(classType,objects,true);
    if(objects.Num()<0||objects.Num()>65536)throw std::runtime_error("The loaded class roster exceeds the safe catalog limit; coverage is incomplete");
    auto rows=nlohmann::json::array();std::set<std::string> seen;
    for(auto row:authored) {
        const auto path=row.value("Class",std::string{}),kind=row.value("Type",std::string{});
        if(path.empty()||(kind!="AI"&&kind!="Resource"&&kind!="NPC"))continue;
        if(kind=="NPC") {
            // Only the live NPC facade supplies authoring permissions, never the disk index.
            rows.push_back(row);continue;
        }
        // Broken definitions remain visible without gaining spawn permission.
        row["Available"]=false;row["Reason"]="Authored class could not be resolved; correct the definition or re-scan";
        try {
            auto* type=ActorHelper::ResolveClass(RC::to_generic_string(path));
            if(type&&((kind=="AI"&&type->IsChildOf(aiType))||(kind=="Resource"&&ToolResourceClass(type,actorType,aiType)))) {
                row["Available"]=!ActorHelper::IsAbstract(type);
                row["Packaged"]=PS::CookedAssets::VerifiedLoadedObject(type);
                const auto snapshot=ToolDefinitionRow(type,row.value("Key",std::string{}),kind=="AI");
                if(snapshot.contains("ResourceFamily"))row["ResourceFamily"]=snapshot["ResourceFamily"];
                row["Reason"]=ActorHelper::IsAbstract(type)?"Abstract base definition; not spawnable":"";
            }
        }catch(...) {}
        rows.push_back(row);seen.insert(kind+path);
    }
    for(auto* object:objects) {
        if(!object||object->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed|RF_NeedLoad|RF_NeedPostLoad)))continue;
        auto* type=static_cast<UClass*>(object);
        const bool npc=npcType&&type->IsChildOf(npcType),ai=!npc&&type->IsChildOf(aiType);if(!npc&&!ai&&!ToolResourceClass(type,actorType,aiType))continue;
        const auto path=RC::to_string(type->GetPathName()),kind=std::string(npc?"NPC":ai?"AI":"Resource");
        if(!path.starts_with('/')||!seen.insert(kind+path).second)continue;
        rows.push_back(ToolDefinitionRow(type,std::string(npc?"@loaded-npc:":ai?"@loaded-ai:":"@loaded-resource:")+path,ai,npc?"NPC":""));
    }
    std::sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){return a.value("Name",std::string{})<b.value("Name",std::string{});});
    return rows;
}
// UE4SS exposes different FAssetData accessors on different host lines. Detect
// supported accessors at compile time rather than guessing the struct's offsets.
template<class Text> std::string ToolNativeString(const Text& value) {
    if constexpr(requires {RC::to_string(value);})return RC::to_string(value);
    else if constexpr(requires {value.GetCharArray();}) {
        const auto& chars=value.GetCharArray();
        if(chars.Num()>0&&chars.Num()<4096&&chars.GetData()&&chars.GetData()[chars.Num()-1]==0)
            return RC::to_string(RC::StringType(chars.GetData(),chars.Num()-1));
    }
    return {};
}
template<class Asset> std::string ToolAssetClass(Asset& asset) {
    if constexpr(requires {asset.AssetClassPath().GetAssetName().ToString();})return ToolNativeString(asset.AssetClassPath().GetAssetName().ToString());
    else if constexpr(requires {asset.AssetClassPath().ToString();})return ToolNativeString(asset.AssetClassPath().ToString());
    else if constexpr(requires {asset.AssetClass().ToString();})return ToolNativeString(asset.AssetClass().ToString());
    else return {};
}
// Public reference requests contain paths only and run off the game thread.
// This job is joined explicitly by the spawn-loader destructor before DLL unload.
PS::F2ReferenceIndex::Job g_f2ReferenceJob;
// Session-wide, intentionally not cleared on world change or ordinary refresh.
// A failed first fetch/save cannot trigger an endless implicit download loop.
bool g_helpyInitialReferenceAttempted=false;
std::optional<nlohmann::json> g_helpyReferenceMemory;
struct ToolCatalogIndex {
    using Kind=PS::F2Catalog::Kind;
    using Candidate=PS::F2Catalog::Candidate;
    bool active=false,finished=false,registryReady=false,fullScan=false,waitingReference=false;
    bool cacheWritable=true,referenceCacheWritable=true,partial=false,stopped=false;
    std::size_t failed=0,unknown=0,unclassified=0,registryRecords=0;
    uint64_t generation=0;
    PS::WeakObjectHandle world;
    PS::F2Catalog::Sources sources;
    PS::F2Catalog::Plan plan;
    std::map<std::string,nlohmann::json> described,itemRows;
    nlohmann::json issues=nlohmann::json::array(),authored=nlohmann::json::array();
    std::string stage="Not indexed",referenceStatus,searchQuery;
    std::chrono::steady_clock::time_point published{};
    std::size_t publishedDone=0;
    void Issue(const std::string& path,const std::string& message) {
        if(issues.size()<2048)issues.push_back({{"Path",path.substr(0,2048)},{"Reason",message.substr(0,512)}});
    }
    void Reset() {
        if(waitingReference)g_f2ReferenceJob.Cancel();
        *this=ToolCatalogIndex{};PS::CookedAssets::Invalidate();
    }
    void Remember(const nlohmann::json& rows) {
        for(const auto& row:rows) {
            const auto key=row.value("Key",std::string{});if(!key.empty())described[key]=row;
        }
    }
    void RememberItems(const nlohmann::json& rows) {
        for(auto row:rows) {
            const auto path=row.value("Path",std::string{});
            if(!PS::F2Catalog::ValidObjectPath(path))continue;
            row["Available"]=row.value("Available",true);itemRows[path]=std::move(row);
        }
    }
    nlohmann::json Definitions(const nlohmann::json& loaded)const {
        auto merged=described;
        for(const auto& row:loaded){const auto key=row.value("Key",std::string{});if(!key.empty())merged[key]=row;}
        auto result=nlohmann::json::array();for(const auto& [key,row]:merged){(void)key;result.push_back(row);}
        std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){
            const auto an=PS::QuickUI::Lower(a.value("Name",std::string{})),bn=PS::QuickUI::Lower(b.value("Name",std::string{}));
            return an==bn?a.value("Key",std::string{})<b.value("Key",std::string{}):an<bn;
        });return result;
    }
    nlohmann::json Items()const {
        auto out=nlohmann::json::array();for(const auto& [path,row]:itemRows){(void)path;out.push_back(row);}
        std::sort(out.begin(),out.end(),[](const auto& a,const auto& b){
            const auto an=PS::QuickUI::Lower(a.value("Name",std::string{})),bn=PS::QuickUI::Lower(b.value("Name",std::string{}));
            return an==bn?a.value("Path",std::string{})<b.value("Path",std::string{}):an<bn;
        });return out;
    }
    void Add(const Candidate& entry) {
        try {plan.Add(entry);}
        catch(const std::exception& e){partial=true;Issue(entry.path,e.what());}
    }
    void LoadEntries(const nlohmann::json& document,bool placeholders=false) {
        std::vector<std::string> errors;
        for(const auto& entry:PS::F2CatalogStore::DecodeEntries(document,errors)) {
            Add(entry);
            if(!placeholders)continue;
            const auto name=entry.name.empty()?PS::QuickDecorations::ReadableAssetName(entry.path):entry.name;
            if(entry.kind==Kind::Item) {
                if(!itemRows.contains(entry.path))itemRows[entry.path]={{"Path",entry.path},{"Name",name},{"Available",false},
                    {"Reason","Cached path; not yet validated in this world"},{"Cooked",false}};
            }else if(entry.kind==Kind::Enemy||entry.kind==Kind::Resource||entry.kind==Kind::Npc) {
                const bool npc=entry.kind==Kind::Npc,ai=entry.kind==Kind::Enemy;const auto key=std::string(npc?"@loaded-npc:":ai?"@loaded-ai:":"@loaded-resource:")+entry.path;
                if(!described.contains(key))described[key]={{"Key",key},{"Name",name},{"Class",entry.path},{"Type",npc?"NPC":ai?"AI":"Resource"},
                    {"Available",false},{"Loaded",false},{"Reason","Cached path; not yet validated in this world"}};
            }
        }
        for(const auto& error:errors){partial=true;Issue("Local path index",error);}
    }
    std::optional<nlohmann::json> ReadSavedCache(const std::filesystem::path& current,const std::filesystem::path& legacy) {
        bool migrated=false;
        auto document=PS::F2CatalogStore::ReadCacheWithLegacy(current,legacy,migrated);
        if(migrated)PS::Log<LogLevel::Normal>(
            STR("RuneSchema Helpy: imported saved data: {}.\n"),
            RC::to_generic_string(current.string()));
        return document;
    }
    int LoadSettings(bool diagnosticCatalogs) {
        int catalogCacheState=0; // 0 absent, 1 path-only legacy cache, 2 display metadata cache
        const auto references=PS::HostServices::ReferencesDirectory();
        const auto path=references/"Helpy-catalog-sources.json";
        try {
            if(auto doc=PS::F2CatalogStore::Read(path))sources=PS::F2CatalogStore::DecodeSources(*doc);
            else PS::F2CatalogStore::Write(path,PS::F2CatalogStore::EncodeSources(sources));
        }catch(const std::exception& e){sources.onlineReference=false;partial=true;Issue(path.string(),e.what());}
        const auto cache=PS::HostServices::CacheDirectory()/"Helpy-catalog-cache.json";
        try {if(diagnosticCatalogs)if(auto doc=ReadSavedCache(cache,PS::HostServices::SettingsDirectory()/"F2-catalog-cache.json")){
            LoadEntries(*doc,true);catalogCacheState=1;
            if(doc->value("MetadataVersion",0)==1&&doc->contains("Items")&&(*doc)["Items"].is_array()
                &&doc->contains("Definitions")&&(*doc)["Definitions"].is_array()) {
                RememberItems((*doc)["Items"]);Remember((*doc)["Definitions"]);catalogCacheState=2;
            }
        }}
        catch(const std::exception& e){cacheWritable=false;partial=true;Issue(cache.string(),e.what());}
        // The universal index accepts Items/Enemies/Resources. Legacy resource-only file is also read.
        for(const auto* file:{"Helpy-catalog-index.json","Helpy-resource-index.json"}) {
            const auto user=references/file;
            try {
                if(auto doc=PS::F2CatalogStore::Read(user))LoadEntries(*doc);
                else if(std::string_view(file)=="Helpy-catalog-index.json")
                    PS::F2CatalogStore::Write(user,{{"Version",1},{"Entries",nlohmann::json::array()}});
            }catch(const std::exception& e){partial=true;Issue(user.string(),e.what());}
        }
        return catalogCacheState;
    }
    void SaveCache() {
        // Catalog snapshots are an explicit advanced diagnostic artifact. The
        // normal Helpy browser is rebuilt from the mounted registry and live
        // objects every session and never depends on this file.
        if(!cacheWritable||!fullScan)return;
        try {
            // Persist bounded JSON display metadata plus path hints. No UObject
            // addresses are stored; mutation backends still resolve and validate
            // the selected path in the current world before acting.
            PS::F2CatalogStore::Write(PS::HostServices::CacheDirectory()/"Helpy-catalog-cache.json",
                {{"Version",1},{"MetadataVersion",1},{"CheckedAt",PS::F2CatalogStore::Now()},
                 {"Entries",PS::F2CatalogStore::EncodeEntries(plan.entries)},{"Items",Items()},
                 {"Definitions",Definitions(nlohmann::json::array())}});
        }catch(const std::exception& e){cacheWritable=false;partial=true;Issue("Helpy-catalog-cache.json",e.what());}
    }
    void Describe(UClass* type) {
        auto* actorType=ActorHelper::ResolveClass(TEXT("/Script/Engine.Actor"));
        auto* aiType=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionAICharacter"));
        if(!type||!actorType||!aiType)throw std::runtime_error("Native actor classes unavailable");
        const bool npc=HelpyNpcGuards::IsNpc(type),ai=!npc&&type->IsChildOf(aiType);
        if(!npc&&!ai&&!ToolResourceClass(type,actorType,aiType)) {
            ++unclassified;const auto path=RC::to_string(type->GetPathName());
            described.erase("@loaded-ai:"+path);described.erase("@loaded-resource:"+path);described.erase("@loaded-npc:"+path);
            return;
        }
        const auto path=RC::to_string(type->GetPathName());if(!PS::F2Catalog::ValidObjectPath(path))return;
        // Remove any old cached *category*, never an authored definition with its own Key.
        for(const auto* prefix:{"@loaded-ai:","@loaded-resource:","@loaded-npc:"})described.erase(std::string(prefix)+path);
        const auto key=std::string(npc?"@loaded-npc:":ai?"@loaded-ai:":"@loaded-resource:")+path;
        auto row=ToolDefinitionRow(type,key,ai,npc?"NPC":"");described[key]=std::move(row);
    }
    void Prioritize(const std::string& query) {
        if(stopped||generation!=PS::SpawnToolRequests::Generation.load()||!world.Get()||query.size()>2048)return;
        searchQuery=query;
        if(query.size()<2){plan.Prioritize({});return;}
        // An exact cooked object path can be searched even without a registry or seed entry.
        if(PS::F2Catalog::ValidObjectPath(query))Add({query,{},query.ends_with("_C")?Kind::ClassCandidate:Kind::Item});
        plan.Prioritize(query);
        if(!plan.Empty()&&!active){active=true;finished=false;stage="Loading search matches";}
    }
    nlohmann::json Snapshot(const std::string& message,bool includeLists=false) {
        std::size_t ai=0,resources=0,npcs=0;
        for(const auto& [key,row]:described){(void)key;if(row.value("Type",std::string{})=="AI")++ai;else if(row.value("Type",std::string{})=="NPC")++npcs;else ++resources;}
        nlohmann::json result={{"CatalogReady",true},{"_CatalogIndexing",active},{"CatalogStatus",message},
            {"RegistryRecords",registryRecords},{"UnresolvedAssets",failed},{"MetadataUnavailable",unknown},{"UnclassifiedRecords",unclassified},
            {"CatalogAI",ai},{"CatalogNPCs",npcs},{"CatalogResources",resources},{"IndexStage",stage},{"IndexDone",plan.Done()},{"IndexTotal",plan.Total()},
            {"IndexHasTotal",!waitingReference},{"IndexFinished",finished},{"IndexDetail",referenceStatus},
            {"CatalogCoverage",registryReady?fullScan?"Mounted registry candidates; classification/load failures listed below":"Targeted mounted registry plus known/cached paths":"Exact-path reference/cache fallback; not a complete census of installed content"}};
        if(includeLists) {
            result["Definitions"]=Definitions(nlohmann::json::array());result["Items"]=Items();
            result["CookedVisuals"]=PS::CookedAssets::VisualRoster();result["CatalogIssues"]=issues;
        }
        return result;
    }
    void ScanRegistry() {
        if(!bFAssetDataAvailable){referenceStatus="Host registry layout unavailable; validating exact-path hints.";return;}
        TArray<FAssetData> assets;
        auto interface=UAssetRegistryHelpers::GetAssetRegistry();auto* registry=static_cast<UAssetRegistry*>(interface.ObjectPointer);
        if(!registry||!registry->GetAllAssets(assets,true)||assets.Num()<=0||assets.Num()>262144)
            throw std::runtime_error("Mounted registry unavailable or outside the safe bound");
        PS::CookedAssets::Build(assets);registryReady=true;registryRecords=static_cast<std::size_t>(assets.Num());
        for(auto& asset:assets) {
            try {
                const auto package=ToolNativeString(asset.PackageName().ToString()),name=ToolNativeString(asset.AssetName().ToString());
                std::string type;try{type=ToolAssetClass(asset);}catch(...){}
                if(PS::QuickCatalogRules::MissingClassMetadata(type))++unknown;
                if(name.starts_with("ITEM_")&&name.find("MeshData")==name.npos) {Add({package+"."+name,name,Kind::Item});continue;}
                if(!PS::QuickCatalogRules::BlueprintCandidate(name,type))continue;
                const auto path=PS::QuickCatalogRules::GeneratedClassPath(package,name);
                const bool resource=PS::F2Catalog::UnderAny(package,sources.resources),enemy=PS::F2Catalog::UnderAny(package,sources.enemies);
                if(fullScan||resource||enemy)Add({path,name,resource?Kind::Resource:enemy?(PS::QuickUI::Lower(package).find("/gameplay/npcs/")!=std::string::npos?Kind::Npc:Kind::Enemy):Kind::ClassCandidate});
            }catch(const std::exception& e){partial=true;++unclassified;Issue("Registry record",e.what());}
        }
        referenceStatus="Using mounted game registry; no online path request required.";
    }
    void Start(UWorld* current,nlohmann::json definitions,nlohmann::json recipients,bool full=false,bool updateReference=false) {
        if(!current)throw std::runtime_error("Load a world before indexing game assets");
        Reset();fullScan=full;world.Assign(current);generation=PS::SpawnToolRequests::Generation.load();authored=std::move(definitions);
        active=true;stage="Preparing index";PS::SpawnToolRequests::CancelRequested.store(false);
        (void)LoadSettings(full);
        for(const auto& value:PS::F2Catalog::BundledPaths()) {
            if(value.kind==Kind::Item||PS::F2Catalog::UnderAny(value.path,(value.kind==Kind::Enemy||value.kind==Kind::Npc)?sources.enemies:sources.resources))Add(value);
        }
        const auto loaded=ToolLiveDefinitions(authored);Remember(loaded);RememberItems(PS::AssetSearch::ItemRoster());
        for(const auto& row:loaded)if(row.value("Available",false)&&PS::F2Catalog::ValidObjectPath(row.value("Class",std::string{})))
            Add({row.value("Class",std::string{}),row.value("Name",std::string{}),row.value("Type",std::string{})=="NPC"?Kind::Npc:row.value("Type",std::string{})=="AI"?Kind::Enemy:Kind::Resource});
        for(const auto& [path,row]:itemRows)if(row.value("Available",false)&&!row.value("RuntimeClone",false))
            Add({path,row.value("Name",std::string{}),Kind::Item});
        // Runtime truth always wins: enumerate the mounted registry on every
        // ordinary refresh. A diagnostic snapshot may seed display rows but it
        // can never short-circuit or replace this scan.
        try {ScanRegistry();}catch(const std::exception& e){partial=true;Issue("Mounted registry",e.what());}
        bool fresh=false,referencePresent=false;
        const auto referenceFile=PS::HostServices::ReferencesDirectory()/"Helpy-reference-index.json";
        if(!registryReady||updateReference) {
            try {
                auto cached=ReadSavedCache(referenceFile,PS::HostServices::SettingsDirectory()/"F2-reference-index.json");
                if(!cached&&g_helpyReferenceMemory)cached=g_helpyReferenceMemory;
                if(cached) {
                    referencePresent=true;fresh=PS::F2CatalogStore::ReferenceFresh(*cached,sources);
                    if(cached->value("Dataset",std::string{})==sources.dataset)LoadEntries(*cached);
                    if(!fresh)Issue(referenceFile.string(),"Discovery settings changed. Existing cache retained; use Helpy Settings > Update RSDW to replace it.");
                }
            }catch(const std::exception& e){referencePresent=true;referenceCacheWritable=false;partial=true;Issue(referenceFile.string(),e.what());}
            if(g_f2ReferenceJob.Poll()){} // discard any cancelled earlier result
            const bool fetch=PS::HelpyReferencePolicy::ShouldFetch(sources.onlineReference,referencePresent,
                referenceCacheWritable,g_helpyInitialReferenceAttempted,updateReference);
            if(fetch) {
                waitingReference=g_f2ReferenceJob.Begin(sources);
                if(waitingReference)g_helpyInitialReferenceAttempted=true;
                referenceStatus=waitingReference?"Downloading RSDW reference paths once; future refreshes use the saved index.":"Previous reference request is stopping; refresh later.";
            }else referenceStatus=fresh?"Using saved RSDW reference index (no download); validating local candidates.":
                referencePresent?"Using retained reference cache; Update RSDW is manual.":
                sources.onlineReference?"No reference index saved; automatic attempt already used. Update RSDW retries explicitly.":
                "Online reference requests disabled; using local paths and loaded assets.";
        }
        stage=waitingReference?"Discovering paths":"Caching known paths";
        auto result=Snapshot(referenceStatus,true);result["Players"]=std::move(recipients);
        result["Status"]="Refresh started: indexing items, NPCs, AI and resources. No spawn or grant is performed.";
        PS::SpawnToolRequests::CatalogProgress(result);PS::SpawnToolRequests::Publish(std::move(result));
    }
    void Stop(const std::string& reason) noexcept {
        stopped=true;active=false;finished=false;if(waitingReference)g_f2ReferenceJob.Cancel();waitingReference=false;stage="Index stopped";
        try{SaveCache();PS::SpawnToolRequests::CatalogProgress(Snapshot(reason,true));}catch(...){}
    }
    bool Tick() {
        if(generation&&generation!=PS::SpawnToolRequests::Generation.load()){Reset();return false;}
        try {
            if(auto reference=g_f2ReferenceJob.Poll()) {
                if(active&&waitingReference) {
                    waitingReference=false;
                    if(reference->error.empty()) {
                        for(const auto& entry:reference->entries)Add(entry);
                        plan.Prioritize(searchQuery);
                        referenceStatus="RSDW path index received; candidates still require native type validation.";
                        g_helpyReferenceMemory=nlohmann::json{{"Version",1},{"Dataset",sources.dataset},{"Reference",sources.reference},
                            {"ReferenceTree",reference->revision},{"SourceSettings",PS::F2CatalogStore::EncodeSources(sources)},
                            {"FetchedAt",PS::F2CatalogStore::Now()},{"Entries",PS::F2CatalogStore::EncodeEntries(reference->entries)}};
                        try {
                            if(!referenceCacheWritable)
                                throw std::runtime_error("Reference cache is protected after a read/migration error; repair with the game closed");
                            PS::F2CatalogStore::Write(PS::HostServices::ReferencesDirectory()/"Helpy-reference-index.json",*g_helpyReferenceMemory);
                        }catch(const std::exception& e){partial=true;Issue("Reference cache",e.what());}
                    }else {partial=true;referenceStatus=reference->error;Issue("RSDW reference index",reference->error);}
                }
            }
            if(!active)return false;
            auto* current=world.Get();
            if(!current||current->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed))) {Stop("World expired; refresh in the current world");return false;}
            if(PS::SpawnToolRequests::CancelRequested.exchange(false)){Stop("Index stopped; discovered paths retained");return false;}
            const auto start=std::chrono::steady_clock::now();std::size_t attempted=0;
            while(attempted++<4) {
                auto next=plan.Pop();if(!next)break;
                stage=std::string("Caching ")+(next->kind==Kind::Item?"items":next->kind==Kind::Resource?"resources":next->kind==Kind::Npc?"NPCs":next->kind==Kind::Enemy?"AI":"class candidates");
                try {
                    if(next->kind==Kind::Item) {
                        auto* object=ActorHelper::ResolveObject(RC::to_generic_string(next->path));
                        auto* itemType=ActorHelper::ResolveClass(TEXT("/Script/Dominion.ItemData"));
                        if(!object||!itemType||!object->IsA(itemType))throw std::runtime_error("Not an available ItemData asset in this installation");
                        auto record=PS::AssetSearch::DescribeItem(object);
                        if(!record.is_object())throw std::runtime_error("Item not ready for a catalogue snapshot");
                        const auto actualPath=record.at("Path").get<std::string>();
                        itemRows[actualPath]=std::move(record);
                    }else {
                        auto* type=ActorHelper::ResolveClass(RC::to_generic_string(next->path));
                        if(!type)throw std::runtime_error("Class not available in this installation");
                        Describe(type);
                    }
                }catch(const std::exception& e){
                    ++failed;Issue(next->path,e.what());
                    if(auto found=itemRows.find(next->path);found!=itemRows.end()&&!found->second.value("Available",false))
                        found->second["Reason"]=std::string(e.what()).substr(0,512);
                    for(const auto* prefix:{"@loaded-ai:","@loaded-resource:","@loaded-npc:"})
                        if(auto found=described.find(std::string(prefix)+next->path);found!=described.end()&&!found->second.value("Available",false))
                            found->second["Reason"]=std::string(e.what()).substr(0,512);
                }
                catch(...){++failed;Issue(next->path,"Unknown asset-load/classification error");}
                // A single native blocking load can exceed this budget; never load off the game thread.
                if(std::chrono::steady_clock::now()-start>std::chrono::milliseconds(2))break;
            }
            if(plan.Empty()&&!waitingReference) {
                RememberItems(PS::AssetSearch::ItemRoster());Remember(ToolLiveDefinitions(authored));
                active=false;finished=true;stage=partial||failed?"Known paths checked (partial)":"Known paths checked";
                SaveCache();if(!cacheWritable)stage="Indexed; cache not saved";
                auto result=Snapshot(referenceStatus,true);
                result["CatalogStatus"]=std::format("{} candidates checked; {} unavailable, {} non-AI/resource candidates. {}",plan.Done(),failed,unclassified,referenceStatus);
                PS::SpawnToolRequests::CatalogProgress(result);
                PS::Log<LogLevel::Normal>(STR("RuneSchema F2 index: {}\n"),RC::to_generic_string(result["CatalogStatus"].get<std::string>()));return false;
            }
            if(plan.Empty())stage="Discovering RSDW paths";
            const auto now=std::chrono::steady_clock::now();
            if(now-published>=std::chrono::milliseconds(125)) {
                const bool lists=plan.Done()-publishedDone>=32;
                PS::SpawnToolRequests::CatalogProgress(Snapshot(referenceStatus,lists));published=now;
                if(lists)publishedDone=plan.Done();
            }
        }catch(const std::exception& e){Stop(std::string("Index stopped: ")+e.what());}
        catch(...){Stop("Index stopped after an unexpected error");}
        return active;
    }
};
ToolCatalogIndex g_quickCatalogIndex;
} // namespace
