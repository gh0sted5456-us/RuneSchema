// Game-thread work only; never creates a gameplay world or touches a save.
namespace {
PS::WeakObjectHandle MenuIconWorldCache;
std::chrono::steady_clock::time_point NextMenuIconWorldScan{};
UWorld* MenuIconWorld(UWorld* ready) {
    if(ready)return ready;
    const auto now=std::chrono::steady_clock::now();
    if(now<NextMenuIconWorldScan){auto* cached=MenuIconWorldCache.Get();return cached&&!cached->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed))?static_cast<UWorld*>(cached):nullptr;}
    NextMenuIconWorldScan=now+std::chrono::milliseconds(250);MenuIconWorldCache.Reset();
    auto* type=ActorHelper::ResolveClass(TEXT("/Script/Engine.World"));if(!type)return nullptr;
    TArray<UObject*> worlds;UECustom::UObjectGlobals::GetObjectsOfClass(type,worlds,true);
    if(worlds.Num()<0||worlds.Num()>128)return nullptr;UWorld* found=nullptr;
    for(auto* object:worlds) {
        if(!object||object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)))continue;
        if(!ActorHelper::GetObjectRef(object,TEXT("OwningGameInstance"))||!ActorHelper::GetObjectRef(object,TEXT("PersistentLevel")))continue;
        if(found)return nullptr; // Travel/ambiguous worlds wait, never select first.
        found=static_cast<UWorld*>(object);
    }MenuIconWorldCache.Assign(found);return found;
}
struct BulkIconCacheJob {
    using Clock=std::chrono::steady_clock;
    struct Icon {std::string Path;std::set<std::string> Origins;};
    bool Active=false,Capturing=false,Discovered=false,Prepared=false;
    std::string Source,Stage="Idle",Error;size_t ObjectAt=0,ObjectTotal=0,MetadataAt=0,At=0,Hits=0,Captured=0,Failed=0;
    std::vector<PS::CookedAssets::Record> Metadata;
    std::map<std::string,std::set<std::string>> Found;std::vector<Icon> Icons;
    std::map<std::string,std::string> FrozenKeys;
    size_t DiscoveryFailed=0;
    nlohmann::json Issues=nlohmann::json::array();
    Clock::time_point Began{},NextPublish{},WaitingSince{};
    void Publish(bool force=false) {
        const auto now=Clock::now();if(!force&&now<NextPublish)return;NextPublish=now+std::chrono::milliseconds(200);
        const auto choices=PS::ContentSource::Choices(PS::HelpyCacheSources::Snapshot());
        PS::IconCacheBuild::Publish({{"Available",true},{"Active",Active},{"Stage",Stage},{"Source",Source},
            {"ScanDone",ObjectAt+MetadataAt},{"ScanTotal",ObjectTotal+Metadata.size()},{"Total",Icons.size()},
            {"Done",At},{"CacheHits",Hits},{"Captured",Captured},{"Failed",Failed},{"Error",Error},{"Sources",choices},
            {"DiscoveryFailed",DiscoveryFailed},{"Issues",Issues},
            {"Coverage","Registered loaded data assets, mounted item/building icon references and UI/icon textures; not all art textures."}});
    }
    void Stop(const std::string& reason) {
        if(Capturing)PS::ItemIconThumbnail::Reset();Active=Capturing=false;Stage="Stopped";Error=reason;Publish(true);
        Metadata.clear();Found.clear();Icons.clear();FrozenKeys.clear();
    }
    void Add(const std::string& icon,const std::set<std::string>& origins) {
        if(!PS::F2Catalog::ValidObjectPath(icon)||origins.empty()||(!Source.empty()&&!origins.contains(Source)))return;
        if(Found.size()>=32768&&!Found.contains(icon))throw std::runtime_error("More than 32,768 icons; select a smaller source.");
        auto& values=Found[icon];values.insert(origins.begin(),origins.end());
    }
    void Tick(UWorld* world,bool rendererFree) {
        if(const auto start=PS::IconCacheBuild::Take()) {
            Source=*start;Active=true;Capturing=Discovered=Prepared=false;Stage="Waiting for registered sources";Error.clear();
            ObjectAt=ObjectTotal=MetadataAt=At=Hits=Captured=Failed=0;Metadata.clear();Found.clear();Icons.clear();FrozenKeys.clear();Began=WaitingSince=Clock::now();NextPublish={};
            DiscoveryFailed=0;Issues=nlohmann::json::array();
            PS::HelpySourceCache::Requested=true;
        }
        if(!Active)return;
        try {
            if(PS::IconCacheBuild::Cancelled()){Stop("Cancelled; completed thumbnails remain cached.");return;}
            if(PS::HelpyRuntimePreferences::IconCacheLimitMB.load()<=0)throw std::runtime_error("Enable Keep icons between launches before building a cache.");
            auto plan=PS::HelpyCacheSources::Snapshot();
            if(!PS::HelpySourceCache::Ready()||!plan.Authoritative||plan.Registered.empty()) {
                if(Clock::now()-WaitingSince>std::chrono::seconds(60))throw std::runtime_error("Registered source identities are not ready; retry after startup completes.");
                Stage="Waiting for registered sources";Publish();return;
            }
            if(!Prepared) {
                const auto choices=PS::ContentSource::Choices(plan);
                if(!Source.empty()&&!choices.contains(Source))throw std::runtime_error("The selected source is not currently registered.");
                if(!PS::CookedAssets::TryEnsure()) {
                    if(Clock::now()-WaitingSince>std::chrono::seconds(60))throw std::runtime_error("Mounted asset registry did not become ready; no empty cache was declared complete.");
                    Stage="Waiting for mounted asset registry";Publish();return;
                }
                Metadata=PS::CookedAssets::MetadataSnapshot();
                const auto count=FUObjectArray::GetNumElements();if(count<0||count>4'000'000)throw std::runtime_error("Loaded-object discovery exceeds its bound.");
                ObjectTotal=static_cast<size_t>(count);Prepared=true;Stage="Discovering registered icons";
            }
            const auto deadline=Clock::now()+std::chrono::milliseconds(2);
            auto* dataType=ActorHelper::ResolveClass(TEXT("/Script/Engine.DataAsset"));
            if(!dataType)throw std::runtime_error("Native DataAsset class is not ready; retry at the main menu.");
            while(!Discovered&&Clock::now()<deadline) {
                if(ObjectAt<ObjectTotal) {
                    auto* slot=FUObjectArray::IndexToObject(static_cast<int32>(ObjectAt++));auto* object=slot?slot->GetUObject():nullptr;
                    if(!object||!dataType||!object->IsA(dataType)||object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed|RF_NeedLoad|RF_NeedPostLoad)))continue;
                    const auto asset=RC::to_string(object->GetPathName());if(!plan.Registered.contains(PS::HelpyCacheSources::Source(asset)))continue;
                    const auto provenance=PS::AssetProvenance::Lookup(object);
                    if(provenance.is_object()&&(!provenance.value("Registered",false)||provenance.value("LastCloneDefinitionErrors",0)!=0))continue;
                    const auto origins=PS::ContentSource::Origins(asset,plan,provenance.is_object()?provenance.value("CreatingMod",std::string{}):std::string{});
                    if(!Source.empty()&&!origins.contains(Source))continue;
                    for(const auto& icon:PS::AssetSearch::IconReferences(object))Add(icon,origins);
                }else if(MetadataAt<Metadata.size()) {
                    const auto& record=Metadata[MetadataAt++];if(!plan.Registered.contains(PS::HelpyCacheSources::Source(record.path)))continue;
                    const auto origins=PS::ContentSource::Origins(record.path,plan);
                    if(!Source.empty()&&!origins.contains(Source))continue;
                    if(PS::ContentSource::UiTexture(record.path,record.type)){Add(record.path,origins);continue;}
                    const auto item=PS::F2Catalog::MountedCandidate(record.path,record.name,record.type);
                    const auto type=PS::HelpyCacheSources::Lower(record.type);
                    if((!item||item->kind!=PS::F2Catalog::Kind::Item)&&!type.ends_with("buildingpiecedata"))continue;
                    // An unloaded mod item may reference an icon outside a UI/
                    // folder. Load only these registered data-asset candidates,
                    // one bounded game-thread slice at a time, not every texture
                    // or Blueprint. Discovery never grants/spawns anything.
                    try {
                        auto* asset=ActorHelper::ResolveObject(RC::to_generic_string(record.path));
                        if(!asset||!asset->IsA(dataType))throw std::runtime_error("Registered icon owner did not resolve to a DataAsset");
                        for(const auto& icon:PS::AssetSearch::IconReferences(asset))Add(icon,origins);
                    }catch(const std::exception& e){++DiscoveryFailed;Error=std::string(e.what()).substr(0,1024);if(Issues.size()<64)Issues.push_back({{"Path",record.path},{"Reason",Error}});}
                }else {
                    for(const auto& [path,origins]:Found)Icons.push_back({path,origins});Found.clear();Discovered=true;Stage="Waiting for source fingerprints";WaitingSince=Clock::now();
                    const uint64_t needed=uint64_t(Icons.size())*(16+64*64*4);
                    if(needed>uint64_t(PS::HelpyRuntimePreferences::IconCacheLimitMB.load())*1024*1024)throw std::runtime_error("Discovered icons exceed the cache limit. Increase it or select one source.");
                }
            }
            if(!Discovered){Publish();return;}
            if(uint64_t(Icons.size())*(16+64*64*4)>uint64_t(PS::HelpyRuntimePreferences::IconCacheLimitMB.load())*1024*1024)throw std::runtime_error("Cache limit changed below this batch's requirement; select a smaller source or increase the limit.");
            plan=PS::HelpyCacheSources::Snapshot();
            if(PS::HelpySourceCache::Current.PlanRevision!=plan.Revision) {
                if(Clock::now()-WaitingSince>std::chrono::seconds(60))throw std::runtime_error("Source fingerprints did not settle; retry when content registration is stable.");
                Stage="Waiting for source fingerprints";Publish();return;
            }
            if(FrozenKeys.empty())FrozenKeys=PS::HelpySourceCache::Current.SourceKeys;
            else if(FrozenKeys!=PS::HelpySourceCache::Current.SourceKeys)throw std::runtime_error("Registered content changed during capture; restart the cache build.");
            if(At==Icons.size()){Active=Capturing=false;Stage=Failed||DiscoveryFailed?"Completed with failures":"Complete";Publish(true);Metadata.clear();Icons.clear();FrozenKeys.clear();return;}
            if(!world){if(Clock::now()-WaitingSince>std::chrono::seconds(60))throw std::runtime_error("No stable menu/world renderer is available. No gameplay world was opened.");Stage="Waiting for menu renderer";Publish();return;}
            WaitingSince=Clock::now();Stage="Caching registered icons";
            if(!rendererFree&&!Capturing){Publish();return;}
            const auto& icon=Icons[At];
            try {
                if(PS::HelpySourceCache::Fingerprint(icon.Path).empty())throw std::runtime_error("Icon source is not registered");
                const auto result=PS::ItemIconThumbnail::Render(world,icon.Path);Capturing=result.value("Status",std::string{})=="Pending";
                if(Capturing){Publish();return;}
                if(result.value("Status",std::string{})!="Ready")throw std::runtime_error("Icon capture did not return pixels");
                const auto file=std::filesystem::path(result.at("File").get<std::string>());auto origins=icon.Origins;
                try{const auto old=nlohmann::json::parse(PS::ConfigFiles::Read(file.wstring()+L".json"));if(old.value("Icon",std::string{})==icon.Path&&old.contains("Origins")&&old["Origins"].is_array())for(const auto& value:old["Origins"])if(value.is_string()&&PS::ContentSource::Valid(value.get<std::string>()))origins.insert(value.get<std::string>());}catch(...){}
                PS::ConfigFiles::Write(file.wstring()+L".json",nlohmann::json{{"Icon",icon.Path},{"Origins",origins}}.dump());
                if(result.value("CacheHit",false))++Hits;else ++Captured;
            }catch(const std::exception& error){PS::ItemIconThumbnail::Reset();Capturing=false;++Failed;Error=std::string(error.what()).substr(0,1024);}
            ++At;Publish();
        }catch(const std::exception& error){Stop(error.what());}
    }
};
BulkIconCacheJob BulkIconJob;
}
