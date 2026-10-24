#include "Core/HelpySourceCache.h"
#include "QuickMenuCatalog.inl"
namespace {
    std::pair<PS::MapProjection::Point,PS::MapProjection::Projection> ToolMapTarget(UWorld* world,const nlohmann::json& request){
        const auto& value=request.at("TargetLocation");
        if(!value.is_object()||!value.contains("X")||!value.contains("Y")||!value["X"].is_number()||!value["Y"].is_number())throw std::runtime_error("Select a valid map position");
        const PS::MapProjection::Point point{value["X"].get<double>(),value["Y"].get<double>()};const auto projection=PS::HelpyMapRuntime::Projection(world);
        if(!projection.Contains(point))throw std::runtime_error("Selected position is outside the native world map");return {point,projection};
    }
    FVector ToolMapGround(UWorld* world,const nlohmann::json& request,AActor* pawn=nullptr){
        const auto [point,projection]=ToolMapTarget(world,request);FVector ground{};std::string error;
        std::vector<AActor*> ignore;if(pawn)ignore.push_back(pawn);
        for(const auto candidate:PS::MapLanding::Candidates(point,request.value("NearMarker",false)))if(projection.Contains(candidate)&&UECustom::UKismetSystemLibrary::FindGroundSurface(world,FVector(candidate.X,candidate.Y,projection.MaxZ),FVector(candidate.X,candidate.Y,projection.MinZ),ignore,ground,error))return ground;
        throw std::runtime_error("Ground is unavailable at this map point. Move closer to load the area. "+error);
    }
    bool ToolMapInWorld(UObject* object,UWorld* world){for(int i=0;object&&i<32;++i,object=object->GetOuterPrivate())if(object==world)return true;return false;}
    bool ToolMapLodestone(std::string name){std::ranges::transform(name,name.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});std::erase_if(name,[](unsigned char c){return c=='_'||c==' '||c=='-';});return name.find("lodestone")!=name.npos||name.find("teleportstone")!=name.npos;}
    std::string ToolMapActorKind(UClass* type){
        if(!type)return {};
        for(UStruct* base=type;base;base=base->GetSuperStruct()){
            const auto path=RC::to_string(base->GetPathName());if(ToolMapLodestone(path))return "Lodestone";
            if(path=="/Script/Dominion.AISpawnPoint")return "AI";
        }
        if(PS::PlayerInventoryPresentation::StorageClass(type))return "Building";
        auto* actor=ActorHelper::ResolveClass(TEXT("/Script/Engine.Actor"));auto* ai=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionAICharacter"));
        return actor&&ai&&ToolResourceClass(type,actor,ai)?"Resource":"";
    }
    nlohmann::json ToolMapActor(UObject* object,UWorld* world,std::string kind,bool managed=false,const std::string& name={}){
        if(!object||!object->IsA<AActor>()||!ToolMapInWorld(object,world)||object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)))return nullptr;
        const auto at=ActorHelper::GetActorLocation(static_cast<AActor*>(object));if(!std::isfinite(at.X())||!std::isfinite(at.Y())||!std::isfinite(at.Z()))return nullptr;
        if(kind=="Building"){if(PS::PlayerInventoryPresentation::Storage(object))kind="Chest";else if(managed)kind="Actor";else return nullptr;}
        const auto identity=PS::WeakObject(object);if(identity.Get()!=object)return nullptr;
        auto label=name;if(label.empty())label=PS::AssetSearch::PresentationName(object);if(label.empty())label=ToolReadableName(RC::to_string(object->GetClassPrivate()->GetPathName()));
        auto row=nlohmann::json{{"Path",RC::to_string(object->GetPathName())},{"Name",label},{"Kind",kind},{"ObjectIndex",identity.ObjectIndex},{"ObjectSerial",identity.ObjectSerialNumber},{"Managed",managed},{"Loaded",true},{"Location",{at.X(),at.Y(),at.Z()}}};
        if(kind=="Chest")row["Icon"]=PS::AssetSearch::PresentationIcon(object);return row;
    }
    nlohmann::json ToolMapNativeMarkers(UWorld* world,bool& scanning,const nlohmann::json& filters,const nlohmann::json& players){
        struct Marker{PS::WeakObjectHandle Actor;std::string Kind;nlohmann::json Row;};
        static uint64_t generation=0;static size_t cursor=0;static std::vector<Marker> found;
        static std::unordered_set<std::string> seen;static std::unordered_map<std::string,std::string> kinds;static auto next=std::chrono::steady_clock::time_point{};
        const auto now=std::chrono::steady_clock::now(),begin=now;const auto current=PS::SpawnToolRequests::Generation.load();
        if(generation!=current){generation=current;cursor=0;found.clear();seen.clear();kinds.clear();next={};}
        const auto count=FUObjectArray::GetNumElements();if(count<0||count>4'000'000)throw std::runtime_error("Map actor discovery exceeds its bound");
        if(now>=next){
            size_t scanned=0;
            while(cursor<static_cast<size_t>(count)&&scanned++<8192&&std::chrono::steady_clock::now()-begin<std::chrono::microseconds(1500)){
                auto* slot=FUObjectArray::IndexToObject(static_cast<int32>(cursor++));auto* object=slot?slot->GetUObject():nullptr;
                if(!object||!object->IsA<AActor>()||!ToolMapInWorld(object,world)||object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)))continue;
                auto* type=object->GetClassPrivate();const auto className=RC::to_string(type->GetPathName());auto it=kinds.find(className);
                if(it==kinds.end())it=kinds.emplace(className,ToolMapActorKind(type)).first;
                if(it->second.empty())continue;const auto key=RC::to_string(object->GetPathName());
                if(found.size()<8192&&!seen.contains(key)){auto row=ToolMapActor(object,world,it->second);if(!row.is_null()){seen.insert(key);const auto kind=row.value("Kind",it->second);found.push_back({PS::WeakObject(object),kind,std::move(row)});}}
            }
            if(cursor>=static_cast<size_t>(count)){cursor=0;next=now+std::chrono::seconds(4);kinds.clear();}
        }
        scanning=cursor!=0;auto rows=nlohmann::json::array();
        static size_t resourceCursor=0;const auto resourceStart=std::chrono::steady_clock::now();size_t sampled=0;
        const auto radius=filters.value("ResourceRadiusMetres",200.0);const bool observations=filters.value("ObserveNearbyResources",true)&&filters.value("Resources",false);
        if(observations&&!found.empty())for(size_t visited=0;visited<found.size()&&sampled<24&&std::chrono::steady_clock::now()-resourceStart<std::chrono::microseconds(1000);++visited){
            resourceCursor%=found.size();auto& marker=found[resourceCursor++];if(marker.Kind!="Resource"||marker.Row.is_null()||!PS::ResourceObservation::Near(marker.Row["Location"],players,radius))continue;
            auto* actor=marker.Actor.Get();if(!actor||!ToolMapInWorld(actor,world)||actor->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed)))continue;
            marker.Row["ResourceObservation"]=PS::ResourceObservation::Read(actor);++sampled;
        }
        for(auto it=found.begin();it!=found.end();){auto* actor=it->Actor.Get();
            if(!actor||!ToolMapInWorld(actor,world)||actor->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed))||it->Row.is_null()){it=found.erase(it);continue;}if(filters.value(it->Kind=="Lodestone"?"Lodestones":it->Kind=="Chest"?"Chests":it->Kind=="Resource"?"Resources":"AI",it->Kind=="Lodestone"))rows.push_back(it->Row);++it;}
        // Rebuild identity membership so a streamed-out marker can return later.
        seen.clear();for(const auto& marker:found)seen.insert(marker.Row.at("Path").get<std::string>());
        return rows;
    }
    nlohmann::json ToolCatalog(const std::filesystem::path& file,const char* kind) {
        if(!std::filesystem::is_regular_file(file))throw std::runtime_error(std::string(kind)+" catalog is missing; explicitly build the catalogs first");
        auto value=nlohmann::json::parse(PS::ConfigFiles::Read(file,16*1024*1024));
        if(!value.is_object() || value.value("SchemaVersion",0)!=1 || value.value("Kind",std::string{})!=kind
            || !value.contains("Entries") || !value["Entries"].is_array() || value["Entries"].size()>65536)
            throw std::runtime_error(std::string(kind)+" catalog is invalid; rebuild it");
        return value["Entries"];
    }
    std::string ToolWindowFold(std::string value) {
        std::ranges::transform(value,value.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return value;
    }
    bool ToolWindowMatches(const nlohmann::json& row,const std::string& query) {
        return PS::SpawnCatalogSearch::Matches(row,query);
    }
    std::uint64_t ToolStableId(const std::string& value) {
        std::uint64_t hash=1469598103934665603ull;for(const unsigned char c:value){hash^=c;hash*=1099511628211ull;}return hash;
    }
    nlohmann::json ToolDefinitionWindow(const nlohmann::json& authored,int category,const std::string& query,std::size_t offset,std::size_t limit,
        const std::string& fieldSet="BasicRow",std::size_t scanBudget=1536,std::uint64_t timeBudgetMicros=800) {
        if(category!=1&&category!=2)throw std::runtime_error("Definition catalog category must be AI/NPC or Resources.");
        if(limit<1||limit>128)throw std::runtime_error("Catalog window limit must be 1..128 rows.");
        if(query.size()>256)throw std::runtime_error("Catalog search text exceeds 256 characters.");
        if(fieldSet!="BasicRow"&&fieldSet!="Detail")throw std::runtime_error("Unsupported definition catalog field projection.");
        scanBudget=std::clamp<std::size_t>(scanBudget,128,8192);timeBudgetMicros=std::clamp<std::uint64_t>(timeBudgetMicros,100,5000);
        nlohmann::json rows=nlohmann::json::array();std::set<std::string> authoredIdentities;
        for(const auto& row:authored)if(row.is_object())authoredIdentities.insert(row.value("Type",std::string{})+row.value("Class",std::string{}));
        std::size_t raw=0,scanned=0;const auto begin=std::chrono::steady_clock::now();
        const auto exhausted=[&]{return scanned>=scanBudget||(scanned>0&&std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-begin).count()>=static_cast<long long>(timeBudgetMicros));};
        for(const auto& original:authored) {
            const auto current=raw++;if(current<offset)continue;if(exhausted())return {{"Rows",std::move(rows)},{"NextOffset",current},{"HasMore",true},{"Scanned",scanned},{"BudgetExhausted",true}};++scanned;
            if(!original.is_object())continue;auto row=original;const auto type=row.value("Type",std::string{});
            const bool allowed=category==1?(type=="AI"||type=="NPC"):type=="Resource";if(!allowed||!ToolWindowMatches(row,query))continue;
            if(type=="AI"){
                auto* native=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionAICharacter"));
                auto* loaded=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,RC::to_generic_string(row.value("Class",std::string{})).c_str(),false);
                const bool spawnable=native&&loaded&&!loaded->HasAnyFlags(static_cast<EObjectFlags>(RF_NeedLoad|RF_NeedPostLoad|RF_BeginDestroyed|RF_FinishDestroyed))&&loaded->IsChildOf(native)&&!ActorHelper::IsAbstract(loaded);
                row["SpawnableCharacter"]=spawnable;
                // Unloaded registry candidates remain discoverable. They are
                // never made spawnable until the existing read-only inspector
                // resolves and verifies the actual native class.
                if(!spawnable){if(!row.value("Mounted",false)||!row.value("NeedsInspection",false))continue;}
                else {row["Available"]=true;row["NeedsInspection"]=false;}
            }
            const auto identity=row.value("Class",row.value("Key",std::string{}));row["StableId"]=ToolStableId(type+":"+identity);
            if(type=="NPC"&&!row.contains("Icon"))if(auto* loaded=UECustom::UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,RC::to_generic_string(identity).c_str(),false))if(auto* defaults=loaded->GetClassDefaultObject().Get();defaults&&!defaults->HasAnyFlags(static_cast<EObjectFlags>(RF_NeedLoad|RF_NeedPostLoad|RF_BeginDestroyed|RF_FinishDestroyed)))row["Icon"]=PS::AssetSearch::PresentationIcon(defaults);
            rows.push_back(std::move(row));if(rows.size()>=limit)return {{"Rows",std::move(rows)},{"NextOffset",raw},{"HasMore",true},{"Scanned",scanned}};
        }
        auto* classType=ActorHelper::ResolveClass(TEXT("/Script/CoreUObject.Class"));
        auto* actorType=ActorHelper::ResolveClass(TEXT("/Script/Engine.Actor"));
        auto* aiType=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionAICharacter"));
        auto* npcType=ActorHelper::ResolveClass(TEXT("/Script/Dominion.InteractableNPC"));
        if(!classType||!actorType||!aiType)throw std::runtime_error("The runtime actor classes are not ready");
        const auto objectCount=FUObjectArray::GetNumElements();
        if(objectCount<0||objectCount>4'000'000)throw std::runtime_error("Global object roster exceeds the safe catalog bound");
        const std::size_t authoredCount=raw,total=static_cast<std::size_t>(objectCount);
        std::size_t objectOffset=offset>authoredCount?std::min(offset-authoredCount,total):0;
        for(std::size_t i=objectOffset;i<total;++i) {
            if(exhausted())return {{"Rows",std::move(rows)},{"NextOffset",authoredCount+i},{"HasMore",true},{"Scanned",scanned},{"BudgetExhausted",true}};++scanned;
            raw=authoredCount+i+1;auto* slot=FUObjectArray::IndexToObject(static_cast<int32>(i));auto* object=slot?slot->GetUObject():nullptr;
            if(!object||!object->IsA(classType)||object->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed|RF_NeedLoad|RF_NeedPostLoad)))continue;
            auto* type=static_cast<UClass*>(object);const bool npc=npcType&&type->IsChildOf(npcType),ai=!npc&&type->IsChildOf(aiType);
            bool resource=false;if(category==2)resource=!npc&&!ai&&ToolResourceClass(type,actorType,aiType);
            if((category==1&&!npc&&!ai)||(category==2&&!resource))continue;
            const auto path=RC::to_string(type->GetPathName()),kind=std::string(npc?"NPC":ai?"AI":"Resource");
            if(path.empty()||authoredIdentities.contains(kind+path))continue;
            nlohmann::json light={{"Key",std::string(npc?"@loaded-npc:":ai?"@loaded-ai:":"@loaded-resource:")+path},{"Name",ToolReadableName(path)},{"Class",path},{"Type",kind},{"StableId",ToolStableId(kind+":"+path)}};
            if(!ToolWindowMatches(light,query))continue;
            if(fieldSet=="Detail") {
                auto detailed=ToolDefinitionRow(type,light["Key"].get<std::string>(),ai,npc?"NPC":"");detailed["StableId"]=light["StableId"];
                rows.push_back(std::move(detailed));
            } else {
                const bool concrete=!ActorHelper::IsAbstract(type);light["Available"]=concrete;light["Loaded"]=true;light["Reason"]=concrete?"":"Abstract base definition; not spawnable";
                if(ai)light["SpawnableCharacter"]=concrete;
                if(auto* defaults=type->GetClassDefaultObject().Get();defaults&&!defaults->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed|RF_NeedLoad|RF_NeedPostLoad))){light["Icon"]=PS::AssetSearch::PresentationIcon(defaults);if(const auto name=PS::AssetSearch::PresentationName(defaults);!name.empty()){light["Name"]=name;light["DisplayName"]=name;}}
                if(npc) {light["TemporaryAllowed"]=false;light["PermanentAllowed"]=false;light["TemporaryReason"]="Vanilla NPC definition: browse-only.";light["PermanentReason"]="Vanilla narrative NPC copying is not enabled.";}
                else if(!ai) {
                    std::string family;std::size_t depth=0;for(UStruct* base=type;base&&depth++<64;base=base->GetSuperStruct()) {const auto name=PS::QuickCatalogRules::Lower(RC::to_string(base->GetName()));if(name.find("fellabletree")!=name.npos||name.find("felledtree")!=name.npos||name.find("splittablelog")!=name.npos||name.find("sapling")!=name.npos){family="Tree";break;}if(PS::QuickCatalogRules::MiningName(name))family="Mineral";}light["ResourceFamily"]=family;
                }
                rows.push_back(std::move(light));
            }
            if(rows.size()>=limit)return {{"Rows",std::move(rows)},{"NextOffset",raw},{"HasMore",i+1<total},{"Scanned",scanned}};
        }
        return {{"Rows",std::move(rows)},{"NextOffset",authoredCount+total},{"HasMore",false},{"Scanned",scanned}};
    }

    std::size_t ToolCursorOffset(const std::string& cursor,const char prefix,std::size_t fallback=0) {
        if(cursor.size()<3||cursor[0]!=prefix||cursor[1]!=':')return fallback;
        try{return static_cast<std::size_t>(std::stoull(cursor.substr(2)));}catch(...){return fallback;}
    }
    bool ToolRandomCursor(const std::string& cursor,std::uint64_t& start,std::size_t& offset) {
        if(cursor.size()<5||cursor[0]!='R'||cursor[1]!=':')return false;const auto split=cursor.find(':',2);if(split==std::string::npos)return false;
        try{start=std::stoull(cursor.substr(2,split-2));offset=static_cast<std::size_t>(std::stoull(cursor.substr(split+1)));return true;}catch(...){return false;}
    }
    void ExportCloneDraft(const nlohmann::json& draft) {
        if(!draft.is_object() || draft.size()!=1)throw std::runtime_error("Clone draft must contain exactly one asset entry");
        const auto& [path,fields]=*draft.items().begin();
        if(path.empty() || path.front()!='/' || !fields.is_object() || !fields.contains("$Clone") || !fields["$Clone"].is_string()
            || !fields.contains("PersistenceID") || !fields["PersistenceID"].is_string()
            || !fields.contains("InternalName") || !fields["InternalName"].is_string())
            throw std::runtime_error("Clone draft requires an output path, $Clone, PersistenceID and InternalName");
        const auto id=fields["PersistenceID"].get<std::string>();
        if(!DragonWilds::IsCanonicalPersistenceId(id))throw std::runtime_error("Clone PersistenceID is invalid");
        const auto directory=PS::HostServices::ExportsDirectory()/"assets";std::filesystem::create_directories(directory);
        const auto stamp=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        const auto file=directory/("clone_"+std::to_string(stamp)+".json");
        PS::ConfigFiles::Write(file,draft.dump(2));
        if(nlohmann::json::parse(PS::ConfigFiles::Read(file,1024*1024))!=draft)throw std::runtime_error("Clone export verification failed");
    }
    void GiveToolItem(UObject* controller,const std::string& path,int32_t count,bool equip=false,nlohmann::json* equipResult=nullptr) {
        if(!controller || count<1 || count>10000)throw std::runtime_error("Item grant requires an authoritative player and quantity 1..10000");
        auto* item=ActorHelper::ResolveObject(RC::to_generic_string(path));
        auto* itemType=ActorHelper::ResolveClass(TEXT("/Script/Dominion.ItemData"));
        auto* inventory=ActorHelper::GetObjectRef(controller,TEXT("InventoryComponent"));
        auto* inventoryType=ActorHelper::ResolveClass(TEXT("/Script/Dominion.InventoryComponent"));
        if(!item || !itemType || !item->IsA(itemType))throw PS::HelpyGrants::Rejected("Selected item is unavailable or is not ItemData");
        if(PS::AssetMetadata::IsIncomplete(item))throw PS::HelpyGrants::Rejected("Item publication is incomplete; inventory grant blocked. Repair pending Helpy files with the game closed.");
        if(!inventory || !inventoryType || !inventory->IsA(inventoryType) || inventory->GetOuterPrivate()!=controller)
            throw std::runtime_error("Selected player's authoritative inventory is unavailable");
        const auto validate=[](const TCHAR* path,int32_t /*legacySize*/,std::initializer_list<std::tuple<const TCHAR*,int32_t,int32_t>> fields) {
            auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path,false);
            ReflectedContract::Frame(function);
            size_t count=0;for(auto* field:TFieldRange<FProperty>(function,EFieldIterationFlags::Default))if(field->HasAnyPropertyFlags(CPF_Parm))++count;
            if(count!=fields.size())throw std::runtime_error("Native inventory command parameter count changed");
            for(const auto& [name,offset,width]:fields) {
                auto* field=function->FindProperty(FName(name,FNAME_Find));
                if(!ReflectedContract::Fits(field,function->GetParmsSize()) || !field->HasAnyPropertyFlags(CPF_Parm)
                    || (RC::StringType(name)!=TEXT("GameplayTags") && field->GetElementSize()!=width))
                    throw std::runtime_error("Native inventory command parameter layout changed");
            }
        };
        constexpr const TCHAR* countPath=TEXT("/Script/Dominion.InventoryComponent:GetNumItemsByData");
        constexpr const TCHAR* readyPath=TEXT("/Script/Dominion.InventoryComponent:CanAddItemByData");
        constexpr const TCHAR* givePath=TEXT("/Script/Dominion.InventoryComponent:AddItemByData");
        validate(countPath,12,{{TEXT("ItemData"),0,8},{TEXT("ReturnValue"),8,4}});
        validate(readyPath,13,{{TEXT("ItemData"),0,8},{TEXT("Count"),8,4},{TEXT("ReturnValue"),12,1}});
        validate(givePath,49,{{TEXT("ItemData"),0,8},{TEXT("Count"),8,4},{TEXT("DurabilityPercentage"),12,4},{TEXT("GameplayTags"),16,32},{TEXT("ReturnValue"),48,1}});
        ActorHelper::FunctionCall beforeCall(inventory,countPath);beforeCall.Arg(TEXT("ItemData"),item).Invoke();const auto before=beforeCall.Result<int32_t>();
        ActorHelper::FunctionCall ready(inventory,readyPath);ready.Arg(TEXT("ItemData"),item).Arg(TEXT("Count"),count).Invoke();
        if(!ready.Result<bool>())throw PS::HelpyGrants::Rejected("The selected inventory cannot accept that quantity");
        auto* tagType=UECustom::UObjectGlobals::StaticFindObject<UScriptStruct*>(nullptr,nullptr,TEXT("/Script/GameplayTags.GameplayTagContainer"));
        if(!tagType)throw std::runtime_error("Inventory tag container unavailable");
        std::set<PS::EquipmentDelivery::Guid> existing;bool captured=false;std::string captureError;
        if(equip&&equipResult)try{existing=PS::EquipmentDelivery::Capture(controller,path);captured=true;}catch(const std::exception& error){captureError=error.what();}catch(...){captureError="Equipment inventory snapshot unavailable";}
        ActorHelper::FunctionCall give(inventory,givePath);give.Arg(TEXT("ItemData"),item).Arg(TEXT("Count"),count)
            .Arg(TEXT("DurabilityPercentage"),1.0f).JsonArg(TEXT("GameplayTags"),nlohmann::json::object()).Invoke();
        ActorHelper::FunctionCall afterCall(inventory,countPath);afterCall.Arg(TEXT("ItemData"),item).Invoke();const auto after=afterCall.Result<int32_t>();
        if(!give.Result<bool>() || int64_t(after)-before!=count)throw std::runtime_error("Native inventory grant could not be confirmed");
        // Delivery is already confirmed. Equipment errors are a separate receipt
        // and must never escape into the grant retry/uncertainty state machine.
        if(equip&&equipResult)*equipResult=captured?PS::EquipmentDelivery::Equip(controller,path,count,existing):nlohmann::json{{"State","Skipped"},{"Message",captureError}};
    }
}

