#include "QuickMenuCatalog.inl"
namespace {
    nlohmann::json ToolCatalog(const std::filesystem::path& file,const char* kind) {
        if(!std::filesystem::is_regular_file(file))throw std::runtime_error(std::string(kind)+" catalog is missing; explicitly build the catalogs first");
        auto value=nlohmann::json::parse(PS::ConfigFiles::Read(file,16*1024*1024));
        if(!value.is_object() || value.value("SchemaVersion",0)!=1 || value.value("Kind",std::string{})!=kind
            || !value.contains("Entries") || !value["Entries"].is_array() || value["Entries"].size()>65536)
            throw std::runtime_error(std::string(kind)+" catalog is invalid; rebuild it");
        return value["Entries"];
    }
    void ExportCloneDraft(const nlohmann::json& draft) {
        if(!draft.is_object() || draft.size()!=1)throw std::runtime_error("Clone draft must contain exactly one asset entry");
        const auto& [path,fields]=*draft.items().begin();
        if(path.empty() || path.front()!='/' || !fields.is_object() || !fields.contains("$Clone") || !fields["$Clone"].is_string()
            || !fields.contains("PersistenceID") || !fields["PersistenceID"].is_string()
            || !fields.contains("InternalName") || !fields["InternalName"].is_string())
            throw std::runtime_error("Clone draft requires an output path, $Clone, PersistenceID and InternalName");
        const auto id=fields["PersistenceID"].get<std::string>();
        if(id.size()!=22 || std::string("AQgw").find(id.back())==std::string::npos)throw std::runtime_error("Clone PersistenceID is invalid");
        const auto directory=PS::HostServices::ExportsDirectory()/"assets";std::filesystem::create_directories(directory);
        const auto stamp=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        const auto file=directory/("clone_"+std::to_string(stamp)+".json");
        PS::ConfigFiles::Write(file,draft.dump(2));
        if(nlohmann::json::parse(PS::ConfigFiles::Read(file,1024*1024))!=draft)throw std::runtime_error("Clone export verification failed");
    }
    void GiveToolItem(UObject* controller,const std::string& path,int32_t count) {
        if(!controller || count<1 || count>10000)throw std::runtime_error("Item grant requires an authoritative player and quantity 1..10000");
        auto* item=ActorHelper::ResolveObject(RC::to_generic_string(path));
        auto* itemType=ActorHelper::ResolveClass(TEXT("/Script/Dominion.ItemData"));
        auto* inventory=ActorHelper::GetObjectRef(controller,TEXT("InventoryComponent"));
        auto* inventoryType=ActorHelper::ResolveClass(TEXT("/Script/Dominion.InventoryComponent"));
        if(!item || !itemType || !item->IsA(itemType))throw std::runtime_error("Selected item is unavailable or is not ItemData");
        if(PS::AssetMetadata::IsIncomplete(item))throw std::runtime_error("Item publication is incomplete; inventory grant blocked. Repair pending Helpy files with the game closed.");
        if(!inventory || !inventoryType || !inventory->IsA(inventoryType) || inventory->GetOuterPrivate()!=controller)
            throw std::runtime_error("Selected player's authoritative inventory is unavailable");
        const auto validate=[](const TCHAR* path,int32_t size,std::initializer_list<std::tuple<const TCHAR*,int32_t,int32_t>> fields) {
            auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path,false);
            if(!function || function->GetParmsSize()!=size)throw std::runtime_error("Native inventory command layout changed");
            size_t count=0;for(auto* field:TFieldRange<FProperty>(function,EFieldIterationFlags::Default))if(field->HasAnyPropertyFlags(CPF_Parm))++count;
            if(count!=fields.size())throw std::runtime_error("Native inventory command parameter count changed");
            for(const auto& [name,offset,width]:fields) {
                auto* field=function->FindProperty(FName(name,FNAME_Find));
                if(!field || !field->HasAnyPropertyFlags(CPF_Parm) || field->GetArrayDim()!=1
                    || field->GetOffset_Internal()!=offset || field->GetElementSize()!=width)
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
        if(!ready.Result<bool>())throw std::runtime_error("The selected inventory cannot accept that quantity");
        auto* tagType=UECustom::UObjectGlobals::StaticFindObject<UScriptStruct*>(nullptr,nullptr,TEXT("/Script/GameplayTags.GameplayTagContainer"));
        if(!tagType)throw std::runtime_error("Inventory tag container unavailable");
        FManagedStruct tags(tagType);struct EmptyTags{uint8_t Bytes[32];} empty{};std::memcpy(&empty,tags.GetData(),sizeof(empty));
        ActorHelper::FunctionCall give(inventory,givePath);give.Arg(TEXT("ItemData"),item).Arg(TEXT("Count"),count)
            .Arg(TEXT("DurabilityPercentage"),1.0f).Arg(TEXT("GameplayTags"),empty).Invoke();
        ActorHelper::FunctionCall afterCall(inventory,countPath);afterCall.Arg(TEXT("ItemData"),item).Invoke();const auto after=afterCall.Result<int32_t>();
        if(!give.Result<bool>() || int64_t(after)-before!=count)throw std::runtime_error("Native inventory grant could not be confirmed");
    }
}

std::string DragonWildsSpawnLoader::HandleNetworkHelpyAuthority(UObject* player,const std::string& action,const std::string& payload) {
    if(!player || !player->GetWorld() || !GetGameMode(player->GetWorld()))
        throw std::runtime_error("Helpy authority request did not arrive in an authoritative world");
    if(action!="GiveItems" && action!="Spawn" && action!="Players")
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
    std::string playerName;
    if(auto* state=ActorHelper::GetObjectRef(controller,TEXT("PlayerState"))) {
        ActorHelper::FunctionCall getName(state,TEXT("/Script/Engine.PlayerState:GetPlayerName"));getName.Invoke();
        const auto value=getName.Result<FString>();const auto& chars=value.GetCharArray();
        if(chars.Num()>1&&chars.Num()<256&&chars.GetData())playerName=RC::to_string(RC::StringType(chars.GetData(),chars.Num()-1));
    }
    if(action=="Players") {
        nlohmann::json roster=nlohmann::json::array();
        for(const auto& connected:GetConnectedPlayersInLoadOrder())roster.push_back({{"Name",connected.Name},{"Guid",connected.Guid}});
        return nlohmann::json{{"Kind","HelpyPlayers1"},{"Players",std::move(roster)}}.dump();
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
            || request["Items"].size()>PS::QuickUI::MaxSelection)
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
        if(count<1 || count>std::clamp(policy.maximumSpawnCount,1,64))
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
    if(actor->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)))
        return {false,"Building actor is a default/archetype/destroying object"};
    if(requireTransient && !actor->HasAnyFlags(RF_Transient))
        return {false,"Tool-created building actor lost RF_Transient"};

    auto* skip=CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(actorClass,TEXT("bSkipSpudStore")));
    if(!skip || skip->GetArrayDim()!=1)
        return {false,"BuildableActor has no verified bSkipSpudStore save-exclusion flag"};
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

