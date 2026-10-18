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
    bool Active=false,Capturing=false,Discovered=false,Prepared=false,ItemsDiscovered=false;
    std::string Source,Stage="Idle",Error;size_t ObjectAt=0,ObjectTotal=0,MetadataAt=0,At=0,Hits=0,Captured=0,Failed=0;
    std::vector<PS::CookedAssets::Record> Metadata;
    size_t MetadataTotal=0,MetadataWindowAt=0;uint64_t MetadataRevision=0;
    std::map<std::string,std::set<std::string>> Found;std::vector<Icon> Icons;
    std::map<std::string,std::string> FrozenKeys;
    size_t DiscoveryFailed=0;
    size_t ItemOffset=0,ItemRows=0,Verified=0,VerifyAt=0;
    size_t Retries=0;unsigned Attempt=0;Clock::time_point RetryAt{};
    std::string ItemCensusKey;
    nlohmann::json ItemPage=nlohmann::json::array();size_t ItemPageAt=0;bool ItemPageLast=false;
    nlohmann::json Issues=nlohmann::json::array();
    Clock::time_point Began{},NextPublish{},WaitingSince{};
    void Publish(bool force=false) {
        const auto now=Clock::now();if(!force&&now<NextPublish)return;NextPublish=now+std::chrono::milliseconds(200);
        const auto choices=PS::ContentSource::Choices(PS::HelpyCacheSources::Snapshot());
        PS::IconCacheBuild::Publish({{"Available",true},{"Active",Active},{"Stage",Stage},{"Source",Source},
            {"ScanDone",ObjectAt+MetadataAt},{"ScanTotal",ObjectTotal+MetadataTotal},{"Total",Icons.size()},
            {"Done",At},{"CacheHits",Hits},{"Captured",Captured},{"Failed",Failed},{"Error",Error},{"Sources",choices},
            {"DiscoveryFailed",DiscoveryFailed},{"Verified",Verified},{"VerifyDone",VerifyAt},{"Retries",Retries},{"ItemRows",ItemRows},{"RequiredMB",(uint64_t(Icons.size())*16400+1048575)/1048576},{"Issues",Issues.size()>64?nlohmann::json(Issues.begin(),Issues.begin()+64):Issues},
            {"Coverage","Registered loaded data assets, mounted item/building icon references and UI/icon textures; not all art textures."}});
    }
    void Issue(const std::string& path,const std::string& phase,const std::string& reason){
        Error=reason.substr(0,1024);if(Issues.size()<32768)Issues.push_back({{"Path",path},{"Phase",phase},{"Reason",Error}});
    }
    void Report(){
        nlohmann::json expected=nlohmann::json::array();for(const auto& icon:Icons)expected.push_back(icon.Path);
        PS::IconCacheBuild::SetManifest({{"Expected",std::move(expected)},{"SourceKeys",FrozenKeys}});
        Publish(true);
        try{auto report=PS::IconCacheBuild::Read(true);report["Issues"]=Issues;
            const auto path=PS::HelpySourceCache::Directory(false).parent_path()/"icon-build-report.json";
            std::filesystem::create_directories(path.parent_path());PS::ConfigFiles::Write(path,report.dump(2));
        }catch(...){} // A report write failure cannot certify missing pixels.
    }
    void Stop(const std::string& reason) {
        if(Capturing)PS::ItemIconThumbnail::Reset();Active=Capturing=false;Stage="Stopped";Error=reason;Report();
        Metadata.clear();Found.clear();Icons.clear();FrozenKeys.clear();
    }
    void Add(const std::string& icon,const std::set<std::string>& origins) {
        if(!PS::F2Catalog::ValidObjectPath(icon)||origins.empty()||(!Source.empty()&&!origins.contains(Source)))return;
        if(Found.size()>=32768&&!Found.contains(icon))throw std::runtime_error("More than 32,768 icons; select a smaller source.");
        auto& values=Found[icon];values.insert(origins.begin(),origins.end());
    }
    void Index(const nlohmann::json& result,const Icon& icon) {
        const auto file=std::filesystem::path(result.at("File").get<std::string>());
        auto origins=icon.Origins;nlohmann::json old;
        try{if(std::filesystem::file_size(file.wstring()+L".json")<=8192)old=nlohmann::json::parse(PS::ConfigFiles::Read(file.wstring()+L".json"));
            if(old.value("Icon",std::string{})==icon.Path&&old.contains("Origins")&&old["Origins"].is_array())for(const auto& value:old["Origins"])if(value.is_string()&&PS::ContentSource::Valid(value.get<std::string>()))origins.insert(value.get<std::string>());
        }catch(...){old=nlohmann::json::object();}
        bool displayOnly=false;try{displayOnly=result.value("CacheHit",false)&&old.is_object()&&old.value("Icon",std::string{})==icon.Path&&old.value("DisplayOnly",false);}catch(...){}
        const nlohmann::json next={{"Icon",icon.Path},{"Origins",origins},{"DisplayOnly",displayOnly}};
        if(old!=next)PS::ConfigFiles::Write(file.wstring()+L".json",next.dump());
    }
    void Tick(UWorld* world,bool rendererFree) {
        if(const auto start=PS::IconCacheBuild::Take()) {
            Source=*start;Active=true;Capturing=Discovered=Prepared=ItemsDiscovered=false;Stage="Waiting for registered sources";Error.clear();
            ObjectAt=ObjectTotal=MetadataAt=At=Hits=Captured=Failed=0;Metadata.clear();Found.clear();Icons.clear();FrozenKeys.clear();Began=WaitingSince=Clock::now();NextPublish={};
            MetadataTotal=MetadataWindowAt=MetadataRevision=0;
            DiscoveryFailed=0;Issues=nlohmann::json::array();
            ItemOffset=ItemRows=Verified=VerifyAt=0;
            Retries=Attempt=0;RetryAt={};
            ItemPage=nlohmann::json::array();ItemPageAt=0;ItemPageLast=false;
            ItemCensusKey.clear();
            PS::HelpySourceCache::Requested=true;
        }
        if(!Active)return;
        try {
            if(PS::IconCacheBuild::Cancelled()){Stop("Cancelled; completed thumbnails remain cached.");return;}
            const auto limitMB=PS::ItemIconThumbnail::CacheLimitMB();
            if(limitMB<=0)throw std::runtime_error("Enable Keep icons between launches before building a cache.");
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
                const auto metadata=PS::CookedAssets::MetadataWindow(0,0);
                MetadataTotal=metadata.Total;MetadataRevision=metadata.Revision;MetadataWindowAt=0;Metadata.clear();
                const auto count=FUObjectArray::GetNumElements();if(count<0||count>4'000'000)throw std::runtime_error("Loaded-object discovery exceeds its bound.");
                ObjectTotal=static_cast<size_t>(count);Prepared=true;Stage="Discovering registered icons";
            }
            const auto deadline=Clock::now()+std::chrono::milliseconds(2);
            // Seed from the same complete native catalogue used by Item Bay.
            // Heuristics over DataAsset field names alone miss projected/clone
            // icons. ItemWindow's bounded class/metadata cursor never grants an
            // item and does not trust an imported cache as a native registry.
            if(!ItemsDiscovered){
                Stage="Discovering item icons";
                const auto census=PS::HelpySourceCache::PublishedFingerprint();
                if(ItemCensusKey!=census){ItemCensusKey=census;ItemOffset=ItemRows=ItemPageAt=0;ItemPage=nlohmann::json::array();ItemPageLast=false;}
                if(ItemPageAt>=ItemPage.size()){
                    const auto page=PS::AssetSearch::ItemWindow({},ItemOffset,128,0,false,"BasicRow",4096,1800,true);
                    ItemPage=page.at("Rows");ItemPageAt=0;ItemOffset=page.value("NextOffset",ItemOffset);ItemPageLast=!page.value("HasMore",false);
                    if(ItemPageLast&&!page.value("MountedMetadataReady",false))throw std::runtime_error("Mounted item metadata is incomplete; no full icon pack was exported. Retry after registration.");
                }
                while(ItemPageAt<ItemPage.size()&&Clock::now()<deadline){
                    const auto& row=ItemPage[ItemPageAt++];++ItemRows;const auto asset=row.value("Path",std::string{}),icon=row.value("Icon",std::string{});
                    if(!row.value("Available",true))continue;
                    if(auto* loaded=UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,RC::to_generic_string(asset).c_str(),false)){
                        const auto provenance=PS::AssetProvenance::Lookup(loaded);
                        if(provenance.is_object()&&(!provenance.value("Registered",false)||provenance.value("LastCloneDefinitionErrors",0)!=0))continue;
                    }
                    const auto owner=row.value("SourceKind",std::string{})=="modded"?row.value("SourceName",std::string{}):std::string{};
                    const auto origins=PS::ContentSource::Origins(asset,plan,owner);
                    if(!Source.empty()&&!origins.contains(Source))continue;
                    if(!icon.empty()){PS::HelpyCacheSources::Registered(asset);PS::HelpyCacheSources::Reference(asset,icon);Add(icon,origins);}
                    else if(row.value("Mounted",false))try{
                        auto* object=ActorHelper::ResolveObject(RC::to_generic_string(asset));
                        auto* itemType=ActorHelper::ResolveClass(TEXT("/Script/Dominion.ItemData"));
                        if(!object||!itemType||!object->IsA(itemType))throw std::runtime_error("Mounted ItemData owner could not be inspected");
                        PS::HelpyCacheSources::Registered(asset);
                        for(const auto& reference:PS::AssetSearch::IconReferences(object))Add(reference,origins);
                    }catch(const std::exception& e){++DiscoveryFailed;Issue(asset,"Item discovery",e.what());}
                }
                if(ItemPageLast&&ItemPageAt>=ItemPage.size()){
                    ItemsDiscovered=true;Stage="Discovering registered icons";
                }
                Publish();return;
            }
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
                }else if(MetadataAt<MetadataTotal) {
                    if(Metadata.empty()||MetadataAt>=MetadataWindowAt+Metadata.size()){
                        auto page=PS::CookedAssets::MetadataWindow(MetadataAt,128);
                        if(page.Revision!=MetadataRevision||page.Total!=MetadataTotal||page.Records.empty())
                            throw std::runtime_error("Mounted registry changed during icon discovery; restart the cache build. No complete pack was exported.");
                        MetadataWindowAt=MetadataAt;Metadata=std::move(page.Records);
                    }
                    const auto& record=Metadata[MetadataAt++-MetadataWindowAt];if(!plan.Registered.contains(PS::HelpyCacheSources::Source(record.path)))continue;
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
                    }catch(const std::exception& e){++DiscoveryFailed;Issue(record.path,"Discovery",e.what());}
                }else {
                    for(const auto& [path,origins]:Found)Icons.push_back({path,origins});Found.clear();Discovered=true;Stage="Waiting for source fingerprints";WaitingSince=Clock::now();
                }
            }
            if(!Discovered){Publish();return;}
            if(uint64_t(Icons.size())*(16+64*64*4)>uint64_t(limitMB)*1024*1024){
                Stage="More cache space needed";Publish();return;
            }
            plan=PS::HelpyCacheSources::Snapshot();
            if(PS::HelpySourceCache::Current.PlanRevision!=plan.Revision) {
                if(Clock::now()-WaitingSince>std::chrono::seconds(60))throw std::runtime_error("Source fingerprints did not settle; retry when content registration is stable.");
                Stage="Waiting for source fingerprints";Publish();return;
            }
            if(FrozenKeys.empty())FrozenKeys=PS::HelpySourceCache::Current.SourceKeys;
            else if(FrozenKeys!=PS::HelpySourceCache::Current.SourceKeys)throw std::runtime_error("Registered content changed during capture; restart the cache build.");
            // Disk hits are checked in bounded batches, before any renderer
            // gate. Re-scans do not redraw/rewrite every existing thumbnail.
            const auto cacheDeadline=Clock::now()+std::chrono::milliseconds(2);
            while(!Capturing&&At<Icons.size()&&Clock::now()<cacheDeadline){
                const auto cached=PS::ItemIconThumbnail::Cached(Icons[At].Path);if(!cached)break;
                Index(*cached,Icons[At]);++Hits;++At;Attempt=0;RetryAt={};
            }
            if(At==Icons.size()){
                Stage="Verifying complete cache";
                // Detect evictions, removed/corrupt files and cross-cook images
                // before certifying a batch, not merely completed attempts.
                while(VerifyAt<Icons.size()&&Clock::now()<cacheDeadline){
                    const auto& icon=Icons[VerifyAt++];bool good=false;
                    if(const auto cached=PS::ItemIconThumbnail::Cached(icon.Path))try{
                        const auto sidecar=std::filesystem::path(cached->at("File").get<std::string>()+".json");
                        if(std::filesystem::file_size(sidecar)<=8192){const auto index=nlohmann::json::parse(PS::ConfigFiles::Read(sidecar));good=index.value("Icon",std::string{})==icon.Path&&!index.value("DisplayOnly",false);}
                    }catch(...){}
                    if(good)++Verified;else Issue(icon.Path,"Verification","Current-source pixels or verified mapping are missing.");
                }
                if(VerifyAt<Icons.size()){Publish();return;}
                Active=Capturing=false;Stage=!Icons.empty()&&!Failed&&!DiscoveryFailed&&Verified==Icons.size()?"Complete":"Completed with failures";
                Report();Metadata.clear();Icons.clear();FrozenKeys.clear();return;
            }
            if(Clock::now()>=cacheDeadline){Stage="Checking cached icons";Publish();return;}
            if(!world){if(Clock::now()-WaitingSince>std::chrono::seconds(60))throw std::runtime_error("No stable menu/world renderer is available. No gameplay world was opened.");Stage="Waiting for menu renderer";Publish();return;}
            WaitingSince=Clock::now();Stage="Caching registered icons";
            if(Clock::now()<RetryAt){Publish();return;}
            if(!rendererFree&&!Capturing){Publish();return;}
            const auto& icon=Icons[At];
            try {
                if(PS::HelpySourceCache::Fingerprint(icon.Path).empty())throw std::runtime_error("Icon source is not registered");
                const auto result=PS::ItemIconThumbnail::Render(world,icon.Path);Capturing=result.value("Status",std::string{})=="Pending";
                if(Capturing){Publish();return;}
                if(result.value("Status",std::string{})!="Ready")throw std::runtime_error("Icon capture did not return pixels");
                Index(result,icon);
                if(result.value("CacheHit",false))++Hits;else ++Captured;
            }catch(const std::exception& error){
                PS::ItemIconThumbnail::Reset();Capturing=false;
                if(!Attempt&&PS::IconCapture::Retryable(error.what())){++Attempt;++Retries;RetryAt=Clock::now()+std::chrono::milliseconds(80);Issue(icon.Path,"Retry",error.what());Publish();return;}
                ++Failed;Issue(icon.Path,"Capture",error.what());
            }
            Attempt=0;RetryAt={};++At;Publish();
        }catch(const std::exception& error){Stop(error.what());}
    }
};
BulkIconCacheJob BulkIconJob;
}