void DragonWildsSpawnLoader::HandleNetworkHelpyMarks(UObject* player,const std::string& payload){if(m_markRelay)m_markRelay->Receive(player,payload);}

std::string DragonWildsSpawnLoader::HandleNetworkHelpyAuthority(UObject* player,const std::string& action,const std::string& payload) {
    if(!player || !player->GetWorld() || !GetGameMode(player->GetWorld()))
        throw std::runtime_error("Helpy authority request did not arrive in an authoritative world");
    if(action!="GiveItems" && action!="Spawn" && action!="Players" && action!="InventoryMarks")
        throw std::runtime_error("Helpy authority action is not permitted");
    if(payload.empty() || payload.size()>256*1024)
        throw std::runtime_error("Helpy authority payload exceeds the safe bound");

    auto request=nlohmann::json::parse(payload);
    if(!request.is_object() || request.value("Action",std::string{})!=action)
        throw std::runtime_error("Helpy authority payload does not match its action");
    auto* controller=ActorHelper::GetObjectRef(player,TEXT("Controller"));
    if(!controller || controller->GetWorld()!=player->GetWorld())
        throw std::runtime_error("Helpy authority request has no owning server controller");

    const auto& policy=PS::PSConfig::Get()->GetSettings().helpyAuthority;
    const auto playerGuid=GetPlayerCharacterGuid(controller);
    if(action=="InventoryMarks"){
        const auto normalize=[](std::string value){std::string out;for(unsigned char c:value){if(c=='-'||c=='{'||c=='}')continue;if(!std::isxdigit(c))return std::string{};out+=static_cast<char>(std::tolower(c));}return out.size()==32&&out!=std::string(32,'0')?out:std::string{};};
        const auto guid=normalize(playerGuid);
        if(payload.size()>3500||guid.empty()||ActorHelper::GetObjectRef(controller,TEXT("Pawn"))!=player)throw std::runtime_error("Public profile requires the active owned player");
        if(request.contains("ProfileAction")){
            const auto operation=request.at("ProfileAction").get<std::string>();const auto id=request.at("RequestId").get<uint64_t>();if(!id)throw std::runtime_error("Invalid profile request");
            if(operation=="Publish"){
                PS::InventoryMarkService::Call({{"Action","ReceivePublicProfile"},{"CharacterGuid",guid},{"Profile",request.at("Profile")}});
                return nlohmann::json{{"Kind","HelpyProfilePublished1"},{"RequestId",id},{"CharacterGuid",guid}}.dump();
            }
            if(operation!="Read")throw std::runtime_error("Invalid profile operation");
            const auto targetGuid=request.at("CharacterGuid").get<std::string>();if(normalize(targetGuid)!=targetGuid)throw std::runtime_error("Invalid profile target");
            bool ambiguous=false;auto* target=FindPlayerControllerByGuid(targetGuid,ambiguous);if(!target||ambiguous||target->GetWorld()!=player->GetWorld())throw std::runtime_error("Profile target is not connected");
            ActorHelper::FunctionCall local(target,TEXT("/Script/Engine.Controller:IsLocalController"));local.Invoke();
            const auto result=PS::InventoryMarkService::Call({{"Action","ReadPublicProfile"},{"CharacterGuid",targetGuid},{"Local",local.Result<bool>()}});
            return nlohmann::json{{"Kind","HelpyProfile1"},{"RequestId",id},{"CharacterGuid",targetGuid},{"Profile",result.at("Profile")}}.dump();
        }
        const auto& batch=request.at("Batch");
        if(payload.size()>3500||guid.empty()||batch.at("CharacterGuid")!=guid||ActorHelper::GetObjectRef(controller,TEXT("Pawn"))!=player)
            throw std::runtime_error("A player may sync only their own active character marks");
        return PS::InventoryMarkService::Call({{"Action","ReceiveMarks"},{"Batch",batch}}).dump();
    }
    std::string playerName;
    if(auto* state=ActorHelper::GetObjectRef(controller,TEXT("PlayerState"))) {
        ActorHelper::FunctionCall getName(state,TEXT("/Script/Engine.PlayerState:GetPlayerName"));getName.Invoke();
        const auto value=getName.Result<FString>();const auto& chars=value.GetCharArray();
        if(chars.Num()>1&&chars.Num()<256&&chars.GetData())playerName=RC::to_string(RC::StringType(chars.GetData(),chars.Num()-1));
    }
    if(action=="Players") {
        nlohmann::json roster=nlohmann::json::array();
        for(const auto& connected:GetConnectedPlayersInLoadOrder()) {
            bool ambiguous=false;auto* target=FindPlayerControllerByGuid(connected.Guid,ambiguous);
            if(!target||ambiguous||target->GetWorld()!=player->GetWorld())continue;
            roster.push_back({{"Name",connected.Name},{"Guid",connected.Guid},
                {"PlayerId",PS::PlayerStatusRuntime::PlayerId(ActorHelper::GetObjectRef(target,TEXT("PlayerState")))},
                {"Platform",PS::PlayerStatusRuntime::ControllerPlatform(target)},{"PlatformAccountName",PS::PlayerStatusRuntime::ControllerAccountName(target)},
                {"RoleStatus",PS::PlayerStatusRuntime::Read(target)}});
            PS::PlayerStatusRuntime::Color(ActorHelper::GetObjectRef(target,TEXT("PlayerState")),roster.back());
        }
        for(auto& row:roster){bool ambiguous=false;auto* target=FindPlayerControllerByGuid(row.at("Guid").get<std::string>(),ambiguous);
            if(target&&!ambiguous)if(auto* pawn=ActorHelper::GetObjectRef(target,TEXT("Pawn")))row["StatWire"]=PS::PartyStatWire::Encode(PS::PlayerVitals::Details(pawn,true),PS::PlayerVitals::Now());}
        // The bridge has a bounded envelope; preserve the roster if a very large
        // server exceeds it, rather than sending a truncated stat message.
        auto response=nlohmann::json{{"Kind","HelpyPlayers1"},{"Players",roster}};
        if(response.dump().size()>3900)for(auto& row:response["Players"])row.erase("StatWire");
        if(response.dump().size()>3900)for(auto& row:response["Players"])row.erase("PlatformAccountName");
        return response.dump();
    }
    const auto permitted=[&]{
        const auto normalize=[](std::string_view value){std::string out;for(const unsigned char c:value)if(std::isxdigit(c))out.push_back(static_cast<char>(std::toupper(c)));return out;};
        const auto actual=normalize(playerGuid);
        if(!actual.empty()&&std::ranges::any_of(policy.permittedPlayerGuids,[&](const auto& value){return normalize(value)==actual;}))return true;
        return !playerName.empty()&&std::find(policy.permittedPlayerNames.begin(),policy.permittedPlayerNames.end(),playerName)!=policy.permittedPlayerNames.end();
    };
    if(!permitted())throw std::runtime_error("This player is not permitted to use Helpy server mutations");
    if(action=="GiveItems") {
        if(!policy.allowClientItemGrants)
            throw std::runtime_error("This server does not allow client Helpy item grants");
        if(!request.contains("Items") || !request["Items"].is_array() || request["Items"].empty()
            || request["Items"].size()>PS::HelpyGrants::MaxSelection)
            throw std::runtime_error("Helpy item grant must contain 1..64 item rows");
        int64_t total=0;
        for(const auto& row:request["Items"]) {
            if(!row.is_object() || !row.contains("Item") || !row["Item"].is_string())
                throw std::runtime_error("Helpy item grant contains an invalid item row");
            const auto item=row["Item"].get<std::string>();
            const int count=row.value("Count",0);
            if(item.empty() || item.size()>1024 || item.front()!='/' || count<1)
                throw std::runtime_error("Helpy item grant contains an invalid path or quantity");
            total+=count;
            if(total>std::clamp(policy.maximumItemCount,1,10000))
                throw std::runtime_error("Helpy item grant exceeds the server quantity limit");
        }
    } else {
        if(!policy.allowClientTemporarySpawns)
            throw std::runtime_error("This server does not allow client Helpy temporary spawns");
        if(request.value("Permanent",false))
            throw std::runtime_error("Clients cannot author permanent Helpy spawns");
        const int count=request.value("Count",1);
        if(count<1 || count>std::clamp(policy.maximumSpawnCount,1,100))
            throw std::runtime_error("Helpy spawn count exceeds the server limit");
        request["Permanent"]=false;
        if(request.value("NPC",false)) {
            const int duration=request.value("DurationSeconds",0);
            if(duration<1 || duration>std::clamp(policy.maximumNpcDurationSeconds,1,3600))
                throw std::runtime_error("Helpy NPC duration exceeds the server limit");
        }
    }

    request["Player"]=RC::to_string(controller->GetPathName());
    request["World"]=RC::to_string(player->GetWorld()->GetPathName());
    request["_Source"]="HelpyNetwork";
    request["_RequestId"]=++m_networkToolSequence;
    if(!PS::SpawnToolRequests::TrySubmit(std::move(request)))
        throw std::runtime_error("Helpy server command queue is busy; retry after the active command completes");
    return action=="GiveItems"?"Server accepted the bounded item request.":"Server accepted the temporary spawn request.";
}


std::pair<bool,std::string> DragonWildsSpawnLoader::VerifyToolBuildingInstance(AActor* actor,UObject* building,const SpawnInfo& spawn,UWorld* world,bool requireTransient) const {
    if(!actor || !building || !world || !GetGameMode(world))
        return {false,"No authoritative world/building actor is available for native building preflight"};
    auto* actorClass=ActorHelper::ResolveClass(spawn.ClassPath);
    if(!actorClass || !ActorHelper::IsActorClass(actorClass) || ActorHelper::IsAbstract(actorClass))
        return {false,"BuildableActor is not a concrete actor class"};
    if(actor->GetWorld()!=world)return {false,"Building actor belongs to another world"};
    if(actor->GetClassPrivate()!=actorClass)return {false,"Building actor class does not exactly match BuildableActor"};
    auto* scene=ActorHelper::ResolveClass(TEXT("/Script/Engine.SceneComponent"));auto* root=ActorHelper::GetObjectRef(actor,TEXT("RootComponent"));
    if(!scene||!root||!root->IsA(scene))return {false,"Constructed building has no native scene root"};
    const auto actual=ActorHelper::GetActorLocation(actor);
    if(!PS::SpawnAuthoring::PositionMatches({spawn.Location.X(),spawn.Location.Y(),spawn.Location.Z()},{actual.X(),actual.Y(),actual.Z()}))
        return {false,std::format("Building actor did not retain the requested position: expected [{},{},{}], observed [{},{},{}]",spawn.Location.X(),spawn.Location.Y(),spawn.Location.Z(),actual.X(),actual.Y(),actual.Z())};
    if(actor->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)))
        return {false,"Building actor is a default/archetype/destroying object"};
    if(requireTransient && !actor->HasAnyFlags(RF_Transient))
        return {false,"Tool-created building actor lost RF_Transient"};

    auto* skip=CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(actorClass,TEXT("bSkipSpudStore")));
    if(requireTransient&&(!skip || skip->GetArrayDim()!=1))
        return {false,"Temporary placement is unavailable for this piece. Choose Permanent to save a placement."};
    if(requireTransient && !skip->GetPropertyValue(skip->ContainerPtrToValuePtr<void>(actor)))
        return {false,"Tool-created building actor is not excluded from the native save store"};

    bool anyBinding=false;
    if(!spawn.BuildingObjectProperty.empty()) {
        anyBinding=true;bool objectMatch=false;
        auto* property=PropertyHelper::GetPropertyByName(actorClass,RC::to_generic_string(spawn.BuildingObjectProperty));
        if(auto* hard=CastField<FObjectProperty>(property))
            objectMatch=hard->GetObjectPropertyValue(hard->ContainerPtrToValuePtr<void>(actor))==building;
        else if(auto* soft=CastField<FSoftObjectProperty>(property)) {
            const auto& current=*soft->ContainerPtrToValuePtr<UECustom::FSoftObjectPtr>(actor);
            const UECustom::FSoftObjectPath expected(building->GetPathName());
            objectMatch=current.ObjectID.AssetPath.GetPackageName()==expected.AssetPath.GetPackageName()
                && current.ObjectID.AssetPath.GetAssetName()==expected.AssetPath.GetAssetName();
        } else return {false,"BuildableActor data-binding property changed to an unsupported reference type"};
        if(!objectMatch)return {false,"BuildingPieceData object binding does not match the selected piece"};
    }
    if(spawn.bHasBuildingDataIndex) {
        anyBinding=true;
        auto* property=CastField<FNumericProperty>(PropertyHelper::GetPropertyByName(actorClass,TEXT("BuildingPieceDataIndex")));
        if(!property || !property->IsInteger() || property->GetArrayDim()!=1)
            return {false,"BuildableActor BuildingPieceDataIndex contract changed"};
        if(property->GetSignedIntPropertyValue(property->ContainerPtrToValuePtr<void>(actor))!=spawn.BuildingDataIndex)
            return {false,"BuildingPieceDataIndex does not match the selected piece"};
    }
    if(!anyBinding)return {false,"BuildableActor exposes no verified BuildingPieceData binding"};
    return {true,requireTransient?"Temporary actor retained the verified native binding and save-exclusion flags":"Native actor exactly matches this BuildingPieceData binding"};
}