std::pair<bool,std::string> DragonWildsSpawnLoader::VerifyToolBuildingCandidate(UObject* building,const SpawnInfo& spawn,UWorld* world) const {
    if(!building || !world || !GetGameMode(world))return {false,"No authoritative world is available for native building preflight"};
    auto* actorClass=ActorHelper::ResolveClass(spawn.ClassPath);
    if(!actorClass || !ActorHelper::IsActorClass(actorClass) || ActorHelper::IsAbstract(actorClass))
        return {false,"BuildableActor is not a concrete actor class"};
    if(!CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(actorClass,TEXT("bSkipSpudStore"))))
        return {false,"BuildableActor has no verified bSkipSpudStore save-exclusion flag"};
    if(spawn.BuildingObjectProperty.empty() && !spawn.bHasBuildingDataIndex)
        return {false,"BuildableActor exposes no verified BuildingPieceData binding"};

    TArray<UObject*> instances;
    UECustom::UObjectGlobals::GetObjectsOfClass(actorClass,instances,true);
    if(instances.Num()>8192)return {false,"BuildableActor native instance roster exceeds safety limit"};
    size_t considered=0;
    for(auto* object:instances) {
        auto* actor=object && object->IsA<AActor>()?static_cast<AActor*>(object):nullptr;
        if(!actor || actor->GetWorld()!=world
            || actor->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_Transient|RF_BeginDestroyed|RF_FinishDestroyed)))continue;
        ++considered;
        const auto verified=VerifyToolBuildingInstance(actor,building,spawn,world,false);
        if(verified.first)return {true,"Matched a native in-world instance with the exact BuildingPieceData binding"};
    }
    return {false,considered?"Native actors of this class exist, but none match this exact BuildingPieceData binding":"No native non-transient instance of this buildable actor exists in the current world"};
}

