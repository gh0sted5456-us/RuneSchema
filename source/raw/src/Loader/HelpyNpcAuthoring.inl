// Included in DragonWildsNpcLoader.cpp after native helper declarations.
// All operations are game-thread-only; temporaries are never added to m_catalog or written to /npc.
namespace {
double HelpyNpcClock() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void ValidateHelpyNpcLifespan() {
    auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,
        TEXT("/Script/Engine.Actor:SetLifeSpan"),false);
    auto* input=function?PropertyHelper::CastProperty<FNumericProperty>(
        function->FindProperty(FName(TEXT("InLifespan"),FNAME_Find))):nullptr;
    unsigned inputs=0;
    if(function)for(auto* property:TFieldRange<FProperty>(function,EFieldIterationFlags::Default))
        if(property->HasAnyPropertyFlags(CPF_Parm))++inputs;
    if(!function||function->GetParmsSize()!=sizeof(float)||function->GetReturnProperty()||inputs!=1
        ||!input||input->GetArrayDim()!=1||!input->IsFloatingPoint()||input->GetElementSize()!=sizeof(float)
        ||input->GetOffset_Internal()!=0||!input->HasAnyPropertyFlags(CPF_Parm)
        ||input->HasAnyPropertyFlags(CPF_OutParm|CPF_ReturnParm))
        throw std::runtime_error("NPC native lifespan contract is unavailable; timed spawning is blocked.");
}
void HelpyNpcLifespan(AActor* actor,int seconds) {
    // Native lifespan backs up our owned cleanup queue. No guessed memory offsets.
    ActorHelper::FunctionCall(actor,TEXT("/Script/Engine.Actor:SetLifeSpan"))
        .Arg(TEXT("InLifespan"),static_cast<float>(seconds)).Invoke();
}
}
std::string DragonWildsNpcLoader::HelpyTemporaryReason(const VendorDefinition& d) {
    if(!d.Enabled)return "Source NPC is disabled.";
    // Temporary definitions retain the source's resolved dialogue, lore, quest and
    // vendor references. Their actor lease remains session-only; the owning loaders
    // continue to own progression and unlock state.
    try {HelpyNpcGuards::TemporaryClass(ActorHelper::ResolveClass(RC::to_generic_string(d.BaseActorClassPath)));ValidateHelpyNpcLifespan();}
    catch(const std::exception& e){return e.what();}
    catch(...) {return "NPC preflight failed; no temporary actor can be created.";}
    return {};
}
nlohmann::json DragonWildsNpcLoader::HelpyNpcDocument(const VendorDefinition& source,const std::string& id) {
    // Select only the /npc contract, not resolved storefront internals or pointers.
    if(!source.HelpySource.is_object())throw std::runtime_error("Original NPC authoring definition is unavailable.");
    if(source.BaseActorClassPath!="/Game/Gameplay/NPCs/BP_BaseInteractableNPC.BP_BaseInteractableNPC_C")
        throw std::runtime_error("Permanent copies of legacy custom proxy classes need an explicit /npc conversion.");
    auto doc=nlohmann::json::object();
    for(const auto* field:{"DisplayName","VisualSource","Mesh","Materials","IdleAnimation","HideMesh","NoInteract","HideName",
        "Location","Rotation","Scale","Enabled","EnableCollision","MeshCollision","Type","Appearance","Equipment","Pose","DialoguePose","Ghost","VisualEffect","Map","OverheadIcon","VendorID","DialogueID","LoreID","LoreEntry","QuestID","Multiplayer","TimeOfDay"})
        if(source.HelpySource.contains(field))doc[field]=source.HelpySource[field];
    if(doc.contains("LoreID"))doc.erase("LoreEntry"); // Resolve() adds the runtime alias; authored copies keep the preferred reference.
    doc["Id"]=id;doc["Enabled"]=true;doc["TimeOfDay"]="Any";doc["Multiplayer"]=source.Multiplayer;
    if(!source.DialogueKey.empty())doc["DialogueID"]=source.DialogueKey;
    if(!source.LoreEntry.empty() && !doc.contains("LoreID"))doc["LoreEntry"]=source.LoreEntry; // Legacy/global lore reference.
    if(!source.QuestKey.empty())doc["QuestID"]=source.QuestKey;
    if(source.Stage==VendorPolicy::VendorStage::Merchant) {
        if(!source.StoreOwner.starts_with("store:"))
            throw std::runtime_error("This legacy inline merchant has no reusable /vendors store ID; temporary mode only.");
        doc["VendorID"]=source.StoreOwner.substr(6);
    }else doc.erase("VendorID");
    // Validate the same contract that will read the file next launch. This local
    // catalog only validates its own definition; it does not re-run every mod.
    NpcCatalog check;check.AddNpc("runeschema",doc);
    return doc;
}
nlohmann::json DragonWildsNpcLoader::HelpyDefinitions() {
    auto rows=nlohmann::json::array();
    for(const auto& d:m_definitions) {
        if(d.HelpyTemporary)continue;
        const auto key="@runeschema-npc:"+d.ModName+":"+d.Id;
        const auto temporary=HelpyTemporaryReason(d);
        std::string permanent;
        try {(void)HelpyNpcDocument(d,"helpy_preflight");if(!d.Enabled)permanent="Source NPC is disabled.";}
        catch(const std::exception& e){permanent=e.what();}
        catch(...) {permanent="NPC placement preflight failed.";}
        rows.push_back({{"Key",key},{"Name",d.DisplayName.empty()?d.Id:d.DisplayName},{"Class",d.BaseActorClassPath},
            {"Type","NPC"},{"RuneSchemaManaged",true},{"Available",d.Enabled},{"Loaded",true},
            {"Vendor",d.StoreOwner.starts_with("store:")},{"QuestGiver",!d.QuestKey.empty()},
            {"Lore",!d.LoreEntry.empty()},{"Dialogue",!d.DialogueKey.empty()},
            {"Reason",d.Enabled?"":"Source NPC is disabled"},{"TemporaryAllowed",temporary.empty()},
            {"TemporaryReason",temporary},{"PermanentAllowed",permanent.empty()},{"PermanentReason",permanent}});
    }
    return rows;
}
AActor* DragonWildsNpcLoader::FindHelpyNpc(UWorld* world,const VendorDefinition& definition) const {
    if(!world||!definition.HelpyTemporary||!definition.Enabled)return nullptr;
    const auto key=definition.ModName+":"+definition.Id;
    AActor* found=nullptr;
    for(const auto& lease:m_helpyNpcs)if(lease.Key==key) {
        if(lease.Lifetime.Due(HelpyNpcClock())||lease.World.Get()!=world)return nullptr;
        auto* object=lease.Actor.Get();
        if(!object||object->GetWorld()!=world||!IsNpcObjectUsable(object))return nullptr;
        if(!definition.BaseActorClass||!object->IsA(definition.BaseActorClass))
            throw std::runtime_error("Helpy NPC lease changed its native class.");
        auto* actor=static_cast<AActor*>(object);
        HelpyNpcGuards::VerifyExcluded(actor);
        if(found)throw std::runtime_error("Ambiguous Helpy NPC lease; refusing interaction.");
        found=actor;
    }
    return found;
}
void DragonWildsNpcLoader::RetireHelpyNpc(HelpyNpcLease& lease) {
    lease.Lifetime.Retire();
    for(auto& d:m_definitions)if(d.HelpyTemporary&&d.ModName+":"+d.Id==lease.Key)d.Enabled=false;
    // Remove our acknowledgement route before destroying anything. Never toggle
    // a UI blindly: the native actor/component EndPlay owns its widget teardown.
    std::erase_if(m_merchantBindings,[&](const auto& b){return b.DefinitionKey==lease.Key;});
    std::erase_if(m_dialogueSessions,[&](const auto& v){return v.second&&v.second->NpcKey==lease.Key;});
    // Presentation/application records can outlive the actor's weak reference.
    // Erase by the exact recorded owner path/key, never by dereferencing a stale token.
    m_applied.erase(lease.AppliedKey);
    if(!lease.ObjectPath.empty())std::erase_if(m_npcNames,[&](const auto& entry) {
        return entry.first==lease.ObjectPath||entry.first.starts_with(lease.ObjectPath+TEXT("."))
            ||entry.first.starts_with(lease.ObjectPath+TEXT(":"));
    });
    std::erase_if(m_spawnedVendors,[&](const auto& entry){return entry.Key==lease.Key;});
    auto* object=lease.Actor.Get();if(!object)return;
    auto* actor=static_cast<AActor*>(object);
    QueueNpcCleanup(actor); // root/queue first so a failed native destroy is retryable
    if(IsNpcObjectUsable(actor)) {
        try {ActorHelper::FunctionCall(actor,TEXT("/Script/Engine.Actor:SetActorEnableCollision")).Arg(TEXT("bNewActorEnableCollision"),false).Invoke();}catch(...) {}
        ActorHelper::DestroyActor(actor);
    }
}
void DragonWildsNpcLoader::PumpHelpyNpcs() {
    const auto now=HelpyNpcClock();
    for(auto it=m_helpyNpcs.begin();it!=m_helpyNpcs.end();) {
        try {
            auto* actor=it->Actor.Get();
            if(!actor||!IsNpcObjectUsable(actor)) {
                RetireHelpyNpc(*it);
                // The normal cleanup queue, not the timer, releases any acquired root.
                it=m_helpyNpcs.erase(it);continue;
            }
            if(!it->World.Get()||it->World.Get()!=actor->GetWorld()||it->Lifetime.Due(now))RetireHelpyNpc(*it);
        }catch(const std::exception& e){ErrorOnce("helpy-npc-cleanup:"+it->Key,PS::ToWideSafe(e.what()));}
        catch(...) { /* keep ownership and retry on the next engine tick */ }
        ++it;
    }
}
std::size_t DragonWildsNpcLoader::DismissHelpyNpcs(UWorld* world) {
    std::size_t requested=0;
    for(auto& lease:m_helpyNpcs)if(!world||lease.World.Get()==world) {
        ++requested;lease.Lifetime.Retire();
        try {RetireHelpyNpc(lease);}catch(const std::exception& e){ErrorOnce("helpy-npc-dismiss:"+lease.Key,PS::ToWideSafe(e.what()));}
        catch(...) { /* Retiring remains sticky; another NPC must still be dismissed. */ }
    }
    return requested;
}
nlohmann::json DragonWildsNpcLoader::SpawnHelpyNpc(const nlohmann::json& request,UWorld* world,AActor* player) {
    using namespace PS::HelpyNodes;
    if(m_helpySpawning)throw std::runtime_error("Another Helpy NPC operation is in progress.");
    if(!world||!player||player->GetWorld()!=world||PS::Network::Detect(world).Mode!=PS::Network::Role::Standalone)
        throw std::runtime_error("Helpy NPC authoring is single-player-only until temporary UI/network cleanup is verified.");
    if(m_worldTeardownCallbackId==Hook::ERROR_ID)throw std::runtime_error("NPC travel cleanup hook is unavailable; no temporary or portable NPC is created.");
    const auto thread=m_gameThreadId.load(std::memory_order_relaxed);
    if(!thread||thread!=GetCurrentThreadId())throw std::runtime_error("NPC authoring must run on the game thread.");
    const auto key=request.at("Definition").get<std::string>();const bool permanent=request.value("Permanent",false);
    if(request.contains("DurationSeconds")&&(!request["DurationSeconds"].is_number_integer()
        ||request["DurationSeconds"]<0||request["DurationSeconds"]>MaxDuration))
        throw std::runtime_error("NPC duration must be a bounded whole number of seconds.");
    const int seconds=request.value("DurationSeconds",0);ValidateDuration(permanent,seconds);
    if(request.contains("Count")&&(!request["Count"].is_number_integer()||request["Count"]!=1))
        throw std::runtime_error("NPC placement creates one actor per request.");
    if(request.value("Count",1)!=1||request.value("Resource",false)||request.contains("PowerLevel")||request.contains("GhostMesh")
        ||(request.contains("AdditionalDrops")&&!request["AdditionalDrops"].empty()))throw std::runtime_error("NPC placement cannot accept AI power, drops, effects or a batch count.");
    const auto name=request.value("Name",std::string{});if(name.size()>256||name.find('\0')!=name.npos)throw std::runtime_error("NPC name is invalid.");
    const double scale=request.value("Scale",1.0);if(!std::isfinite(scale)||scale<0.01||scale>100.0)throw std::runtime_error("NPC scale must be finite and between 0.01 and 100.");
    if(m_helpyNpcs.size()+m_pendingNpcCleanup.size()>=MaxTemporary)throw std::runtime_error("32 NPCs/cleanup records are outstanding; let cleanup finish first.");
    if(m_helpyCreated>=128)throw std::runtime_error("128 Helpy NPC operations reached in this process; restart before another authoring cycle.");
    const auto generation=m_worldGeneration;
    struct Busy {bool& flag;Busy(bool& f):flag(f){flag=true;}~Busy(){flag=false;}} busy(m_helpySpawning);
    std::optional<VendorDefinition> copy;
    const auto stem=PS::Authoring::NewName();
    if(key.starts_with("@runeschema-npc:")) {
        const auto sourceKey=key.substr(16);
        const auto found=std::find_if(m_definitions.begin(),m_definitions.end(),[&](const auto& d){return !d.HelpyTemporary&&d.ModName+":"+d.Id==sourceKey;});
        if(found==m_definitions.end()||!found->Enabled)throw std::runtime_error("Source NPC is unavailable; refresh the catalogue.");
        if(request.value("Class",std::string{})!=found->BaseActorClassPath)throw std::runtime_error("NPC class changed; select it again.");
        if(!permanent) {const auto why=HelpyTemporaryReason(*found);if(!why.empty())throw std::runtime_error(why);}
        copy=*found;
        // Share the source store's ownership; expiry never deletes its shared rows.
        if(copy->StoreOwner.empty())copy->StoreOwner=VendorOffers::Owner(found->ModName,found->Id);
    }else {
        // Native storyline objects are browsable, not silently converted into
        // RuneSchema neutral proxies with guessed interaction contracts.
        throw std::runtime_error("Select a RuneSchema NPC definition for portable NPC creation. Vanilla NPCs remain browsable in this cycle.");
    }
    auto& d=*copy;
    auto document=permanent?HelpyNpcDocument(d,stem):nlohmann::json{};
    d.Id=stem;d.ModName="runeschema";d.HelpyTemporary=!permanent;
    d.Time=TimeOfDay::Requirement::Any;d.Multiplayer=true;d.SpawnGate.ResetForMap();
    if(!name.empty())d.DisplayName=name;
    for(auto& value:d.SpawnScale)value=scale;
    if(std::any_of(m_definitions.begin(),m_definitions.end(),[&](const auto& e){return e.ModName==d.ModName&&e.Id==d.Id;}))
        throw std::runtime_error("Generated NPC identity already exists; no actor was created.");
    const auto origin=ActorHelper::GetActorLocation(player);
    if(!std::isfinite(origin.X())||!std::isfinite(origin.Y())||!std::isfinite(origin.Z()))
        throw std::runtime_error("Player location is not finite; no NPC created.");
    const auto placement=request.value("PlacementMode",std::string("Radius"));double targetX=origin.X(),targetY=origin.Y();
    if(placement=="Radius") {
        const double radius=request.value("PlacementRadius",500.0);
        if(!std::isfinite(radius)||radius<0||radius>10000)throw std::runtime_error("NPC radius must be 0..10000 centimetres.");
        // Derive two stable fractions from this generated identity. This avoids a
        // shared RNG while distributing repeated requests uniformly across a disk.
        const auto hash=std::hash<std::string>{}(stem),hash2=std::hash<std::string>{}(stem+":radius");
        constexpr double tau=6.28318530717958647692;
        const double angle=(static_cast<double>(hash&0xffffff)/16777216.0)*tau;
        const double distance=std::sqrt(static_cast<double>(hash2&0xffffff)/16777216.0)*radius;
        targetX+=std::cos(angle)*distance;targetY+=std::sin(angle)*distance;
    }else if(placement=="Grid") {
        const double cell=request.value("GridSize",100.0),x=request.value("GridX",0.0),y=request.value("GridY",0.0);
        if(!std::isfinite(cell)||cell<10||cell>1000||!std::isfinite(x)||!std::isfinite(y)||std::abs(x)>100||std::abs(y)>100)
            throw std::runtime_error("NPC grid uses X/Y -100..100 cells and a 10..1000 cm cell size.");
        targetX=std::round(origin.X()/cell)*cell+x*cell;targetY=std::round(origin.Y()/cell)*cell+y*cell;
    }else throw std::runtime_error("NPC placement mode must be Radius or Grid.");
    if(!std::isfinite(targetX)||!std::isfinite(targetY))throw std::runtime_error("NPC horizontal placement is not finite.");
    // Preserve an authored $z placement until the shared NPC spawn path runs.
    // Resolving here and disabling grounding caused the later actor path to treat
    // a provisional trace as an absolute transform, which could leave NPCs above
    // or inside streamed terrain and prevent normal navigation.
    d.TargetLocation[0]=targetX;d.TargetLocation[1]=targetY;d.TargetLocation[2]=origin.Z();
    d.GroundToSurface=true;d.GroundOffset=100;d.HasTargetLocation=true;d.SpawnRotation[0]=0;d.SpawnRotation[1]=0;d.SpawnRotation[2]=0;
    d.PersistentId=VendorIdentity::ForOwner(VendorOffers::Owner(d.ModName,d.Id));
    if(!ResolveDefinitionClasses(d))throw std::runtime_error("NPC native class/component contracts are not ready.");
    if(!permanent)HelpyNpcGuards::TemporaryClass(d.BaseActorClass);
    if(m_definitions.size()>=65536)throw std::runtime_error("NPC definition capacity reached.");
    m_definitions.reserve(m_definitions.size()+1);m_helpyNpcs.reserve(m_helpyNpcs.size()+1);m_spawnedVendors.reserve(m_spawnedVendors.size()+1);
    const auto ownKey=d.ModName+":"+d.Id;
    std::unique_ptr<PS::Authoring::StagedFile> file;
    if(permanent) {
        document["Location"]={{"X",d.TargetLocation[0]},{"Y",d.TargetLocation[1]},{"Z","$+100"}};
        document["Rotation"]={{"Pitch",0},{"Yaw",0},{"Roll",0}};document["Scale"]=scale;document["DisplayName"]=d.DisplayName;
        NpcCatalog validate;validate.AddNpc(d.ModName,document);
        d.HelpySource=document;
        file=std::make_unique<PS::Authoring::StagedFile>("npc",stem,document);
        if(generation!=m_worldGeneration)throw std::runtime_error("World changed before NPC publication; no placement saved.");
        file->Commit();
    }
    // Definitions are installed before the actor can acknowledge an interaction.
    // Use a local copy during reflected calls so world changes cannot invalidate it.
    AActor* actor=nullptr;
    try {
        m_definitions.push_back(d);++m_helpyCreated;
        if(d.Stage==VendorPolicy::VendorStage::Merchant&&d.InlineMerchant)CreateInlineMerchantRow(d);
        actor=SpawnVisualVendor(world,d);
        if(!actor)throw std::runtime_error("NPC creation returned no actor.");
        if(!permanent) {
            HelpyNpcGuards::VerifyExcluded(actor);
            m_helpyNpcs.push_back({PS::WeakObject(actor),PS::WeakObject(world),ownKey,Lease::Start(HelpyNpcClock(),seconds),ActorKey(actor,d),actor->GetPathName()});
            HelpyNpcLifespan(actor,seconds);
        }
        if(generation!=m_worldGeneration)throw std::runtime_error("World changed during NPC setup; new actor retired.");
        m_spawnedVendors.push_back({ownKey,PS::WeakObject(actor)});
        for(auto& entry:m_definitions)if(entry.ModName+":"+entry.Id==ownKey){entry.SpawnGate.Begin();break;}
        nlohmann::json result={{"State","Completed"},{"Permanent",permanent},{"Activated",1},{"Requested",1},{"NpcId",ownKey},{"DurationSeconds",seconds},
            {"PlacementMode",placement},{"X",targetX},{"Y",targetY},{"Z","$+100"}};
        if(file)result["File"]=file->Path().string();
        return result;
    }catch(...) {
        const auto failure=std::current_exception();
        std::string error="Unknown native NPC setup failure";
        try {std::rethrow_exception(failure);}catch(const std::exception& e){error=e.what();}catch(...) {}
        // Disable every route before a native destroy, including a lease created
        // just before SetLifeSpan failed. Never leave a failed actor interactive.
        for(auto& entry:m_definitions)if(entry.ModName+":"+entry.Id==ownKey){entry.Enabled=false;break;}
        std::erase_if(m_merchantBindings,[&](const auto& binding){return binding.DefinitionKey==ownKey;});
        for(auto& lease:m_helpyNpcs)if(lease.Key==ownKey)lease.Lifetime.Retire();
        if(actor){try{QueueNpcCleanup(actor);ActorHelper::DestroyActor(actor);}catch(...) {}}
        // A committed author file is retained for explicit repair/restart, not
        // guessed rollback. In-memory failure cannot claim a successful spawn.
        if(!file)std::rethrow_exception(failure);
        return {{"State","SavedActivationIncomplete"},{"Permanent",true},{"Activated",0},{"Requested",1},{"File",file->Path().string()},
            {"Error",error},{"NpcId",ownKey}};
    }
}