std::pair<bool,std::string> DragonWildsSpawnLoader::VerifyToolBuildingCandidate(UObject* building,const SpawnInfo& spawn,UWorld* world,bool requireTemporary) const {
    if(!building || !world || !GetGameMode(world))return {false,"No authoritative world is available for native building preflight"};
    auto* actorClass=ActorHelper::ResolveClass(spawn.ClassPath);
    if(!actorClass || !ActorHelper::IsActorClass(actorClass) || ActorHelper::IsAbstract(actorClass))
        return {false,"BuildableActor is not a concrete actor class"};
    if(requireTemporary&&!CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(actorClass,TEXT("bSkipSpudStore"))))
        return {false,"Temporary placement is unavailable for this piece. Choose Permanent to save a placement."};
    if(spawn.BuildingObjectProperty.empty() && !spawn.bHasBuildingDataIndex)
        return {false,"BuildableActor exposes no verified BuildingPieceData binding"};

    auto* nativeBase=ActorHelper::ResolveClass(TEXT("/Game/Gameplay/BaseBuilding/Actors/BP_BaseBuilding_BaseActor.BP_BaseBuilding_BaseActor_C"));
    if(!nativeBase||!actorClass->IsChildOf(nativeBase))return {false,"BuildableActor is not a native building actor child"};
    auto* defaults=actorClass->GetClassDefaultObject().Get();
    if(!defaults||defaults->HasAnyFlags(static_cast<EObjectFlags>(RF_NeedLoad|RF_NeedPostLoad|RF_NeedInitialization|RF_BeginDestroyed|RF_FinishDestroyed)))
        return {false,"BuildableActor defaults are not initialized"};
    auto* root=ActorHelper::GetObjectRef(defaults,TEXT("RootComponent"));
    auto* scene=ActorHelper::ResolveClass(TEXT("/Script/Engine.SceneComponent"));
    bool hasRoot=root&&scene&&root->IsA(scene);
    // Blueprint construction can create the root after deferred spawning; the CDO need not own it.
    size_t rootClasses=0;for(auto* type=actorClass;!hasRoot&&scene&&type&&rootClasses++<64;type=static_cast<UClass*>(type->GetSuperStruct())){
        auto* scriptField=CastField<FObjectProperty>(PropertyHelper::GetPropertyByName(type->GetClassPrivate(),TEXT("SimpleConstructionScript")));if(!scriptField)continue;
        auto* script=scriptField->GetObjectPropertyValue(scriptField->ContainerPtrToValuePtr<void>(type));if(!script)continue;
        auto* nodes=CastField<FArrayProperty>(PropertyHelper::GetPropertyByName(script->GetClassPrivate(),TEXT("RootNodes")));auto* nodeType=nodes?CastField<FObjectProperty>(nodes->GetInner()):nullptr;if(!nodeType)continue;
        FScriptArrayHelper values(nodes,nodes->ContainerPtrToValuePtr<void>(script));if(values.Num()<0||values.Num()>64)return {false,"Building Blueprint root-node limit exceeded"};
        for(int index=0;index<values.Num();++index)if(auto* node=nodeType->GetObjectPropertyValue(values.GetRawPtr(index))){
            auto* componentField=CastField<FObjectProperty>(PropertyHelper::GetPropertyByName(node->GetClassPrivate(),TEXT("ComponentTemplate")));if(!componentField)continue;
            auto* component=componentField->GetObjectPropertyValue(componentField->ContainerPtrToValuePtr<void>(node));if(component&&component->IsA(scene)){hasRoot=true;break;}
        }
    }
    if(!hasRoot)return {false,"BuildableActor has no native or Blueprint scene root"};
    if(spawn.BuildingDataPath!=RC::to_string(building->GetPathName()))return {false,"Selected building differs from the validated data-binding source"};
    // RegisterBuildingProp preflights the exact writable reference/index types.
    // Bind them on the deferred instance before construction, not on shared
    // defaults. Requiring an existing world copy prevented the first placement.
    return {true,"Verified cooked building defaults and deferred native data-binding contract"};
}