void DragonWildsSpawnLoader::PumpSpawnTools() {
    PS::SpawnToolRequests::Available=true;
    if(auto query=PS::SpawnToolRequests::TakeSearchHint())g_quickCatalogIndex.Prioritize(*query);
    g_quickCatalogIndex.Tick(); // Does not own or block the Spawn command slot.
    auto request=PS::SpawnToolRequests::Take();if(!request)return;
    const bool f2Spawn=request->value("_Source",std::string{}).starts_with("Helpy")&&request->value("Action",std::string{})=="Spawn";
    const auto requestId=request->value("_RequestId",uint64_t{});
    const char* spawnStage="recipient selection";
    try {
        const auto action=request->at("Action").get<std::string>();
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
        auto* controllerClass=ActorHelper::ResolveClass(TEXT("/Script/Dominion.DominionPlayerControllerBase"));
        if(!controllerClass)throw std::runtime_error("Load a world first");
        TArray<UObject*> controllers;UECustom::UObjectGlobals::GetObjectsOfClass(controllerClass,controllers,true);
        if(controllers.Num()<0||controllers.Num()>256)throw std::runtime_error("Player roster exceeds the safe bound");
        nlohmann::json players=nlohmann::json::array(),definitions=nlohmann::json::array(),items=nlohmann::json::array(),buildings=nlohmann::json::array();
        UObject* selected=nullptr;UWorld* catalogWorld=nullptr;
        for(auto* controller:controllers) {
            if(!controller || controller->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)) || !controller->GetWorld())continue;
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
            players.push_back({{"Name",name},{"Path",path}});
            if(path==request->value("Player",std::string{})){if(selected)throw std::runtime_error("Player selection is ambiguous; refresh");selected=controller;}
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
            auto result=PS::SpawnToolRequests::Read();result["Players"]=players;
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
            if(bFAssetDataAvailable) {
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
                    const auto [safe,reason]=VerifyToolBuildingCandidate(object,prototype,catalogWorld);
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
            if(!rows.is_array()||rows.empty()||rows.size()>PS::QuickUI::MaxSelection)throw std::runtime_error("Select 1..64 different items for a batch grant");
            std::vector<PS::QuickUI::Grant> grants;
            for(const auto& row:rows)grants.push_back({row.at("Item").get<std::string>(),row.at("Count").get<int>()});
            const auto outcomes=PS::QuickUI::ExecuteBatch(grants,[&](const PS::QuickUI::Grant& grant){GiveToolItem(selected,grant.item,grant.count);});
            auto result=PS::SpawnToolRequests::Read();result["Players"]=players;result["GrantResults"]=nlohmann::json::array();size_t confirmed=0;
            for(const auto& outcome:outcomes) {
                const bool ok=outcome.state==PS::QuickUI::BatchRow::State::Confirmed;if(ok)++confirmed;
                result["GrantResults"].push_back({{"Item",outcome.grant.item},{"Count",outcome.grant.count},
                    {"State",ok?"Confirmed":outcome.state==PS::QuickUI::BatchRow::State::Unconfirmed?"Unconfirmed":"NotAttempted"},{"Message",outcome.message}});
            }
            result["Status"]=confirmed==grants.size()?std::format("Granted {} selected item types; every inventory delta confirmed.",confirmed):
                std::format("{} / {} item types confirmed. Batch stopped; inspect inventory before retrying any unconfirmed item.",confirmed,grants.size());
            PS::SpawnToolRequests::Publish(std::move(result));return;
        }
        if(action=="GiveItem") {
            GiveToolItem(selected,request->at("Item").get<std::string>(),request->at("Count").get<int32_t>());
            auto result=PS::SpawnToolRequests::Read();result["Players"]=players;
            result["Status"]=std::format("Granted {} item(s) to the selected authoritative player and confirmed the inventory delta.",request->at("Count").get<int32_t>());
            PS::SpawnToolRequests::Publish(std::move(result));return;
        }
        if(action=="SpawnBuilding") {
            // Building previews are intentionally single-shot.  Unlike ordinary
            // actors, a bad construction script can take the process down before
            // C++ exception handling gets a chance to recover, so do not multiply
            // that risk with grids/batches.
            if(request->contains("Grid"))throw std::runtime_error("Building preview grids are disabled by the safety gate; spawn one verified piece at a time");
            const int count=request->value("Count",1);const double distance=request->at("Distance").get<double>();
            if(count!=1 || !std::isfinite(distance) || distance<3 || distance>50)
                throw std::runtime_error("Building previews require Count=1 and distance 3..50 metres");
            const auto buildingPath=request->at("Building").get<std::string>();
            const double yaw=request->value("Yaw",0.0),height=request->value("Height",0.0),scale=request->value("Scale",1.0);
            if(!std::isfinite(yaw) || !std::isfinite(height) || std::abs(height)>1000 || !std::isfinite(scale) || scale<=0.0)
                throw std::runtime_error("Building scale/yaw/height is invalid");
            auto authored=nlohmann::json{{"Id","candidate"},{"Type","BuildingProp"},{"Building",buildingPath},
                {"Scale",scale},{"AllowDeconstruction",request->value("AllowDeconstruction",false)},{"UseNativeRespawn",false},
                {"Location",nlohmann::json::array({0,0,0})},{"Rotation",{{"Pitch",0},{"Yaw",yaw},{"Roll",0}}}};
            const auto time=request->value("TimeOfDay",std::string("Any"));
            if(time!="Any")authored["TimeOfDay"]=time;
            SpawnInfo prototype;prototype.ModName=TEXT("__RuneSchemaTools");prototype.EntryId=TEXT("candidate");
            RegisterBuildingProp(prototype,authored);
            auto* buildingObject=ActorHelper::ResolveObject(RC::to_generic_string(buildingPath));
            const auto [buildingSafe,buildingReason]=VerifyToolBuildingCandidate(buildingObject,prototype,world);
            if(!buildingSafe)throw std::runtime_error("Building preview blocked by safety preflight: "+buildingReason);
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
            if(!UECustom::UKismetSystemLibrary::LineTraceGround(world,FVector(x,y,origin.Z()+500),FVector(x,y,origin.Z()-1500),{pawn},ground,error))
                throw std::runtime_error("No ground at the requested building position; nothing spawned. "+error);
            positions.emplace_back(ground.X(),ground.Y(),ground.Z()+height);
            try {
                for(const auto& position:positions) {
                    auto* actor=ActorHelper::SpawnActor(world,buildingClass,position,FRotator(0,yaw,0),[&](AActor* value){
                        value->SetFlags(RF_Transient);
                        auto* skip=CastField<FBoolProperty>(PropertyHelper::GetPropertyByName(value->GetClassPrivate(),TEXT("bSkipSpudStore")));
                        if(!skip || skip->GetArrayDim()!=1)throw std::runtime_error("Building preview lost its bSkipSpudStore safety contract before construction");
                        skip->SetPropertyValue(skip->ContainerPtrToValuePtr<void>(value),true);
                        ApplyBuildingData(value,prototype);
                    });
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
                    if(ordinary->VisualEffect.value("Type",std::string{})!="Ghost")throw std::runtime_error("This spawn visual cannot be copied safely by the tool");
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
        if(!std::isfinite(requestedScale) || requestedScale<=0.0)
            throw std::runtime_error("AI/resource instance scale must be finite and greater than zero");
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
        const auto origin=ActorHelper::GetActorLocation(pawn);
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
            if(!UECustom::UKismetSystemLibrary::LineTraceGround(world,FVector(x,y,origin.Z()+500),FVector(x,y,origin.Z()-1500),{pawn},ground,error))
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
                    });
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
        if(request->value("Action",std::string{})=="CreateClone")result["CloneResult"]={{"State","Stopped"}};
        if(f2Spawn){
            result["Status"]=std::string("Spawn stopped [")+spawnStage+"]: "+e.what();
            result["SpawnResult"]={{"State","Stopped"},{"Stage",spawnStage}};
            PS::Log<LogLevel::Warning>(STR("RuneSchema Helpy spawn #{} stopped [{}]: {}\n"),requestId,RC::to_generic_string(spawnStage),RC::to_generic_string(e.what()));
        }
        PS::SpawnToolRequests::Publish(std::move(result));
    }catch(...){
        auto result=PS::SpawnToolRequests::Read();result["Status"]="The runtime action stopped after an unknown error; inspect the world or inventory before retrying.";
        if(request->value("Action",std::string{})=="CreateClone")result["CloneResult"]={{"State","Stopped"}};
        if(f2Spawn){result["SpawnResult"]={{"State","Stopped"},{"Stage",spawnStage}};PS::Log<LogLevel::Warning>(STR("RuneSchema Helpy spawn #{} stopped [{}]: unknown error.\n"),requestId,RC::to_generic_string(spawnStage));}
        PS::SpawnToolRequests::Publish(std::move(result));
    }
}