void DragonWildsSpawnLoader::PumpSpawnTools(PS::WorkSlice& work) {
    // A coalesced read-only hint from Helpy's existing Read service. Never owns
    // or completes the mutation mailbox, even during a grant or travel action.
    if(m_readyWorld&&IsWorldStillLoaded(m_readyWorld)&&PS::SpawnToolRequests::TakePlayerRoles())try {
        const auto networkRole=PS::Network::Detect(m_readyWorld).Mode;
        PS::NetworkStatus::Current.Publish(networkRole,PS::SpawnToolRequests::Generation.load());
        if(networkRole==PS::Network::Role::Client) {
            if(ForwardHelpyAuthority)ForwardHelpyAuthority("Players",nlohmann::json{{"Action","Players"}}.dump());
        }else if(GetGameMode(m_readyWorld)) {
            auto rows=nlohmann::json::array();
            for(const auto& connected:GetConnectedPlayersInLoadOrder()) {
                if(rows.size()>=256)break;bool ambiguous=false;auto* controller=FindPlayerControllerByGuid(connected.Guid,ambiguous);
                if(!controller||ambiguous||controller->GetWorld()!=m_readyWorld)continue;
                bool isLocal=false;try{ActorHelper::FunctionCall local(controller,TEXT("/Script/Engine.Controller:IsLocalController"));local.Invoke();isLocal=local.Result<bool>();}catch(...){}
                rows.push_back({{"Name",connected.Name},{"Path",RC::to_string(controller->GetPathName())},{"Guid",connected.Guid},
                    {"IsLocal",isLocal},
                    {"PlayerId",PS::PlayerStatusRuntime::PlayerId(ActorHelper::GetObjectRef(controller,TEXT("PlayerState")))},
                    {"Platform",PS::PlayerStatusRuntime::ControllerPlatform(controller)},{"PlatformAccountName",PS::PlayerStatusRuntime::ControllerAccountName(controller)},
                    {"RoleStatus",PS::PlayerStatusRuntime::Read(controller)}});
                PS::PlayerStatusRuntime::Color(ActorHelper::GetObjectRef(controller,TEXT("PlayerState")),rows.back());
            }
            for(auto& row:rows){bool ambiguous=false;auto* target=FindPlayerControllerByGuid(row.at("Guid").get<std::string>(),ambiguous);
                if(target&&!ambiguous)if(auto* pawn=ActorHelper::GetObjectRef(target,TEXT("Pawn"))){row["Stats"]=PS::PlayerVitals::Details(pawn,true);row["Health"]=row["Stats"]["Health"];row["Equipment"]=PS::PlayerLoadoutPresentation::Cached(target,row.value("Guid",std::string{}),PS::SpawnToolRequests::CurrentGeneration());}}
            auto roles=PS::PlayerStatus::Snapshot(rows,PS::SpawnToolRequests::Generation.load());
            PS::NetworkStatus::Current.Publish(networkRole,PS::SpawnToolRequests::Generation.load(),rows.size());
            PS::SpawnToolRequests::CatalogProgress({{"Players",std::move(rows)},{"PlayerRoles",std::move(roles)}});
        }
    }catch(...){PS::NetworkStatus::Current.Publish(PS::Network::Role::Unknown,PS::SpawnToolRequests::Generation.load());} // Never rejects gameplay.
    // Retain the request mailbox's mutation slot until native arrival/failure.
    // Other loader work still runs on subsequent engine ticks; never flush or
    // block the world to stream a distant map destination.
    if(ToolMapTravelPump(m_readyWorld))return;
    // Hover queries coalesce and never own the mutation slot or load terrain.
    static auto nextMapGround=std::chrono::steady_clock::time_point{};
    const auto groundNow=std::chrono::steady_clock::now();
    if(!PS::SpawnToolRequests::IsWaiting()&&groundNow>=nextMapGround){
        if(auto query=PS::SpawnToolRequests::TakeMapGround()){
            nextMapGround=groundNow+std::chrono::milliseconds(150);
            const auto generation=PS::SpawnToolRequests::Generation.load();
            if(query->value("Generation",uint64_t{})==generation&&m_readyWorld){
                nlohmann::json reply={{"RequestId",query->at("RequestId")},{"Generation",generation},{"TargetLocation",query->at("TargetLocation")}};
                try{(*query)["NearMarker"]=false;const auto ground=ToolMapGround(m_readyWorld,*query);
                    reply["GroundAvailable"]=true;reply["Location"]={ground.X(),ground.Y(),ground.Z()};}
                catch(const std::exception& error){reply["GroundAvailable"]=false;reply["Error"]=error.what();}
                reply["ObservedUnixMs"]=PS::PlayerVitals::Now();reply["LandingRisk"]="unknown";
                reply["LandingRiskReason"]="Unloaded or unchecked ground; native travel checks still apply.";
                const auto riskPlayer=query->value("RiskPlayer",std::string{});
                if(!riskPlayer.empty()&&reply.value("GroundAvailable",false))try{
                    UObject* target=nullptr;TArray<UObject*> controllers;
                    auto* type=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerController"));if(type)UECustom::UObjectGlobals::GetObjectsOfClass(type,controllers,true);
                    if(controllers.Num()<0||controllers.Num()>256)throw std::runtime_error("Player roster bound exceeded");
                    for(auto* controller:controllers)if(controller&&controller->GetWorld()==m_readyWorld&&RC::to_string(controller->GetPathName())==riskPlayer){if(target)throw std::runtime_error("Ambiguous landing character");target=controller;}
                    auto* object=target?ActorHelper::GetObjectRef(target,TEXT("Pawn")):nullptr;auto* playerType=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerCharacter"));
                    if(!target||query->value("RiskGuid",std::string{}).empty()||GetPlayerCharacterGuid(target)!=query->value("RiskGuid",std::string{}))throw std::runtime_error("Landing character changed");
                    if(!object||!playerType||!object->IsA(playerType)||object->GetWorld()!=m_readyWorld)throw std::runtime_error("Landing character unavailable");
                    auto* pawn=static_cast<AActor*>(object);auto* capsule=ActorHelper::GetObjectRef(pawn,TEXT("CapsuleComponent"));auto* capsuleType=ActorHelper::ResolveClass(TEXT("/Script/Engine.CapsuleComponent"));
                    if(!capsule||!capsuleType||!capsule->IsA(capsuleType))throw std::runtime_error("Landing capsule unavailable");
                    ActorHelper::FunctionCall height(capsule,TEXT("/Script/Engine.CapsuleComponent:GetScaledCapsuleHalfHeight"));height.Invoke();
                    ActorHelper::FunctionCall width(capsule,TEXT("/Script/Engine.CapsuleComponent:GetScaledCapsuleRadius"));width.Invoke();
                    const auto [point,projection]=ToolMapTarget(m_readyWorld,*query);bool allGround=true;
                    try{
                        (void)PS::MapLanding::Resolve(projection,point,width.Result<float>(),height.Result<float>(),[&](PS::MapProjection::Point at,double top,double bottom){
                            FVector hit{};std::string error;if(!UECustom::UKismetSystemLibrary::FindGroundSurface(m_readyWorld,FVector(at.X,at.Y,top),FVector(at.X,at.Y,bottom),{pawn},hit,error)){allGround=false;throw std::runtime_error("Landing footprint is not fully loaded or measured");}
                            return PS::MapLanding::Position{hit.X(),hit.Y(),hit.Z()};
                        });reply["LandingFootprintChecked"]=true;reply["LandingRisk"]="lower";reply["LandingRiskReason"]="Recent native ground/slope footprint check passed. Collision, streaming and arrival remain checked at travel time.";
                    }catch(const std::exception& error){
                        // A missing trace is unknown, not proof of danger. Only
                        // measured slope/ledge failures receive a red warning.
                        const std::string reason=error.what();if(allGround&&reason.find("walkable slope")!=std::string::npos){reply["LandingFootprintChecked"]=true;reply["LandingRisk"]="higher";reply["LandingRiskReason"]=reason;}
                    }
                }catch(...){} // Advice never rejects the point or occupies the mutation slot.
                reply["RiskPlayer"]=riskPlayer;
                reply["RiskGuid"]=query->value("RiskGuid",std::string{});
                if(PS::SpawnToolRequests::PushUpdate)PS::SpawnToolRequests::PushUpdate("mapground",reply,0);
            }
        }
    }
    const bool becameAvailable=!PS::SpawnToolRequests::Available.exchange(true);
    if(becameAvailable&&PS::SpawnToolRequests::PushUpdate){static uint64_t readinessRevision=1;PS::SpawnToolRequests::PushUpdate("readiness",{{"Status","Spawn tools ready."}},++readinessRevision);}
    if(g_quickCatalogIndex.active) {
        if(auto query=PS::SpawnToolRequests::TakeSearchHint())g_quickCatalogIndex.Prioritize(*query);
    }
    const auto runCatalogWindow=[&](const nlohmann::json& queryRequest)->nlohmann::json {
        const int category=queryRequest.value("Category",-1);const auto mode=queryRequest.value("Mode",std::string("Search"));
        const auto query=queryRequest.value("Query",std::string{}),cursor=queryRequest.value("Cursor",std::string{}),fieldSet=queryRequest.value("FieldSet",std::string("BasicRow"));
        const int requestedLimit=queryRequest.value("Limit",24);const std::size_t limit=static_cast<std::size_t>(std::clamp(requestedLimit,1,128));
        const bool reset=queryRequest.value("Reset",false);const auto generation=PS::SpawnToolRequests::Generation.load();
        static std::deque<std::pair<std::string,nlohmann::json>> pageCache;
        const auto cacheKey=PS::HelpySourceCache::PublishedFingerprint()+"|"+std::to_string(generation)+"|"+std::to_string(category)+"|"+mode+"|"+fieldSet+"|"+query+"|"+cursor+"|"+std::to_string(limit)+"|"+std::to_string(queryRequest.value("Seed",uint64_t{}));
        nlohmann::json page;bool cacheHit=false;
        if(mode!="Alphabetical")for(const auto& [key,value]:pageCache)if(key==cacheKey){page=value;cacheHit=true;break;}
        if(!cacheHit) {
            if(category==0) {
                if(mode=="Random") {
                    std::uint64_t start=queryRequest.value("Seed",uint64_t{});std::size_t offset=0;ToolRandomCursor(cursor,start,offset);
                    page=PS::AssetSearch::ItemWindow({},offset,limit,start,true,fieldSet,2048,1000);
                    const auto actualStart=page.value("Start",std::size_t{}),next=page.value("NextOffset",std::size_t{});
                    page["NextCursor"]="R:"+std::to_string(actualStart)+":"+std::to_string(next);
                } else if(mode=="Alphabetical") {
                    page=PS::AssetSearch::AlphabeticalItemWindow(query,cursor,limit,fieldSet);
                } else if(mode=="Search") {
                    const auto offset=ToolCursorOffset(cursor,'S');page=PS::AssetSearch::ItemWindow(query,offset,limit,0,false,fieldSet,4096,1800);
                    page["NextCursor"]="S:"+std::to_string(page.value("NextOffset",std::size_t{}));
                } else throw std::runtime_error("Unknown item catalog window mode");
            } else if(category==1||category==2) {
                if(mode!="Search")throw std::runtime_error("AI/resource catalog windows support Search mode only");
                static std::size_t cachedEvents=static_cast<std::size_t>(-1),cachedSpawns=static_cast<std::size_t>(-1);
                static nlohmann::json authoredBase=nlohmann::json::array();
                if(cachedEvents!=m_eventTemplates.size()||cachedSpawns!=m_spawns.size()) {
                    authoredBase=nlohmann::json::array();cachedEvents=m_eventTemplates.size();cachedSpawns=m_spawns.size();
                    for(const auto& [key,definition]:m_eventTemplates)if(!key.starts_with("__RuneSchemaTools:"))authoredBase.push_back({{"Key",key},{"Name",definition.Name},{"Class",definition.Class},{"Type","AI"},{"RuneSchemaManaged",true},{"StableId",ToolStableId("AI:"+definition.Class)}});
                    for(const auto& spawn:m_spawns)if(spawn.Type!=ESpawnEntryType::RemoveActor){const auto type=std::string(spawn.Type==ESpawnEntryType::Actor?"Resource":"AI"),path=RC::to_string(spawn.Type==ESpawnEntryType::Actor?spawn.ClassPath:spawn.AIClassPath);authoredBase.push_back({{"Key",RC::to_string(spawn.ModName)+":"+RC::to_string(spawn.EntryId)},{"Name",spawn.DisplayName},{"Class",path},{"Type",type},{"RuneSchemaManaged",true},{"StableId",ToolStableId(type+":"+path)}});}
                    pageCache.clear();
                }
                auto authored=authoredBase;std::set<std::string> known;
                for(const auto& row:authored)known.insert(row.value("Class",std::string{}));
                for(auto row:ToolMountedDefinitionHints())if(known.insert(row.value("Class",std::string{})).second)authored.push_back(std::move(row));
                if(auto* npcs=DragonWildsNpcLoader::HelpyInstance){const auto roster=npcs->HelpyDefinitions();for(auto row:roster){const auto type=row.value("Type",std::string("NPC")),path=row.value("Class",row.value("Key",std::string{}));row["StableId"]=ToolStableId(type+":"+path);authored.push_back(std::move(row));}}
                const auto offset=ToolCursorOffset(cursor,'S');page=ToolDefinitionWindow(authored,category,query,offset,limit,fieldSet,4096,1800);
                page["NextCursor"]="S:"+std::to_string(page.value("NextOffset",std::size_t{}));
            } else throw std::runtime_error("Unknown catalog window category");
            if(mode!="Alphabetical"){pageCache.push_back({cacheKey,page});while(pageCache.size()>32)pageCache.pop_front();}
        }
        nlohmann::json rows=page.value("Rows",nlohmann::json::array()),dictionary=nlohmann::json::object();
        if(category==0&&rows.is_array()) {
            static uint64_t iconDictionaryGeneration=0;static std::map<uint64_t,std::string> announcedIcons;
            if(iconDictionaryGeneration!=generation){iconDictionaryGeneration=generation;announcedIcons.clear();}
            dictionary["Icons"]=nlohmann::json::array();
            for(auto& row:rows)if(row.is_object()) {
                const auto icon=row.value("Icon",std::string{});if(icon.empty())continue;const auto id=ToolStableId("Icon:"+icon);
                const auto prior=announcedIcons.find(id);if(prior!=announcedIcons.end()&&prior->second!=icon)continue;
                row["IconId"]=id;if(prior==announcedIcons.end()){announcedIcons[id]=icon;dictionary["Icons"].push_back({{"Id",id},{"Value",icon}});}row.erase("Icon");
            }
            if(dictionary["Icons"].empty())dictionary.erase("Icons");
        }
        nlohmann::json window={{"RequestId",queryRequest.value("RequestId",uint64_t{})},{"CancelGeneration",queryRequest.value("CancelGeneration",uint64_t{})},
            {"RuntimeGeneration",generation},{"Category",category},{"Mode",mode},{"Query",query},{"Reset",reset||page.value("Reset",false)},{"FieldSet",fieldSet},
            {"Rows",std::move(rows)},{"NextCursor",page.value("NextCursor",std::string{})},{"HasMore",page.value("HasMore",false)},
            {"Scanned",page.value("Scanned",std::size_t{})},{"BudgetExhausted",page.value("BudgetExhausted",false)},{"CacheHit",cacheHit}};
        PS::CatalogWindowProgress::Forward(window,page);
        window["Coverage"]=page.value("Coverage",PS::CookedAssets::Available()?"Loaded native objects + mounted-content metadata. Unloaded candidates are validated on use.":"Loaded native objects only: mounted registry metadata is not available in this session.");
        if(mode=="Alphabetical"){
            window["AlphabeticalReady"]=page.value("AlphabeticalReady",false);window["IndexedItems"]=page.value("IndexedItems",size_t{});
        }
        if(!dictionary.empty())window["Dictionary"]=std::move(dictionary);
        return {{"CatalogWindow",std::move(window)},{"Status","Loaded a bounded Helpy query window on demand."}};
    };
    // Catalogue definitions are process-owned, not world actors. A dedicated
    // loader can miss the initial world callback; never strand accepted reads
    // behind m_readyWorld, a local player or GPU readiness. Mutations below
    // retain their independent world, player and authority checks.
    if(work.MayStart()&&!PS::SpawnToolRequests::Waiting.load()) {
        if(PS::HelpyQueryService::HasPending()) {
            work.Started();PS::HelpyQueryService::PumpOne(false,runCatalogWindow);
        }else if(g_quickCatalogIndex.active)g_quickCatalogIndex.Tick(work);
        else {work.Started();PS::AssetSearch::PumpItemCatalog();}
    }
    auto request=PS::SpawnToolRequests::Take();if(!request)return;
    const bool f2Spawn=request->value("_Source",std::string{}).starts_with("Helpy")&&request->value("Action",std::string{})=="Spawn";
    const auto requestId=request->value("_RequestId",uint64_t{});
    const char* spawnStage="recipient selection";
    try {
        const auto action=request->at("Action").get<std::string>();
        if(action=="CancelCatalogIndex") {
            g_quickCatalogIndex.Stop("Full scan cancelled; discovered entries retained.");
            auto result=PS::SpawnToolRequests::Read();result["Status"]="Full scan cancelled; discovered entries retained.";
            PS::SpawnToolRequests::Publish(std::move(result));return;
        }
        if(action=="InspectSpawnDefinition") {
            const auto path=request->at("Class").get<std::string>(),expected=request->at("ExpectedType").get<std::string>();
            if(!PS::F2Catalog::ValidObjectPath(path)||(expected!="AI"&&expected!="NPC"&&expected!="Resource"))throw std::runtime_error("Invalid native definition inspection");
            auto* type=ActorHelper::ResolveClass(RC::to_generic_string(path));
            auto* actors=ActorHelper::ResolveClass(TEXT("/Script/Engine.Actor"));auto* ais=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionAICharacter"));
            auto* npcs=ActorHelper::ResolveClass(TEXT("/Script/Dominion.InteractableNPC"));
            if(!type||!actors||!ais)throw std::runtime_error("Native class could not be loaded");
            const bool npc=npcs&&type->IsChildOf(npcs),ai=!npc&&type->IsChildOf(ais);
            if((expected=="NPC"&&!npc)||(expected=="AI"&&!ai)||(expected=="Resource"&&(npc||ai||!ToolResourceClass(type,actors,ais))))throw std::runtime_error("Mounted candidate does not have the expected native ancestry");
            auto row=ToolDefinitionRow(type,request->at("Key").get<std::string>(),ai,npc?"NPC":"");row["NeedsInspection"]=false;
            auto result=PS::SpawnToolRequests::Read();result["SpawnDefinition"]=std::move(row);result["Status"]="Native definition inspected; no actor spawned.";PS::SpawnToolRequests::Publish(std::move(result));return;
        }
        if(f2Spawn)PS::Log<LogLevel::Verbose>(STR("RuneSchema Helpy spawn #{} received by SpawnTools.\n"),requestId);
        if(action=="ExportClone") {
            ExportCloneDraft(request->at("Draft"));
            auto result=PS::SpawnToolRequests::Read();result["Status"]="Verified /assets clone exported. Install it in a mod and restart before use.";
            PS::SpawnToolRequests::Publish(std::move(result));return;
        }
        if(action=="ClearPlacements") {
            m_toolPlacements=nlohmann::json::array();
            auto result=PS::SpawnToolRequests::Read();
            result["Status"]="Authored placements cleared; live actors unchanged.";
            PS::SpawnToolRequests::Publish(std::move(result));return;
        }
        if(action=="Export") {
            if(m_toolPlacements.empty())throw std::runtime_error("No authored placements; spawn actors first");
            const auto exported=PS::SpawnAuthoring::ResolveGroundDrafts(m_toolPlacements);
            const auto directory=PS::HostServices::ExportsDirectory()/"spawns";fs::create_directories(directory);
            const auto stamp=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            const auto file=directory/("authored_spawns_"+std::to_string(stamp)+".json");const auto text=exported.dump(2);
            HANDLE handle=CreateFileW(file.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create unique spawn export");
            DWORD written=0;const bool ok=WriteFile(handle,text.data(),static_cast<DWORD>(text.size()),&written,nullptr) && written==text.size();CloseHandle(handle);
            if(!ok)throw std::runtime_error("Spawn export write failed");
            std::ifstream read(file);if(nlohmann::json::parse(read)!=exported)throw std::runtime_error("Spawn export verification failed");
            auto result=PS::SpawnToolRequests::Read();result["Status"]="Exported "+std::to_string(exported.size())+" grounded placements with resolved Z coordinates to "+file.string();PS::SpawnToolRequests::Publish(result);return;
        }
        // Inspection is read-only and does not target a player, mutate an
        // inventory, or spawn into a world. Keep it ahead of controller roster
        // and authority validation so Item Details and Item Lab remain useful
        // while no authoritative recipient is selected or available.
        if(action=="ValidateCartItems") {
            const auto& paths=request->at("Items");
            if(!paths.is_array()||paths.empty()||paths.size()>4)throw std::runtime_error("Cart checks require 1..4 item paths");
            auto rows=nlohmann::json::array();auto* itemType=ActorHelper::ResolveClass(TEXT("/Script/Dominion.ItemData"));
            for(const auto& path:paths){
                if(!path.is_string()||!PS::ItemCart::KitPath(path.get<std::string>()))throw std::runtime_error("Cart check requires canonical item paths");
                bool valid=false;std::string message="Item is unavailable in this runtime.";
                try{auto* item=ActorHelper::ResolveObject(RC::to_generic_string(path.get<std::string>()));
                    valid=item&&itemType&&item->IsA(itemType)&&!PS::AssetMetadata::IsIncomplete(item);
                    if(valid)message="ItemData verified in the current runtime. Inventory and quantity are checked again at delivery.";
                }catch(const std::exception& error){message=error.what();}
                rows.push_back({{"Item",path},{"Valid",valid},{"Message",message}});
            }
            auto result=PS::SpawnToolRequests::Read();result["CartValidation"]=std::move(rows);result["Status"]="Checked staged item paths; no inventory changes.";
            PS::SpawnToolRequests::Publish(std::move(result));return;
        }
        if(action=="InspectKitItems") {
            const auto& paths=request->at("Items");if(!paths.is_array()||paths.empty()||paths.size()>PS::ItemCart::Model::Limit)throw std::runtime_error("Kit metadata requires 1..64 item paths");
            auto rows=nlohmann::json::array();
            for(const auto& path:paths){if(!path.is_string()||!PS::ItemCart::KitPath(path.get<std::string>()))throw std::runtime_error("Kit metadata requires canonical item paths");rows.push_back(PS::AssetSearch::ItemMetadata(path.get<std::string>()));}
            auto result=PS::SpawnToolRequests::Read();result["KitMetadata"]=std::move(rows);result["Status"]="Loaded item metadata for the staged kit; no items delivered.";PS::SpawnToolRequests::Publish(std::move(result));return;
        }
        if(action=="InspectUnlockEquipContracts") {
            auto report=InspectNativeUnlockEquipContracts();
            const auto directory=PS::HostServices::ExportsDirectory()/"native-contracts";
            const auto stamp=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            const auto file=directory/("unlock-equip-"+std::to_string(stamp)+".json");PS::ConfigFiles::Write(file,report.dump(2));
            auto result=PS::SpawnToolRequests::Read();result["NativeContracts"]=std::move(report);
            result["Status"]="Read-only native equip/unlock signatures exported: "+file.string();PS::SpawnToolRequests::Publish(std::move(result));return;
        }
        if(action=="PrepareClone") {
            auto* loader=DragonWildsAssetModLoader::AuthoringInstance;
            if(!loader)throw std::runtime_error("The assets loader must be enabled before preparing item exports");
            auto draft=*request;draft["Permanent"]=true;draft["Give"]=false;
            auto result=PS::SpawnToolRequests::Read();result["CloneResult"]=loader->CreateToolClone(draft,m_readyWorld);
            result["Status"]="RuneSchema JSONC prepared. No item created, installed or given.";
            PS::SpawnToolRequests::Publish(std::move(result));return;
        }
        if(action=="InspectClone") {
            auto* loader=DragonWildsAssetModLoader::AuthoringInstance;
            if(!loader)throw std::runtime_error("The assets loader must be enabled before inspecting items");
            const bool details=request->value("InspectDetails",false);
            auto result=PS::SpawnToolRequests::Read();
            result[details?"ItemDetails":request->value("InspectAppearance",false)?"CloneAppearance":"CloneSource"]=
                loader->InspectToolClone(request->at("Source").get<std::string>(),!details);
            result["CookedVisuals"]=PS::CookedAssets::VisualRoster();
            PS::SpawnToolRequests::CatalogProgress({{"CookedVisuals",result["CookedVisuals"]}});
            result["Status"]=details?"Item details loaded. Player selection is only required for inventory actions.":
                "Clone source inspected. Edit item fields and linked table stats; source data remains unchanged.";
            PS::SpawnToolRequests::Publish(std::move(result));return;
        }
        if(action=="InspectContainerInventory"){
            if(!m_readyWorld||!GetGameMode(m_readyWorld)||request->value("Generation",uint64_t{})!=PS::SpawnToolRequests::CurrentGeneration())throw std::runtime_error("Chest inspection requires the current authoritative host/server world");
            const auto index=request->value("ObjectIndex",-1),serial=request->value("ObjectSerial",0),count=FUObjectArray::GetNumElements();
            if(index<0||index>=count||count>4'000'000||serial<=0)throw std::runtime_error("Storage identity is invalid or unloaded");
            PS::WeakObjectHandle identity;identity.ObjectIndex=index;identity.ObjectSerialNumber=serial;auto* actor=identity.Get();
            if(!actor||actor->GetWorld()!=m_readyWorld||RC::to_string(actor->GetPathName())!=request->value("Player",std::string{})||actor->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)))throw std::runtime_error("Storage is unloaded or replaced; contents unavailable, not empty");
            auto* inventory=PS::PlayerInventoryPresentation::Storage(actor);if(!inventory)throw std::runtime_error("Loaded building has no unambiguous owned storage inventory");
            auto snapshot=PS::PlayerInventoryPresentation::ReadInventory(inventory,*request);snapshot["Player"]=request->at("Player");snapshot["PlayerGuid"]="";snapshot["Generation"]=PS::SpawnToolRequests::CurrentGeneration();snapshot["Container"]=true;
            snapshot["Icon"]=PS::AssetSearch::PresentationIcon(actor);snapshot["Name"]=PS::AssetSearch::PresentationName(actor);
            // Network ownership and player default-privacy settings do not prove
            // this particular chest's lock owner or grant deposit permission.
            snapshot["LockState"]="Unknown";snapshot["LockOwner"]="";snapshot["CanDeposit"]=false;snapshot["CanOverrideUnlock"]=false;
            snapshot["AccessReason"]="Chest-specific ownership, access and persistence contracts have not been verified for this runtime.";
            PS::SpawnToolRequests::Publish({{"InventorySnapshot",snapshot},{"Status","Read loaded placed-storage contents; no items changed."}});return;
        }
        if(action=="JournalCatalog") {
            auto result=nlohmann::json{{"JournalCatalog",PS::JournalPresentation::Window(*request)},{"Status","Read-only native journal slice; no character progress changed."}};
            PS::SpawnToolRequests::Publish(std::move(result));return;
        }
        if(action=="RecipeWorkspaceCatalog"||action=="InspectRecipe") {
            auto* loader=DragonWildsAssetModLoader::AuthoringInstance;
            if(!loader)throw std::runtime_error("The assets loader must be enabled before recipe authoring");
            auto result=PS::SpawnToolRequests::Read();
            result[action=="InspectRecipe"?"RecipeWorkspaceSource":"RecipeWorkspaceCatalog"]=loader->RecipeWorkspaceSnapshot(*request);
            result["Status"]="Recipe authoring data read from native assets; no recipe changed.";
            PS::SpawnToolRequests::Publish(std::move(result));return;
        }
        if(action=="ExportRecipe"||action=="ExportItemOverrides") {
            auto* loader=DragonWildsAssetModLoader::AuthoringInstance;
            if(!loader)throw std::runtime_error("The assets loader must be enabled before exporting item authoring files");
            auto result=PS::SpawnToolRequests::Read();
            if(action=="ExportRecipe") {
                result["RecipeExportResult"]=loader->ExportToolRecipe(*request);
                result["Status"]="Recipe JSON installed. Reload its loader or restart before testing it.";
            }else {
                result["ItemOverrideExportResult"]=loader->ExportToolOverrides(*request);
                result["Status"]="Item override JSON installed with direct fields in /assets and linked rows in /raw. Restart before testing it.";
            }
            PS::SpawnToolRequests::Publish(std::move(result));return;
        }
        if(action=="CatalogWindow") {auto result=runCatalogWindow(*request);PS::SpawnToolRequests::Publish(std::move(result));return;}
        auto* controllerClass=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerControllerBase"));
        if(!controllerClass)throw std::runtime_error("Load a world first");
        TArray<UObject*> controllers;UECustom::UObjectGlobals::GetObjectsOfClass(controllerClass,controllers,true);
        if(controllers.Num()<0||controllers.Num()>256)throw std::runtime_error("Player roster exceeds the safe bound");
        nlohmann::json players=nlohmann::json::array(),definitions=nlohmann::json::array(),items=nlohmann::json::array(),buildings=nlohmann::json::array();
        UObject* selected=nullptr;
        // A dedicated world has no local player and may have no connected
        // players yet. Read-only cataloguing must not depend on either.
        UWorld* catalogWorld=m_readyWorld&&IsWorldStillLoaded(m_readyWorld)?m_readyWorld:nullptr;
        for(auto* controller:controllers) {
            if(!controller || controller->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)) || !controller->GetWorld())continue;
            if(action=="QuickPlayers"&&controller->GetWorld()!=m_readyWorld)continue;
            if(action=="MapSnapshot"&&controller->GetWorld()!=m_readyWorld)continue;
            const auto requestedWorld=request->value("World",std::string{});
            if(!requestedWorld.empty()&&RC::to_string(controller->GetWorld()->GetPathName())!=requestedWorld)continue;
            if(!catalogWorld)catalogWorld=controller->GetWorld();
            if(!GetGameMode(controller->GetWorld()))continue;
            auto* pawn=ActorHelper::GetObjectRef(controller,TEXT("Pawn"));
            if(!pawn || !pawn->IsA<AActor>())continue;
            if(!catalogWorld)catalogWorld=controller->GetWorld();
            auto name=std::string("Player");
            if(auto* state=ActorHelper::GetObjectRef(controller,TEXT("PlayerState"))) {
                ActorHelper::FunctionCall call(state,TEXT("/Script/Engine.PlayerState:GetPlayerName"));call.Invoke();
                const auto value=call.Result<FString>();const auto& chars=value.GetCharArray();
                if(chars.Num()>1 && chars.Num()<256 && chars.GetData() && chars.GetData()[chars.Num()-1]==0)name=RC::to_string(RC::StringType(chars.GetData(),chars.Num()-1));
            }
            const auto path=RC::to_string(controller->GetPathName());
            bool isLocal=false;
            try{ActorHelper::FunctionCall local(controller,TEXT("/Script/Engine.Controller:IsLocalController"));local.Invoke();isLocal=local.Result<bool>();}catch(...){}
            auto playerRow=nlohmann::json{{"Name",name},{"Path",path},{"Guid",GetPlayerCharacterGuid(controller)},
                {"IsLocal",isLocal},
                {"PlayerId",PS::PlayerStatusRuntime::PlayerId(ActorHelper::GetObjectRef(controller,TEXT("PlayerState")))},
                {"Platform",PS::PlayerStatusRuntime::Platform(ActorHelper::GetObjectRef(controller,TEXT("PlayerState")),isLocal)},{"PlatformAccountName",PS::PlayerStatusRuntime::ControllerAccountName(controller)},
                {"RoleStatus",action=="QuickPlayers"?PS::PlayerStatusRuntime::Read(controller):nlohmann::json::object()}};
            PS::PlayerStatusRuntime::Color(ActorHelper::GetObjectRef(controller,TEXT("PlayerState")),playerRow);
            try{if(auto* playerPawn=static_cast<AActor*>(ActorHelper::GetObjectRef(controller,TEXT("Pawn")))){
                const auto position=ActorHelper::GetActorLocation(playerPawn);playerRow["Location"]={position.X(),position.Y(),position.Z()};
                if(action=="MapSnapshot"||action=="QuickPlayers"){playerRow["Stats"]=PS::PlayerVitals::Details(playerPawn,true);playerRow["Health"]=playerRow["Stats"]["Health"];playerRow["Equipment"]=PS::PlayerLoadoutPresentation::Cached(controller,playerRow.value("Guid",std::string{}),PS::SpawnToolRequests::CurrentGeneration());}
            }}catch(...){}
            players.push_back(std::move(playerRow));
            if(path==request->value("Player",std::string{})){if(selected)throw std::runtime_error("Player selection is ambiguous; refresh");selected=controller;}
        }
        if(action=="MapBuildings"){
            auto result=PS::SpawnToolRequests::Read();result["MapBuildings"]=PS::HelpyMapRuntime::Buildings(request->value("Query",std::string{}),request->value("Offset",size_t{}));result["Status"]="Read the game's building menu.";PS::SpawnToolRequests::Publish(std::move(result));return;
        }
        if(action=="MapProbe"){
            auto result=PS::SpawnToolRequests::Read();
            try{const auto ground=ToolMapGround(m_readyWorld,*request);result["MapPoint"]={{"GroundAvailable",true},{"Location",{ground.X(),ground.Y(),ground.Z()}}};}
            catch(const std::exception& error){result["MapPoint"]={{"GroundAvailable",false},{"Error",error.what()}};}
            result["Status"]="Read coordinates at the selected map point.";PS::SpawnToolRequests::Publish(std::move(result));return;
        }
        if(action=="MapSnapshot"){
            auto result=PS::SpawnToolRequests::Read();
            try{
                auto& map=result["MapSnapshot"];map=PS::HelpyMapRuntime::Snapshot(m_readyWorld,players);map["CanBuildAndTeleport"]=GetGameMode(m_readyWorld)!=nullptr;
                map["PartyTravelVersion"]=2;
                bool scanning=false;auto markers=ToolMapNativeMarkers(m_readyWorld,scanning,*request,map["Players"]);std::unordered_map<std::string,size_t> identities;
                for(size_t i=0;i<markers.size();++i)identities[markers[i].at("Path").get<std::string>()]=i;
                const auto append=[&](nlohmann::json row){if(row.is_null())return;const auto key=row.at("Path").get<std::string>();if(auto it=identities.find(key);it!=identities.end()){
                    const auto& previous=markers[it->second];if(previous.contains("ResourceObservation")&&!row.contains("ResourceObservation"))row["ResourceObservation"]=previous["ResourceObservation"];
                    markers[it->second]=std::move(row);
                }else if(markers.size()<8192){identities[key]=markers.size();markers.push_back(std::move(row));}};
                for(const auto& ref:m_toolActors)if(auto* actor=ref.Get()){
                    auto kind=ToolMapActorKind(actor->GetClassPrivate());if(kind.empty())kind="Actor";if(!request->value("Placements",true)&&!(kind=="Lodestone"&&request->value("Lodestones",true)))continue;append(ToolMapActor(actor,m_readyWorld,kind,true));
                }
                for(const auto& spawn:m_spawns){
                    if(spawn.Type==ESpawnEntryType::RemoveActor||spawn.bDeconstructed)continue;
                    const std::string kind=ToolMapLodestone(RC::to_string(spawn.ClassPath)+spawn.BuildingDataPath)?"Lodestone":spawn.Type==ESpawnEntryType::AISpawnPoint?"AI":spawn.bBuildingProp?"Building":"Resource";
                    if(!request->value("Placements",true)&&!(kind=="Lodestone"&&request->value("Lodestones",true)))continue;
                    auto name=spawn.DisplayName;if(name.empty())name=ToolReadableName(RC::to_string(spawn.Type==ESpawnEntryType::AISpawnPoint?spawn.AIClassPath:spawn.ClassPath));
                    auto row=ToolMapActor(spawn.LiveActor.Get(),m_readyWorld,kind,true,name);
                    if(row.is_null()){const auto& at=spawn.Location;if(!std::isfinite(at.X())||!std::isfinite(at.Y())||!std::isfinite(at.Z()))continue;
                        row={{"Path","@placement:"+RC::to_string(spawn.ModName)+":"+RC::to_string(spawn.EntryId)},{"Name",name+" (saved placement)"},{"Kind",kind},{"Managed",true},{"Loaded",false},{"Location",{at.X(),at.Y(),at.Z()}}};}
                    append(std::move(row));
                }
                map["Markers"]=std::move(markers);map["MarkersScanning"]=scanning;map["MarkerCoverage"]="Loaded world actors and authored RuneSchema placements";
            }
            catch(const std::exception& error){result["MapSnapshot"]={{"BridgeVersion",1},{"Error",error.what()}};}
            result["Status"]="Read native world-map bounds, artwork and player positions.";PS::SpawnToolRequests::Publish(std::move(result));return;
        }
        if(action=="QuickPlayers") {
            if(catalogWorld&&PS::Network::Detect(catalogWorld).Mode==PS::Network::Role::Client) {
                const auto message=nlohmann::json{{"Action","Players"}}.dump();
                if(!ForwardHelpyAuthority||!ForwardHelpyAuthority("Players",message))
                    throw std::runtime_error("The owning-player RuneSchema roster bridge is unavailable");
                auto result=PS::SpawnToolRequests::Read();
                result["Status"]="Requested the authoritative connected-player roster.";
                PS::SpawnToolRequests::Publish(std::move(result));return;
            }
            static uint64_t playerGeneration=0;static std::map<std::string,nlohmann::json> previousPlayers;
            const auto generation=PS::SpawnToolRequests::Generation.load();const bool reset=playerGeneration!=generation;if(reset){playerGeneration=generation;previousPlayers.clear();}
            std::map<std::string,nlohmann::json> currentPlayers;for(const auto& row:players)if(row.is_object()){const auto path=row.value("Path",std::string{});if(!path.empty())currentPlayers[path]=row;}
            nlohmann::json delta={{"Reset",reset},{"Added",nlohmann::json::array()},{"Updated",nlohmann::json::array()},{"Removed",nlohmann::json::array()}};
            for(const auto& [path,row]:currentPlayers){const auto prior=previousPlayers.find(path);if(prior==previousPlayers.end())delta["Added"].push_back(row);else if(prior->second!=row)delta["Updated"].push_back(row);}
            for(const auto& [path,row]:previousPlayers)if(!currentPlayers.contains(path))delta["Removed"].push_back(path);previousPlayers=std::move(currentPlayers);
            auto result=PS::SpawnToolRequests::Read();result["Players"]=players;result["PlayersDelta"]=std::move(delta);
            result["PlayerRoles"]=PS::PlayerStatus::Snapshot(players,generation);
            PS::SpawnToolRequests::CatalogProgress({{"Players",players},{"PlayerRoles",result["PlayerRoles"]}});
            result["Status"]=std::format("Refreshed {} currently available authoritative player(s).",players.size());
            PS::SpawnToolRequests::Publish(std::move(result));return;
        }
        for(const auto& [key,definition]:m_eventTemplates)if(!key.starts_with("__RuneSchemaTools:"))definitions.push_back({{"Key",key},{"Name",definition.Name},{"Class",definition.Class},{"Type","AI"},{"RuneSchemaManaged",true}});
        for(const auto& spawn:m_spawns)if(spawn.Type!=ESpawnEntryType::RemoveActor)
            definitions.push_back({{"Key",RC::to_string(spawn.ModName)+":"+RC::to_string(spawn.EntryId)},{"Name",spawn.DisplayName},{"Class",RC::to_string(spawn.Type==ESpawnEntryType::Actor?spawn.ClassPath:spawn.AIClassPath)},{"Type",spawn.Type==ESpawnEntryType::Actor?"Resource":"AI"},{"RuneSchemaManaged",true}});
        if(auto* npcs=DragonWildsNpcLoader::HelpyInstance) {
            const auto roster=npcs->HelpyDefinitions();definitions.insert(definitions.end(),roster.begin(),roster.end());
        }
        const auto catalogDirectory=PS::HostServices::RuntimeDirectory()/"Helpy";
        if(action=="QuickCatalog"||action=="IndexQuickCatalog"||action=="UpdateHelpyReference") {
            if(g_quickCatalogIndex.active) {
                auto result=g_quickCatalogIndex.Snapshot("Index already running; duplicate refresh coalesced",true);
                result["Players"]=players;result["Status"]="Index already running.";
                PS::SpawnToolRequests::CatalogProgress(result);PS::SpawnToolRequests::Publish(std::move(result));return;
            }
            try {g_quickCatalogIndex.Start(catalogWorld,definitions,players,action=="IndexQuickCatalog",action=="UpdateHelpyReference");}
            catch(...) {g_quickCatalogIndex.Stop("Refresh could not start; inspect the action status and Coverage.");throw;}
            return;
        }
        if(action=="Catalog") {
            auto ai=ToolCatalog(catalogDirectory/"Helpy-catalog-ai.json","AI");
            auto resources=ToolCatalog(catalogDirectory/"Helpy-catalog-resources.json","Resource");
            items=ToolCatalog(catalogDirectory/"Helpy-catalog-items.json","Item");
            if(std::filesystem::is_regular_file(catalogDirectory/"Helpy-catalog-buildings.json"))
                buildings=ToolCatalog(catalogDirectory/"Helpy-catalog-buildings.json","Building");
            // Building safety is deliberately world-specific.  A piece that was
            // proven against a streamed native instance yesterday is not trusted
            // after a world/map change.  Keep the saved rows for inspection, but
            // force an explicit in-world rebuild before any building can spawn.
            for(auto& row:buildings) {
                row["Safe"]=false;
                row["SafetyReason"]="Saved building safety is stale; rebuild catalogs in the current world to verify a native instance";
            }
            // A saved catalog is a complete point-in-time snapshot.  Do not
            // merge the currently loaded definitions into it: that produced
            // duplicate choices and made the cache silently change between
            // explicit rebuilds.
            definitions=nlohmann::json::array();
            definitions.insert(definitions.end(),ai.begin(),ai.end());
            definitions.insert(definitions.end(),resources.begin(),resources.end());
            if(std::filesystem::is_regular_file(catalogDirectory/"Helpy-catalog-npcs.json")) {
                const auto npcs=ToolCatalog(catalogDirectory/"Helpy-catalog-npcs.json","NPC");definitions.insert(definitions.end(),npcs.begin(),npcs.end());
            }
            PS::SpawnToolRequests::Publish({{"Players",players},{"Definitions",definitions},{"Items",items},{"Buildings",buildings},{"CatalogReady",true},
                {"Status",std::format("Loaded saved catalogs: {} AI/resources, {} items and {} buildings. Building rows are inspection-only until rebuilt in this world.",definitions.size(),items.size(),buildings.size())}});return;
        }
        if(action=="RebuildCatalogs") {
            auto* classType=ActorHelper::ResolveClass(TEXT("/Script/CoreUObject.Class"));
            auto* actorType=ActorHelper::ResolveClass(TEXT("/Script/Engine.Actor"));
            auto* aiType=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionAICharacter"));
            if(!classType || !actorType || !aiType)throw std::runtime_error("Loaded class roster is unavailable; enter a world first");
            TArray<UObject*> classes;UECustom::UObjectGlobals::GetObjectsOfClass(classType,classes,true);
            if(classes.Num()>32768)throw std::runtime_error("Loaded class roster exceeds 32768 entries");
            std::set<std::string> discovered;
            for(const auto& row:definitions)discovered.insert(row.value("Type",std::string{})+row.value("Class",std::string{}));
            const auto readableClassName=[](std::string value) {
                const auto slash=value.find_last_of("./");
                if(slash!=std::string::npos)value=value.substr(slash+1);
                if(value.starts_with("BP_"))value.erase(0,3);
                if(value.ends_with("_C"))value.resize(value.size()-2);
                std::ranges::replace(value,'_',' ');
                return value;
            };
            for(auto* object:classes) {
                auto* type=static_cast<UClass*>(object);
                if(!type || ActorHelper::IsAbstract(type))continue;
                const auto path=RC::to_string(type->GetPathName());
                std::string kind;
                if(HelpyNpcGuards::IsNpc(type)) {
                    if(discovered.insert("NPC"+path).second)definitions.push_back(ToolDefinitionRow(type,"@loaded-npc:"+path,false,"NPC"));
                    continue;
                }
                if(type->IsChildOf(aiType))kind="AI";
                else if(type->IsChildOf(actorType)
                    && PropertyHelper::GetPropertyByName(type,TEXT("SpudGuid"))
                    && PropertyHelper::GetPropertyByName(type,TEXT("bSkipSpudStore")))kind="Resource";
                else continue;
                if(!discovered.insert(kind+path).second)continue;
                definitions.push_back({{"Key",std::string("@loaded-")+(kind=="AI"?"ai:":"resource:")+path},
                    {"Name",readableClassName(path)},{"Class",path},{"Type",kind},{"Loaded",true}});
            }
            // The UObject list only contains classes touched by the current
            // world. An explicit catalog rebuild may also load packaged mining
            // nodes so ore/stone variants are available before their cell has
            // streamed. Limit this to mining packages and validate the live
            // class contract before admitting an entry.
            {
                auto registryInterface=UAssetRegistryHelpers::GetAssetRegistry();
                auto* registry=static_cast<UAssetRegistry*>(registryInterface.ObjectPointer);
                TArray<FAssetData> assets;
                if(registry && registry->GetAllAssets(assets,true) && assets.Num()<=262144) {
                    size_t candidates=0;
                    for(auto& asset:assets) {
                        const auto package=RC::to_string(asset.PackageName().ToString());
                        const auto assetName=RC::to_string(asset.AssetName().ToString());
                        if(package.find("/Gameplay/World/Mining/")==std::string::npos)continue;
                        if(!(assetName.starts_with("BP_OreNode_") || assetName.starts_with("BP_MiningRock_")))continue;
                        if(++candidates>2048)throw std::runtime_error("Packaged mining resource catalog exceeds 2048 candidates");
                        const auto path=package+"."+assetName+"_C";
                        auto* type=ActorHelper::ResolveClass(RC::to_generic_string(path));
                        if(!type || ActorHelper::IsAbstract(type) || !type->IsChildOf(actorType)
                            || !PropertyHelper::GetPropertyByName(type,TEXT("SpudGuid"))
                            || !PropertyHelper::GetPropertyByName(type,TEXT("bSkipSpudStore")))continue;
                        if(!discovered.insert("Resource"+path).second)continue;
                        definitions.push_back({{"Key","@catalog-resource:"+path},{"Name",readableClassName(path)},
                            {"Class",path},{"Type","Resource"},{"Packaged",true}});
                    }
                }
            }
            items=PS::AssetSearch::ItemRoster();
            auto* buildingType=ActorHelper::ResolveClass(TEXT("/Script/Dominion.BuildingPieceData"));
            if(!buildingType)throw std::runtime_error("BuildingPieceData roster is unavailable; enter a world first");
            TArray<UObject*> buildingObjects;UECustom::UObjectGlobals::GetObjectsOfClass(buildingType,buildingObjects,true);
            if(buildingObjects.Num()>32768)throw std::runtime_error("Loaded building roster exceeds 32768 entries");
            std::set<std::string> buildingPaths;
            size_t safeBuildingCount=0;
            for(auto* object:buildingObjects) {
                if(!object || object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)))continue;
                const auto path=RC::to_string(object->GetPathName());if(path.empty() || !buildingPaths.insert(path).second)continue;
                nlohmann::json row={{"Path",path},{"Name",readableClassName(path)},{"Safe",false}};
                try {
                    auto authored=nlohmann::json{{"Id","catalog-preflight"},{"Type","BuildingProp"},{"Building",path},
                        {"Scale",1.0},{"AllowDeconstruction",false},{"UseNativeRespawn",false},
                        {"Location",nlohmann::json::array({0,0,0})},{"Rotation",{{"Pitch",0},{"Yaw",0},{"Roll",0}}}};
                    SpawnInfo prototype;prototype.ModName=TEXT("__RuneSchemaTools");prototype.EntryId=TEXT("catalog-preflight");
                    RegisterBuildingProp(prototype,authored);
                    const auto [safe,reason]=VerifyToolBuildingCandidate(object,prototype,catalogWorld,false);
                    const auto [temporarySafe,temporaryReason]=VerifyToolBuildingCandidate(object,prototype,catalogWorld,true);
                    row["TemporarySafe"]=temporarySafe;row["TemporarySafetyReason"]=temporaryReason;
                    row["Safe"]=safe;row["SafetyReason"]=reason;row["ActorClass"]=RC::to_string(prototype.ClassPath);
                    if(safe)++safeBuildingCount;
                } catch(const std::exception& error) {
                    row["SafetyReason"]=error.what();
                }
                buildings.push_back(std::move(row));
            }
            std::sort(definitions.begin(),definitions.end(),[](const auto& left,const auto& right){
                return left.value("Name",std::string{})<right.value("Name",std::string{});
            });
            std::sort(buildings.begin(),buildings.end(),[](const auto& left,const auto& right){
                return left.value("Name",std::string{})<right.value("Name",std::string{});
            });
            fs::create_directories(catalogDirectory);
            const auto stamp=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            nlohmann::json ai=nlohmann::json::array(),resources=nlohmann::json::array(),npcs=nlohmann::json::array();
            for(const auto& row:definitions){const auto kind=row.value("Type",std::string{});if(kind=="AI")ai.push_back(row);else if(kind=="NPC")npcs.push_back(row);else if(kind=="Resource")resources.push_back(row);}
            PS::ConfigFiles::Write(catalogDirectory/"Helpy-catalog-npcs.json",nlohmann::json{{"SchemaVersion",1},{"Kind","NPC"},{"GeneratedUnixSeconds",stamp},{"Entries",npcs}}.dump(2));
            PS::ConfigFiles::Write(catalogDirectory/"Helpy-catalog-ai.json",nlohmann::json{{"SchemaVersion",1},{"Kind","AI"},{"GeneratedUnixSeconds",stamp},{"Entries",ai}}.dump(2));
            PS::ConfigFiles::Write(catalogDirectory/"Helpy-catalog-resources.json",nlohmann::json{{"SchemaVersion",1},{"Kind","Resource"},{"GeneratedUnixSeconds",stamp},{"Entries",resources}}.dump(2));
            PS::ConfigFiles::Write(catalogDirectory/"Helpy-catalog-items.json",nlohmann::json{{"SchemaVersion",1},{"Kind","Item"},{"GeneratedUnixSeconds",stamp},{"Entries",items}}.dump(2));
            PS::ConfigFiles::Write(catalogDirectory/"Helpy-catalog-buildings.json",nlohmann::json{{"SchemaVersion",1},{"Kind","Building"},{"GeneratedUnixSeconds",stamp},{"Entries",buildings}}.dump(2));
            const auto rosterFile=catalogDirectory/"Helpy-catalog-authoring.json";
            PS::ConfigFiles::Write(rosterFile,nlohmann::json{{"GeneratedUnixSeconds",stamp},{"Definitions",definitions},{"Items",items},{"Buildings",buildings}}.dump(2));
            PS::SpawnToolRequests::Publish({{"Players",players},{"Definitions",definitions},{"Items",items},{"Buildings",buildings},
                {"CatalogReady",true},{"Status",players.empty()?"Catalogs updated, but no authoritative players are available. Building previews remain blocked until rebuilt with an authoritative world.":
                    std::format("Catalogs overwritten: {} AI/resources, {} items, {} buildings ({} verified safe / {} blocked). {}",definitions.size(),items.size(),buildings.size(),safeBuildingCount,buildings.size()-safeBuildingCount,catalogDirectory.string())}});return;
        }
        if(!selected&&catalogWorld&&PS::Network::Detect(catalogWorld).Mode==PS::Network::Role::Client
            &&(action=="GiveItems"||action=="Spawn")) {
            if(!ForwardHelpyAuthority||!ForwardHelpyAuthority(action,request->dump()))
                throw std::runtime_error("The owning-player RuneSchema authority bridge is unavailable");
            auto result=PS::SpawnToolRequests::Read();
            result["Status"]="Request submitted to server authority; the server will apply its Helpy permission policy.";
            PS::SpawnToolRequests::Publish(std::move(result));return;
        }
        if(!selected)throw std::runtime_error("Selected authoritative player is unavailable; refresh");
        auto* world=selected->GetWorld();
        if(!world||!GetGameMode(world))throw std::runtime_error("Only the authoritative server may run spawn tools");
        if(action=="RestorePlayerGear"||action=="ChangePlayerGear"){
            if(world!=m_readyWorld||request->value("Generation",uint64_t{})!=PS::SpawnToolRequests::CurrentGeneration()||request->value("PlayerGuid",std::string{}).empty()||request->value("PlayerGuid",std::string{})!=GetPlayerCharacterGuid(selected))throw std::runtime_error("Gear target/world changed; refresh the party member");
            const auto outcome=action=="RestorePlayerGear"?PS::PlayerGearActions::Restore(selected,*request):PS::PlayerGearActions::Change(selected,*request);
            PS::SpawnToolRequests::Publish({{"EquipmentAction",outcome},{"Status",outcome.at("Status")}});return;
        }
        if(action=="InspectPlayerInventory"){
            if(world!=m_readyWorld)throw std::runtime_error("Inventory target is not in the current ready world");
            if(request->value("Generation",uint64_t{})!=PS::SpawnToolRequests::CurrentGeneration()||request->value("PlayerGuid",std::string{}).empty()||request->value("PlayerGuid",std::string{})!=GetPlayerCharacterGuid(selected))throw std::runtime_error("Inventory target/world changed; refresh the party member");
            auto snapshot=PS::PlayerInventoryPresentation::Read(selected,*request);snapshot["Player"]=request->at("Player");snapshot["PlayerGuid"]=request->at("PlayerGuid");snapshot["Generation"]=PS::SpawnToolRequests::CurrentGeneration();
            PS::SpawnToolRequests::Publish({{"InventorySnapshot",snapshot},{"Status","Read native player inventory; no items or save files changed."}});return;
        }
        if(action=="MapTeleport"){
            auto* pawn=static_cast<AActor*>(ActorHelper::GetObjectRef(selected,TEXT("Pawn")));if(!pawn)throw std::runtime_error("Selected player has no active character");
            if(request->contains("TargetPlayer")||request->value("PartyPoint",false)){
                // This branch is deliberately AFTER the authoritative-world gate.
                // Resolve current identities/positions, never accept cached UI coordinates.
                if(request->value("Generation",uint64_t{})!=PS::SpawnToolRequests::Generation.load()||world!=m_readyWorld
                    ||request->value("World",std::string{})!=RC::to_string(world->GetPathName())
                    ||request->value("PlayerGuid",std::string{}).empty()||request->value("PlayerGuid",std::string{})!=GetPlayerCharacterGuid(selected))
                    throw std::runtime_error("Party travel world or moving character changed; refresh the party list");
            }
            if(request->contains("TargetPlayer")){
                UObject* destination=nullptr;const auto targetPath=request->at("TargetPlayer").get<std::string>();
                for(auto* candidate:controllers)if(candidate&&candidate->GetWorld()==world&&!candidate->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed))&&RC::to_string(candidate->GetPathName())==targetPath){if(destination)throw std::runtime_error("Party destination is ambiguous");destination=candidate;}
                if(!destination||destination==selected||request->value("TargetPlayerGuid",std::string{}).empty()||request->value("TargetPlayerGuid",std::string{})!=GetPlayerCharacterGuid(destination))
                    throw std::runtime_error("Party destination disconnected or changed character; refresh the party list");
                auto* targetPawn=ActorHelper::GetObjectRef(destination,TEXT("Pawn"));
                if(!targetPawn||!targetPawn->IsA<AActor>()||targetPawn->GetWorld()!=world||targetPawn->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed)))throw std::runtime_error("Party destination has no active character");
                const auto at=ActorHelper::GetActorLocation(static_cast<AActor*>(targetPawn));
                (*request)["TargetLocation"]={{"X",at.X()},{"Y",at.Y()},{"Z",at.Z()}};(*request)["NearMarker"]=true;
            }
            const auto [point,projection]=ToolMapTarget(world,*request);
            PS::SpawnToolRequests::CancelRequested=false;
            std::optional<double> height;const auto& target=request->at("TargetLocation");
            if(target.contains("Z")&&target["Z"].is_number()){
                const auto z=target["Z"].get<double>();if(std::isfinite(z)&&z>=projection.MinZ&&z<=projection.MaxZ)height=z;
            }
            ToolMapTravelBegin(world,selected,pawn,projection,point,request->value("NearMarker",false),height);return;
        }
        if(action=="DismissHelpyNpcs"||(action=="Spawn"&&request->value("NPC",false))) {
            auto* npcs=DragonWildsNpcLoader::HelpyInstance;
            if(!npcs)throw std::runtime_error("Enable the NPC loader before using Helpy NPC authoring.");
            auto result=PS::SpawnToolRequests::Read();result["Players"]=players;
            if(action=="DismissHelpyNpcs") {
                const auto count=npcs->DismissHelpyNpcs(world);
                result["Status"]="Requested cleanup of "+std::to_string(count)+" Helpy temporary NPCs. Failed native destruction remains queued; permanent/source actors are untouched.";
            }else {
                auto* pawn=static_cast<AActor*>(ActorHelper::GetObjectRef(selected,TEXT("Pawn")));
                auto spawned=npcs->SpawnHelpyNpc(*request,world,pawn);
                const bool ok=spawned.value("State",std::string{})=="Completed";
                result["Status"]=spawned.value("Permanent",false)?
                    "NPC definition saved: "+spawned.value("File",std::string{})+(ok?"; placement activated.":"; native activation incomplete. "+spawned.value("Error",std::string{})):
                    "Temporary NPC created for "+std::to_string(spawned.value("DurationSeconds",0))+" seconds. Only its owned actor is retired; source store rows are retained.";
                result["SpawnResult"]=std::move(spawned);
            }
            PS::SpawnToolRequests::Publish(std::move(result));return;
        }
        if(action=="CreateClone") {
            auto* loader=DragonWildsAssetModLoader::AuthoringInstance;
            if(!loader)throw std::runtime_error("The assets loader must be enabled before using Clone Creator");
            auto result=PS::SpawnToolRequests::Read();result["Players"]=players;
            // Runtime-created identities have not been synchronized to remote
            // clients. Do not put unresolved clone IDs in a network inventory.
            if(PS::Network::Detect(world).Mode!=PS::Network::Role::Standalone)
                throw std::runtime_error("Live clone creation is restricted to single-player tests until client identity synchronization is verified");
            const int quantity=request->value("Count",1);
            if(quantity<1 || quantity>10000)throw std::runtime_error("Clone quantity must be 1..10000");
            auto clone=loader->CreateToolClone(*request,world);
            const bool give=request->value("Give",true);
            const auto path=clone.at("Path").get<std::string>();
            clone["GrantState"]="NotRequested";
            if(give&&!clone.value("CompanionsReady",true))clone["GrantState"]="BlockedCompanions";
            else if(give) {
                try {GiveToolItem(selected,path,quantity);clone["GrantState"]="Confirmed";}
                catch(const std::exception& error){clone["GrantState"]="Unconfirmed";clone["GrantMessage"]=error.what();}
                catch(...){clone["GrantState"]="Unconfirmed";clone["GrantMessage"]="Native grant returned an unknown error";}
            }
            clone["State"]=!clone.value("CompanionsReady",true)?"CreatedFilesIncomplete":clone["GrantState"]=="Unconfirmed"?"CreatedGrantUnconfirmed":"Completed";
            result["Status"]=clone.value("Permanent",false)?"Clone created and installed: "+clone.at("File").get<std::string>()
                :"Temporary clone created with "+clone.at("SoftDeleteField").get<std::string>()+" = true. Save/reload safety remains experimental.";
            if(!clone.value("CompanionsReady",true))result["Status"]="Item files are incomplete; NO grant attempted. Repair with game closed. "+clone.value("CompanionError",std::string{})+" Saved: "+clone["Files"].dump()+" Pending: "+clone["PendingFiles"].dump();
            else if(clone["GrantState"]=="Unconfirmed")result["Status"]=result["Status"].get<std::string>()+" Inventory grant unconfirmed; check inventory before retrying. The clone definition already exists.";
            else if(give)result["Status"]=result["Status"].get<std::string>()+" Inventory grant confirmed.";
            if(clone.value("CompanionsReady",true)&&clone.value("CompanionsNeedRestart",false))result["Status"]=result["Status"].get<std::string>()+" Journal/recipe files saved for their existing loaders; restart to confirm placement.";
            result["CloneResult"]=std::move(clone);
            try {auto roster=PS::AssetSearch::ItemRoster();g_quickCatalogIndex.RememberItems(roster);result["Items"]=roster;PS::SpawnToolRequests::CatalogProgress({{"Items",std::move(roster)}});}
            catch(...) {result["Status"]=result["Status"].get<std::string>()+" Refresh Helpy to update item placards.";}
            PS::SpawnToolRequests::Publish(std::move(result));return;
        }
        if(action=="GiveItems") {
            const auto& rows=request->at("Items");
            if(!rows.is_array()||rows.empty()||rows.size()>PS::HelpyGrants::MaxSelection)throw std::runtime_error("Select 1..64 different items for a batch grant");
            std::vector<PS::HelpyGrants::Grant> grants;
            for(const auto& row:rows)grants.push_back({row.at("Item").get<std::string>(),row.at("Count").get<int>()});
            const bool equip=request->value("EquipOnReceipt",false);std::map<std::string,nlohmann::json> equipment;
            const auto outcomes=PS::HelpyGrants::ExecuteBatch(grants,[&](const PS::HelpyGrants::Grant& grant){GiveToolItem(selected,grant.item,grant.count,equip,equip?&equipment[grant.item]:nullptr);});
            auto result=PS::SpawnToolRequests::Read();result["Players"]=players;result["GrantResults"]=nlohmann::json::array();size_t confirmed=0;
            for(const auto& outcome:outcomes) {
                const bool ok=outcome.state==PS::HelpyGrants::BatchRow::State::Confirmed;if(ok)++confirmed;
                result["GrantResults"].push_back({{"Item",outcome.grant.item},{"Count",outcome.grant.count},
                    {"State",ok?"Confirmed":outcome.state==PS::HelpyGrants::BatchRow::State::Rejected?"Rejected":outcome.state==PS::HelpyGrants::BatchRow::State::Unconfirmed?"Unconfirmed":"NotAttempted"},{"Message",outcome.message}});
                if(equip)result["GrantResults"].back()["Equip"]=ok&&equipment.contains(outcome.grant.item)?equipment.at(outcome.grant.item):nlohmann::json{{"State","NotAttempted"},{"Message","Delivery was not confirmed; no equip attempted."}};
                if(outcome.state==PS::HelpyGrants::BatchRow::State::Unconfirmed)PS::Log<LogLevel::Warning>(STR("[HELPY][CART][UNCONFIRMED] request={} item={} reason={}\n"),requestId,RC::to_generic_string(outcome.grant.item),RC::to_generic_string(outcome.message));
            }
            result["Status"]=confirmed==grants.size()?std::format("Granted {} selected item types; every inventory delta confirmed.",confirmed):
                std::format("{} / {} item types confirmed. Rejected items were skipped; inspect inventory before retrying any unconfirmed item.",confirmed,grants.size());
            PS::SpawnToolRequests::Publish(std::move(result));return;
        }
        if(action=="GiveItem") {
            GiveToolItem(selected,request->at("Item").get<std::string>(),request->at("Count").get<int32_t>());
            auto result=PS::SpawnToolRequests::Read();result["Players"]=players;
            result["Status"]=std::format("Granted {} item(s) to the selected authoritative player and confirmed the inventory delta.",request->at("Count").get<int32_t>());
            PS::SpawnToolRequests::Publish(std::move(result));return;
        }
        if(action=="ImportBuildings") {
            if(world!=m_readyWorld||request->value("Generation",uint64_t{})!=PS::SpawnToolRequests::CurrentGeneration())throw std::runtime_error("World changed; select the prefab position again");
            if(!PlanBuildingImport)throw std::runtime_error("RSDW Builder importer is unavailable");
            auto document=request->at("Document");if(document.dump().size()>1024*1024)throw std::runtime_error("Prefab export exceeds 1 MiB");
            auto* pawn=static_cast<AActor*>(ActorHelper::GetObjectRef(selected,TEXT("Pawn")));const auto ground=ToolMapGround(world,*request,pawn);
            const double yaw=request->value("Yaw",0.0);if(!std::isfinite(yaw))throw std::runtime_error("Prefab rotation must be finite");
            document["RuneSchemaPlacement"]={{"Location",{{"X",ground.X()},{"Y",ground.Y()},{"Z",ground.Z()}}},{"Rotation",{{"Yaw",yaw}}},{"OriginMode","BoundsCenter"},{"ImportMode","NativeBuildingPieces"},{"AllowDeconstruction",true},{"IncludeGhosted",false}};
            auto plan=PlanBuildingImport(document);const auto stem=PS::Authoring::NewName();
            double lowest=std::numeric_limits<double>::max();for(const auto& piece:plan)lowest=std::min(lowest,piece.at("Location").at("Z").get<double>());
            if(!std::isfinite(lowest))throw std::runtime_error("Prefab height is invalid");
            const auto checkpoint=m_spawns.size(),reportedCheckpoint=m_reportedNewSpawns;if(checkpoint+plan.size()>65536)throw std::runtime_error("Spawn definition limit reached");
            const auto projection=PS::HelpyMapRuntime::Projection(world);std::unique_ptr<PS::Authoring::StagedFile> file;
            try{
                for(auto& piece:plan){
                    piece["Id"]=stem+"-"+piece.at("Id").get<std::string>();
                    // Center in XY, but put the lowest piece on ground rather than burying half the build.
                    piece["Location"]["Z"]=piece.at("Location").at("Z").get<double>()+ground.Z()-lowest;const auto& position=piece.at("Location");
                    if(!projection.Contains({position.at("X").get<double>(),position.at("Y").get<double>()})||!position.at("Z").is_number()||!std::isfinite(position.at("Z").get<double>())||position.at("Z").get<double>()<projection.MinZ||position.at("Z").get<double>()>projection.MaxZ)throw std::runtime_error("Prefab extends outside the native world bounds; nothing saved");
                    RegisterSpawn(piece,TEXT("runeschema"));const auto& spawn=m_spawns.back();auto* building=ActorHelper::ResolveObject(RC::to_generic_string(spawn.BuildingDataPath));
                    const auto [safe,reason]=VerifyToolBuildingCandidate(building,spawn,world,false);if(!safe)throw std::runtime_error("Prefab piece "+spawn.BuildingDataPath+": "+reason);
                }
                file=std::make_unique<PS::Authoring::StagedFile>("spawns",stem,plan);{std::scoped_lock lock{m_toolSpawnFileMutex};m_toolSpawnFiles.insert(file->Path().lexically_normal());}file->Commit();
            }catch(...){if(file){std::scoped_lock lock{m_toolSpawnFileMutex};m_toolSpawnFiles.erase(file->Path().lexically_normal());}m_spawns.resize(checkpoint);m_reportedNewSpawns=reportedCheckpoint;throw;}
            size_t active=0;std::string activationError;
            for(size_t index=checkpoint;index<m_spawns.size();++index)try{
                auto& spawn=m_spawns[index];ProcessActorEntry(world,spawn);auto* live=spawn.LiveActor.Get();auto* building=ActorHelper::ResolveObject(RC::to_generic_string(spawn.BuildingDataPath));
                if(spawn.bExistsInWorld&&live&&live->IsA<AActor>()){
                    const auto [safe,reason]=VerifyToolBuildingInstance(static_cast<AActor*>(live),building,spawn,world,false);
                    if(safe)++active;else if(activationError.empty())activationError=reason;
                }
            }catch(const std::exception& error){if(activationError.empty())activationError=error.what();}
            auto result=PS::SpawnToolRequests::Read();const bool complete=active==plan.size();
            result["Status"]=std::format("Prefab saved: {} of {} pieces active. {}",active,plan.size(),complete?"Placement confirmed.":"Some pieces are pending or failed; do not place a duplicate.");
            result["SpawnResult"]={{"Permanent",true},{"File",file->Path().string()},{"Active",complete},{"Created",active},{"Requested",plan.size()},{"State",complete?"Completed":"SavedActivationIncomplete"},{"Location",{ground.X(),ground.Y(),ground.Z()}}};
            if(!activationError.empty())result["SpawnResult"]["Error"]=activationError;
            PS::Log<LogLevel::Normal>(STR("[HELPY][BUILDING][IMPORT] request={} active={} requested={} file={}\n"),requestId,active,plan.size(),file->Path().wstring());
            PS::SpawnToolRequests::Publish(std::move(result));return;
        }
        if(action=="SpawnBuilding") {
            // Building previews are intentionally single-shot.  Unlike ordinary
            // actors, a bad construction script can take the process down before
            // C++ exception handling gets a chance to recover, so do not multiply
            // that risk with grids/batches.
            if(request->contains("Grid"))throw std::runtime_error("Building preview grids are disabled by the safety gate; spawn one verified piece at a time");
            const bool mapPlacement=request->contains("TargetLocation"),permanent=request->value("Permanent",false);
            const int count=request->value("Count",1);const double distance=request->value("Distance",5.0);
            if(count!=1 || (!mapPlacement&&(!std::isfinite(distance) || distance<3 || distance>50)))
                throw std::runtime_error("Building previews require Count=1 and distance 3..50 metres");
            const auto buildingPath=request->at("Building").get<std::string>();
            const double yaw=request->value("Yaw",0.0),height=request->value("Height",0.0),scale=request->value("Scale",1.0);
            if(!std::isfinite(yaw) || !std::isfinite(height) || std::abs(height)>1000 || !std::isfinite(scale) || scale<0.01 || scale>100.0)
                throw std::runtime_error("Building scale/yaw/height is invalid");
            auto authored=nlohmann::json{{"Id","candidate"},{"Type","BuildingProp"},{"Building",buildingPath},
                {"Scale",scale},{"AllowDeconstruction",request->value("AllowDeconstruction",false)},{"UseNativeRespawn",false},
                {"Location",nlohmann::json::array({0,0,0})},{"Rotation",{{"Pitch",0},{"Yaw",yaw},{"Roll",0}}}};
            const auto time=request->value("TimeOfDay",std::string("Any"));
            if(time!="Any")authored["TimeOfDay"]=time;
            SpawnInfo prototype;prototype.ModName=TEXT("__RuneSchemaTools");prototype.EntryId=TEXT("candidate");
            RegisterBuildingProp(prototype,authored);
            auto* buildingObject=ActorHelper::ResolveObject(RC::to_generic_string(buildingPath));
            const auto [buildingSafe,buildingReason]=VerifyToolBuildingCandidate(buildingObject,prototype,world,!permanent);
            if(!buildingSafe)throw std::runtime_error("Building placement unavailable: "+buildingReason);
            auto* buildingClass=ResolveClass(prototype.ClassPath);
            if(!buildingClass)throw std::runtime_error("Building actor class unloaded during validation");
            auto* pawn=static_cast<AActor*>(ActorHelper::GetObjectRef(selected,TEXT("Pawn")));
            const auto origin=ActorHelper::GetActorLocation(pawn);
            std::vector<FVector> positions;std::vector<AActor*> created;
            if(m_toolActors.size()+count>100)throw std::runtime_error("100 tool actors reached; clean them up first");
            if(m_toolPlacements.size()+count>1000)throw std::runtime_error("Export or clear the current 1000 authored placements first");
            const double x=origin.X()+distance*100;
            const double y=origin.Y();
            FVector ground{};std::string error;
            if(mapPlacement)ground=ToolMapGround(world,*request,pawn);
            else if(!UECustom::UKismetSystemLibrary::FindGroundSurface(world,FVector(x,y,origin.Z()+500),FVector(x,y,origin.Z()-1500),{pawn},ground,error,origin.Z()))
                throw std::runtime_error("No ground at the requested building position; nothing spawned. "+error);
            positions.emplace_back(ground.X(),ground.Y(),ground.Z()+height);
            if(permanent){
                const auto stem=PS::Authoring::NewName();const auto& position=positions.front();
                auto placement=PS::SpawnAuthoring::Placement(authored,stem,{position.X(),position.Y(),position.Z()},yaw);
                auto document=PS::SpawnAuthoring::ResolveGroundDrafts(nlohmann::json::array({PS::SpawnAuthoring::StageGroundDraft(std::move(placement),position.Z(),height)}));
                const auto checkpoint=m_spawns.size(),reportedCheckpoint=m_reportedNewSpawns;if(checkpoint>=65536)throw std::runtime_error("Spawn definition limit reached");
                std::unique_ptr<PS::Authoring::StagedFile> file;
                try{RegisterSpawn(document.front(),TEXT("runeschema"));file=std::make_unique<PS::Authoring::StagedFile>("spawns",stem,document);{std::scoped_lock lock{m_toolSpawnFileMutex};m_toolSpawnFiles.insert(file->Path().lexically_normal());}file->Commit();}
                catch(...){if(file){std::scoped_lock lock{m_toolSpawnFileMutex};m_toolSpawnFiles.erase(file->Path().lexically_normal());}m_spawns.resize(checkpoint);m_reportedNewSpawns=reportedCheckpoint;throw;}
                auto result=PS::SpawnToolRequests::Read();
                try{ProcessActorEntry(world,m_spawns[checkpoint]);result["Status"]=m_spawns[checkpoint].bExistsInWorld?"Building placement saved and activated.":"Building placement saved; activation is pending. Do not create a duplicate.";}
                catch(const std::exception& error){result["Status"]=std::string("Building placement saved; activation failed. Do not create a duplicate. ")+error.what();}
                auto* live=m_spawns[checkpoint].LiveActor.Get();
                const auto verification=m_spawns[checkpoint].bExistsInWorld&&live&&live->IsA<AActor>()
                    ?VerifyToolBuildingInstance(static_cast<AActor*>(live),buildingObject,m_spawns[checkpoint],world,false)
                    :std::pair<bool,std::string>{false,"No matching live building actor was available"};
                const bool active=verification.first;
                result["SpawnResult"]={{"Permanent",true},{"File",file->Path().string()},{"Active",active},{"Building",buildingPath},{"Location",{position.X(),position.Y(),position.Z()}},{"State",active?"Completed":"SavedActivationIncomplete"}};
                result["SpawnResult"]["RequestedLocation"]={position.X(),position.Y(),position.Z()};
                if(live&&live->IsA<AActor>()){const auto actual=ActorHelper::GetActorLocation(static_cast<AActor*>(live));result["SpawnResult"]["ObservedLocation"]={actual.X(),actual.Y(),actual.Z()};if(active)result["SpawnResult"]["Location"]=result["SpawnResult"]["ObservedLocation"];}
                else {
                    result["SpawnResult"]["ObservedLocation"]=nullptr;
                }
                if(!active){
                    result["SpawnResult"]["Error"]=verification.second;
                    result["SpawnResult"]["ActivationStatus"]=result.value("Status",std::string{});
                    result["Status"]="Building placement saved; native placement verification failed. Do not place a duplicate.";
                }
                else result["Status"]="Building placement saved and verified at the requested position.";
                PS::Log<LogLevel::Normal>(STR("[HELPY][BUILDING][RESULT] request={} active={} expected=[{},{},{}] file='{}' detail='{}'\n"),requestId,active,position.X(),position.Y(),position.Z(),file->Path().wstring(),PS::ToWideSafe(verification.second.c_str()));
                PS::SpawnToolRequests::Publish(std::move(result));return;
            }
            try {
                for(const auto& position:positions) {
                    prototype.Location=position;
                    auto* actor=ActorHelper::SpawnActor(world,buildingClass,position,FRotator(0,yaw,0),[&](AActor* value){
                        value->SetFlags(RF_Transient);
                        auto* skip=CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(value->GetClassPrivate(),TEXT("bSkipSpudStore")));
                        if(!skip || skip->GetArrayDim()!=1)throw std::runtime_error("Building preview lost its bSkipSpudStore safety contract before construction");
                        skip->SetPropertyValue(skip->ContainerPtrToValuePtr<void>(value),true);
                        ApplyBuildingData(value,prototype);
                    },ESpawnActorScaleMethod::OverrideRootScale,ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding,FVector(scale,scale,scale));
                    const auto [retainedSafe,retainedReason]=VerifyToolBuildingInstance(actor,buildingObject,prototype,world,true);
                    if(!retainedSafe) {ActorHelper::DestroyActor(actor);throw std::runtime_error("Building preview failed post-construction safety verification: "+retainedReason);}
                    actor->SetActorScale3D(FVector(scale,scale,scale));created.push_back(actor);
                }
            }catch(...){for(auto* actor:created)ActorHelper::DestroyActor(actor);throw;}
            const auto stamp=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            for(size_t i=0;i<created.size();++i) {
                m_toolActors.emplace_back(PS::WeakObject(created[i]));const auto& p=positions[i];
                auto placement=PS::SpawnAuthoring::Placement(authored,"authored_"+std::to_string(stamp)+"_"+std::to_string(i),{p.X(),p.Y(),p.Z()},yaw);
                placement=PS::SpawnAuthoring::StageGroundDraft(std::move(placement),p.Z(),height);
                m_toolPlacements.push_back(std::move(placement));
            }
            auto result=PS::SpawnToolRequests::Read();
            result["Players"]=players;
            result["Status"]=std::format("Spawned {} verified temporary building preview; {} placements ready to export.",created.size(),m_toolPlacements.size());
            const auto actual=ActorHelper::GetActorLocation(created.front());
            result["SpawnResult"]={{"State","Completed"},{"Permanent",false},{"Active",true},{"Building",buildingPath},{"Created",created.size()},{"Location",{actual.X(),actual.Y(),actual.Z()}}};
            PS::SpawnToolRequests::Publish(std::move(result));return;
        }
        if(action=="Cleanup") {
            size_t removed=0;
            for(auto it=m_toolActors.begin();it!=m_toolActors.end();) {
                auto* object=it->Get();
                if(!object){it=m_toolActors.erase(it);continue;}
                if(object->GetWorld()!=world){++it;continue;}
                if(!object->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed)))ActorHelper::DestroyActor(static_cast<AActor*>(object));
                it=m_toolActors.erase(it);++removed;
            }
            auto result=PS::SpawnToolRequests::Read();
            result["Players"]=players;
            result["Status"]=std::format("Cleaned {} tool-created actors in this world.",removed);
            PS::SpawnToolRequests::Publish(std::move(result));return;
        }
        if(action!="Spawn")throw std::runtime_error("Unknown spawn-tool action");
        spawnStage="request validation";
        const int count=request->at("Count").get<int>();const double distance=request->value("Distance",5.0);
        const auto placementMode=request->value("PlacementMode",std::string("Legacy"));
        if(count<1 || count>(request->contains("Grid")?100:20))throw std::runtime_error("Count must be 1..20 (grid: 100 maximum)");
        if(placementMode=="Legacy"&&(!std::isfinite(distance)||distance<3||distance>50))throw std::runtime_error("Distance must be 3..50 metres");
        if(placementMode!="Legacy"&&placementMode!="Radius"&&placementMode!="Grid")throw std::runtime_error("PlacementMode must be Radius or Grid");
        std::erase_if(m_toolActors,[](const auto& ref){auto* value=ref.Get();return !value || value->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed|RF_FinishDestroyed));});
        if(!request->value("Permanent",false) && m_toolActors.size()+count>100)throw std::runtime_error("100 tool actors reached; clean them up first");
        spawnStage="definition resolution";
        Events::SpawnTemplate definition;const SpawnInfo* ordinary=nullptr;
        bool resource=request->value("Resource",false);
        const auto key=request->value("Definition",std::string{});
        if(!key.empty()) {
            if(auto found=m_eventTemplates.find(key);found!=m_eventTemplates.end()){definition=found->second;resource=false;}
            else if(key.starts_with("@loaded-ai:")) {
                definition.Class=key.substr(std::string("@loaded-ai:").size());
                definition.Name=definition.Class.substr(definition.Class.find_last_of("./")+1);resource=false;
            }
            else if(key.starts_with("@loaded-resource:") || key.starts_with("@catalog-resource:")) {
                const auto prefix=key.starts_with("@loaded-resource:")?std::string("@loaded-resource:"):std::string("@catalog-resource:");
                definition.Class=key.substr(prefix.size());
                definition.Name=definition.Class.substr(definition.Class.find_last_of("./")+1);resource=true;
            }
            else {
                for(const auto& spawn:m_spawns)if(spawn.Type!=ESpawnEntryType::RemoveActor && RC::to_string(spawn.ModName)+":"+RC::to_string(spawn.EntryId)==key){if(ordinary)throw std::runtime_error("Ambiguous spawn definition");ordinary=&spawn;}
                if(!ordinary)throw std::runtime_error("Loaded spawn definition no longer exists");
                resource=ordinary->Type==ESpawnEntryType::Actor;
                definition.Class=RC::to_string(resource?ordinary->ClassPath:ordinary->AIClassPath);definition.Name=ordinary->DisplayName;definition.BossName=ordinary->BossName;
                definition.Scale=ordinary->Scale.X();definition.LootRow=ordinary->LootRow;
                if(ordinary->Scale.Y()!=definition.Scale || ordinary->Scale.Z()!=definition.Scale)throw std::runtime_error("Tool AI copies currently require uniform scale");
                if(!ordinary->VisualEffect.empty()&&!request->contains("GhostMesh")) {
                    definition.VisualEffect=ordinary->VisualEffect;definition.VisualEffect["Type"]="Ghost Glow";
                    if(ordinary->VisualEffect.value("Type",std::string{})!="Ghost")throw std::runtime_error("This spawn visual is not supported by the copy tool");
                }
            }
        } else {
            definition=Events::ParseSpawn("__RuneSchemaTools",{{"Id","candidate"},{"Type","AISpawnPoint"},{"EventOnly",true},
                {"AIClass",request->at("Class")},{"Scale",request->at("Scale")}});
            const auto name=request->value("Name",std::string{});
            if(!name.empty())definition.Name=Dialogue::Text(name,256);
            if(request->value("Boss",false))definition.BossName=definition.Name;
        }
        if(f2Spawn&&request->value("Class",std::string{})!=definition.Class)
            throw std::runtime_error("The selected definition no longer maps to that class; refresh and select it again");
        // A selected roster entry is still an authored instance. Let its
        // readable instance name differ from the source class/template name.
        const auto requestedName=request->value("Name",std::string{});
        if(!requestedName.empty()) {
            definition.Name=Dialogue::Text(requestedName,256);
            if(!resource && request->value("Boss",false))definition.BossName=definition.Name;
        }
        const double requestedScale=request->value("Scale",definition.Scale);
        if(!std::isfinite(requestedScale) || requestedScale<0.01 || requestedScale>100.0)
            throw std::runtime_error("AI/resource instance scale must be finite and between 0.01 and 100");
        definition.Scale=requestedScale;
        const double yaw=request->value("Yaw",0.0),height=request->value("Height",0.0);
        if(!std::isfinite(yaw) || !std::isfinite(height) || std::abs(height)>1000)throw std::runtime_error("Yaw/height must be finite; height offset limited to +/-1000 cm");
        auto authored=ordinary?ordinary->AuthoredDefinition:nlohmann::json{{"Type",resource?"Actor":"AISpawnPoint"},{resource?"Class":"AIClass",definition.Class},{"Scale",definition.Scale}};
        authored["Scale"]=definition.Scale;
        const bool permanent=request->value("Permanent",false);
        int powerLevel=-1;
        if(request->contains("PowerLevel")) {
            if(resource)throw std::runtime_error("PowerLevel is only supported for AI/enemy spawns");
            if(!request->at("PowerLevel").is_number_integer())throw std::runtime_error("PowerLevel must be an integer");
            powerLevel=request->at("PowerLevel").get<int>();PS::Authoring::ValidatePower(powerLevel);
            if(powerLevel==-1)throw std::runtime_error("Omit PowerLevel to inherit the native value");
            authored["PowerLevel"]=powerLevel;
        }
        if(request->contains("GhostMesh")) {
            definition.VisualEffect=request->at("GhostMesh").get<bool>()?nlohmann::json{{"Type","Ghost Glow"},{"Overlay",true},{"BodyMaterial",false}}:nlohmann::json::object();
            authored.erase("VisualEffect");
            if(!definition.VisualEffect.empty()){authored["VisualEffect"]=definition.VisualEffect;authored["VisualEffect"]["Type"]="Ghost";}
        }
        spawnStage="additional loot validation";
        auto additional=ordinary?ordinary->AdditionalDrops:nlohmann::json::array();
        if(request->value("AppendAdditionalDrops",false)) {
            const auto extra=request->value("AdditionalDrops",nlohmann::json::array());SpawnFields::Drops(extra);
            additional.insert(additional.end(),extra.begin(),extra.end());
        } else additional=request->value("AdditionalDrops",additional);
        SpawnFields::Drops(additional);
        if(!additional.empty())authored["AdditionalDrops"]=additional;else authored.erase("AdditionalDrops");
        if(!definition.Name.empty())authored["DisplayName"]=definition.Name;
        if(!resource && !definition.BossName.empty())authored["BossName"]=definition.BossName;
        if(!resource && !definition.LootRow.empty())authored["LootRow"]=definition.LootRow;
        if(!ordinary && !definition.VisualEffect.empty())authored["VisualEffect"]=definition.VisualEffect;
        if(resource && !definition.BossName.empty())throw std::runtime_error("Resource tool spawns do not support boss titles");
        if(!permanent && m_toolPlacements.size()+count>1000)throw std::runtime_error("Export or clear the current 1000 authored placements first");
        definition.Key="__RuneSchemaTools:temporary_"+std::to_string(++m_toolSequence);
        auto check=Events::Identity(definition);check["kind"]=resource?"tool-resource":"tool-ai";(void)Events::ToolIdentity(check);
        if(!m_eventTemplates.emplace(definition.Key,definition).second)throw std::runtime_error("Temporary spawn identity is already in use");
        struct Erase {std::map<std::string,Events::SpawnTemplate>& Values;std::string Key;~Erase(){Values.erase(Key);}} erase{m_eventTemplates,definition.Key};
        spawnStage="native class and save-exclusion validation";
        UClass* resourceClass=nullptr;
        if(resource && !permanent) {
            resourceClass=ResolveClass(RC::to_generic_string(definition.Class));
            if(!resourceClass || !ActorHelper::IsActorClass(resourceClass) || ActorHelper::IsAbstract(resourceClass))throw std::runtime_error("Resource requires a concrete actor class");
            auto* guid=PropertyHelper::GetPropertyByName(resourceClass,TEXT("SpudGuid"));
            auto* skip=CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(resourceClass,TEXT("bSkipSpudStore")));
            if(!guid || guid->GetSize()!=sizeof(FGuid) || !skip || skip->GetArrayDim()!=1)throw std::runtime_error("This resource lacks verified temporary save exclusion; not spawned");
        }else if(!permanent)ValidateEventSpawn(definition.Key);
        auto* pawn=static_cast<AActor*>(ActorHelper::GetObjectRef(selected,TEXT("Pawn")));
        auto origin=ActorHelper::GetActorLocation(pawn);
        std::optional<PS::MapProjection::Projection> mapProjection;
        if(request->contains("TargetLocation")){
            const auto& point=request->at("TargetLocation");
            if(!point.is_object()||!point.contains("X")||!point.contains("Y")||!point["X"].is_number()||!point["Y"].is_number())throw std::runtime_error("Map placement requires finite world X and Y");
            const PS::MapProjection::Point xy{point["X"].get<double>(),point["Y"].get<double>()};
            mapProjection=PS::HelpyMapRuntime::Projection(world);
            if(!mapProjection->Contains(xy))throw std::runtime_error("Selected position is outside the native world map");
            origin=FVector(xy.X,xy.Y,origin.Z());
        }
        nlohmann::json gridPoints=nlohmann::json::array();
        if(request->contains("Grid")) {
            gridPoints=SpawnFields::Expand({{"Type",resource?"Actor":"AISpawnPoint"},{"Grid",request->at("Grid")},
                {"Location",{{"X",origin.X()+distance*100},{"Y",origin.Y()},{"Z",0}}},
                {"Rotation",{{"Pitch",0},{"Yaw",yaw},{"Roll",0}}}});
            if(gridPoints.size()!=count)throw std::runtime_error("Grid count does not match request");
        }
        double placementRadius=distance*100,gridX=0,gridY=0,gridSize=100;
        if(placementMode=="Radius") {
            placementRadius=request->value("PlacementRadius",500.0);
            if(!std::isfinite(placementRadius)||placementRadius<0||placementRadius>10000)throw std::runtime_error("Spawn radius must be 0..10000 centimetres");
        }else if(placementMode=="Grid") {
            gridX=request->value("GridX",0.0);gridY=request->value("GridY",0.0);gridSize=request->value("GridSize",100.0);
            if(!std::isfinite(gridX)||!std::isfinite(gridY)||std::abs(gridX)>100||std::abs(gridY)>100||!std::isfinite(gridSize)||gridSize<10||gridSize>1000)
                throw std::runtime_error("Grid placement uses X/Y -100..100 cells and a 10..1000 cm cell size");
        }
        spawnStage="ground placement";
        std::vector<FVector> positions;
        for(int i=0;i<count;++i) {
            const double angle=6.283185307179586*(static_cast<double>(i)/count+(placementMode=="Legacy"?0.0:static_cast<double>(requestId%360)/360.0));
            double x=origin.X()+std::cos(angle)*placementRadius,y=origin.Y()+std::sin(angle)*placementRadius;
            if(!gridPoints.empty()){x=gridPoints[i]["Location"]["X"].get<double>();y=gridPoints[i]["Location"]["Y"].get<double>();}
            else if(placementMode=="Grid") {const int columns=static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count))));x=std::round(origin.X()/gridSize)*gridSize+(gridX+i%columns)*gridSize;y=std::round(origin.Y()/gridSize)*gridSize+(gridY+i/columns)*gridSize;}
            FVector ground{};std::string error;
            if(mapProjection&&!mapProjection->Contains({x,y}))throw std::runtime_error("The selected spawn spread extends outside the world map");
            if(!UECustom::UKismetSystemLibrary::FindGroundSurface(world,FVector(x,y,mapProjection?mapProjection->MaxZ:origin.Z()+500),FVector(x,y,mapProjection?mapProjection->MinZ:origin.Z()-1500),{pawn},ground,error,mapProjection?std::optional<double>{}:std::optional<double>{origin.Z()}))
                throw std::runtime_error("No ground at a requested spawn position; nothing spawned. "+error);
            positions.emplace_back(ground.X(),ground.Y(),ground.Z()+(resource?0:100)+height);
        }
        if(permanent) {
            spawnStage="permanent placement validation";
            // One entry per actual position: reload uses the exact original
            // placements, not the actor's later wander position or player position.
            const auto stem=PS::Authoring::NewName();
            auto document=nlohmann::json::array();
            auto installed=authored;installed.erase("EventOnly");installed.erase("Grid");
            if(!resource && !installed.contains("Respawn"))installed["Respawn"]=false;
            for(size_t i=0;i<positions.size();++i) {
                const auto& p=positions[i];
                auto placement=PS::SpawnAuthoring::Placement(installed,stem+"_"+std::to_string(i),{p.X(),p.Y(),p.Z()},yaw);
                document.push_back(PS::SpawnAuthoring::StageGroundDraft(std::move(placement),p.Z(),(resource?0:100)+height));
            }
            document=PS::SpawnAuthoring::ResolveGroundDrafts(document);
            const auto checkpoint=m_spawns.size();
            const auto reportedCheckpoint=m_reportedNewSpawns;
            if(checkpoint+document.size()>65536)throw std::runtime_error("Spawn definition limit reached");
            std::unique_ptr<PS::Authoring::StagedFile> file;
            try {
                // RegisterSpawn throws on invalid native class/property contracts;
                // no actor is created until the entire document is valid and saved.
                for(const auto& entry:document)RegisterSpawn(entry,TEXT("runeschema"));
                file=std::make_unique<PS::Authoring::StagedFile>("spawns",stem,document);
                {std::scoped_lock lock{m_toolSpawnFileMutex};m_toolSpawnFiles.insert(file->Path().lexically_normal());}
                file->Commit();
            }catch(...) {
                if(file){std::scoped_lock lock{m_toolSpawnFileMutex};m_toolSpawnFiles.erase(file->Path().lexically_normal());}
                m_spawns.resize(checkpoint);m_reportedNewSpawns=reportedCheckpoint;throw;
            }
            spawnStage="permanent placement activation";
            size_t activated=0;auto errors=nlohmann::json::array();
            for(size_t i=checkpoint;i<m_spawns.size();++i) {
                auto& entry=m_spawns[i];
                try {
                    if(resource)ProcessActorEntry(world,entry);else ProcessAISpawnPointEntry(world,entry);
                    if(entry.bExistsInWorld)++activated;
                }catch(const std::exception& error) {entry.bSpawnFailed=true;errors.push_back(error.what());}
                catch(...) {entry.bSpawnFailed=true;errors.push_back("Unknown native activation error");}
            }
            auto result=PS::SpawnToolRequests::Read();result["Players"]=players;
            result["Status"]="Saved "+std::to_string(document.size())+" permanent placements; "+std::to_string(activated)
                +" native placements activated. "+file->Path().string();
            if(activated!=document.size())result["Status"]=result["Status"].get<std::string>()+" Some activation failed; the JSON remains installed. Inspect the log before retrying.";
            // AI activation confirms its native spawn point, not a fabricated
            // promise that an asynchronously-spawned enemy already exists.
            result["SpawnResult"]={{"State",activated==document.size()?"Completed":"SavedActivationIncomplete"},
                {"Permanent",true},{"Saved",document.size()},{"Activated",activated},{"Requested",count},
                {"File",file->Path().string()},{"Errors",std::move(errors)}};
            PS::SpawnToolRequests::Publish(std::move(result));return;
        }
        spawnStage="native actor creation";
        std::vector<AActor*> created;
        try {
            for(const auto& position:positions) {
                AActor* actor=nullptr;
                if(resource) {
                    actor=ActorHelper::SpawnActor(world,resourceClass,position,FRotator(0,yaw,0),[&](AActor* value){
                        value->SetFlags(RF_Transient);
                        auto* skip=CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(resourceClass,TEXT("bSkipSpudStore")));
                        skip->SetPropertyValue(skip->ContainerPtrToValuePtr<void>(value),true);
                        if(ordinary)ApplyEntryProperties(value,ordinary->Properties);
                        skip->SetPropertyValue(skip->ContainerPtrToValuePtr<void>(value),true);
                    }, ESpawnActorScaleMethod::OverrideRootScale);
                    created.push_back(actor);actor->SetActorScale3D(FVector(definition.Scale,definition.Scale,definition.Scale));
                    ApplyActorDisplayName(actor,definition.Name);
                    if(!definition.VisualEffect.empty()){auto visual=definition.VisualEffect;visual["Type"]="Ghost";ApplyVisualEffect(actor,visual,TEXT("Tool resource"));}
                    if(PS::Network::Detect(actor).Mode!=PS::Network::Role::Standalone) {
                        if(!PublishEventIdentity)throw std::runtime_error("Resource identity transport unavailable");
                        auto payload=Events::Identity(definition);payload["kind"]="tool-resource";PublishEventIdentity(actor,payload.dump());
                    }
                }else {actor=SpawnEventAI(definition.Key,world,position,true,yaw,{},powerLevel);created.push_back(actor);}
                if(ordinary) {
                    ApplyAIProperties(actor,ordinary->CharacterProperties,ordinary->ComponentProperties);
                    ApplyCombatMultipliers(actor,ordinary->HealthMultiplier,ordinary->DamageMultiplier);
                    ApplyDropMultiplier(actor,ordinary->DropMultiplier);
                }
                ApplyAdditionalDrops(actor,additional);
                if(powerLevel!=-1) {
                    auto* power=CastField<FNumericProperty>(PropertyHelper::GetPropertyByName(actor->GetClassPrivate(),TEXT("PowerLevel")));
                    if(!power || power->GetArrayDim()!=1)throw std::runtime_error("PowerLevel contract changed during spawn overrides");
                    auto* address=power->ContainerPtrToValuePtr<void>(actor);
                    const auto actual=power->IsInteger()?static_cast<double>(power->GetSignedIntPropertyValue(address)):power->GetFloatingPointPropertyValue(address);
                    if(actual!=powerLevel)throw std::runtime_error("Inherited overrides changed the requested PowerLevel; temporary spawn rolled back");
                }
                auto* skip=CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(actor->GetClassPrivate(),TEXT("bSkipSpudStore")));
                if(!skip || skip->GetArrayDim()!=1)throw std::runtime_error("Tool actor save-exclusion contract changed");
                skip->SetPropertyValue(skip->ContainerPtrToValuePtr<void>(actor),true);
                if(!skip->GetPropertyValue(skip->ContainerPtrToValuePtr<void>(actor)))throw std::runtime_error("Tool actor save exclusion was overridden");
            }
        }catch(...){for(auto* actor:created)ActorHelper::DestroyActor(actor);throw;}
        const auto stamp=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        for(size_t i=0;i<created.size();++i) {
            m_toolActors.emplace_back(PS::WeakObject(created[i]));const auto& p=positions[i];
            auto placement=PS::SpawnAuthoring::Placement(authored,"authored_"+std::to_string(stamp)+"_"+std::to_string(i),{p.X(),p.Y(),p.Z()},yaw);
            placement=PS::SpawnAuthoring::StageGroundDraft(std::move(placement),p.Z(),(resource?0:100)+height);
            m_toolPlacements.push_back(std::move(placement));
        }
        auto result=PS::SpawnToolRequests::Read();
        result["Players"]=players;
        result["Status"]=std::format("Spawned {} functioning temporary actors; {} original placements ready to export. Normal applicable quest credit is enabled.",created.size(),m_toolPlacements.size());
        result["SpawnResult"]={{"State","Completed"},{"Created",created.size()},{"Requested",count}};
        if(f2Spawn)PS::Log<LogLevel::Normal>(STR("RuneSchema Helpy spawn #{} completed: {} actor(s).\n"),requestId,created.size());
        PS::SpawnToolRequests::Publish(std::move(result));
    }catch(const std::exception& e){
        auto result=PS::SpawnToolRequests::Read();result["Status"]=e.what();
        if(request->value("Action",std::string{})=="SpawnBuilding"||request->value("Action",std::string{})=="ImportBuildings"){
            result["SpawnResult"]={{"State","Stopped"},{"Active",false},{"Error",e.what()}};
            PS::Log<LogLevel::Warning>(STR("[HELPY][BUILDING][FAILED] request={} reason={}\n"),requestId,RC::to_generic_string(e.what()));
        }
        if(request->value("Action",std::string{})=="CreateClone")result["CloneResult"]={{"State","Stopped"}};
        if(f2Spawn){
            result["Status"]=std::string("Spawn stopped [")+spawnStage+"]: "+e.what();
            result["SpawnResult"]={{"State","Stopped"},{"Stage",spawnStage}};
            PS::Log<LogLevel::Warning>(STR("RuneSchema Helpy spawn #{} stopped [{}]: {}\n"),requestId,RC::to_generic_string(spawnStage),RC::to_generic_string(e.what()));
        }
        PS::SpawnToolRequests::Publish(std::move(result));
    }catch(...){
        auto result=PS::SpawnToolRequests::Read();result["Status"]="The runtime action stopped after an unknown error; inspect the world or inventory before retrying.";
        if(request->value("Action",std::string{})=="SpawnBuilding"||request->value("Action",std::string{})=="ImportBuildings")result["SpawnResult"]={{"State","Stopped"},{"Active",false},{"Error",result["Status"]}};
        if(request->value("Action",std::string{})=="CreateClone")result["CloneResult"]={{"State","Stopped"}};
        if(f2Spawn){result["SpawnResult"]={{"State","Stopped"},{"Stage",spawnStage}};PS::Log<LogLevel::Warning>(STR("RuneSchema Helpy spawn #{} stopped [{}]: unknown error.\n"),requestId,RC::to_generic_string(spawnStage));}
        PS::SpawnToolRequests::Publish(std::move(result));
    }
}
