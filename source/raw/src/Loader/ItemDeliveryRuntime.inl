// Included by DragonWildsSpawnLoader.cpp.
namespace PS::ItemDeliveryRuntime {
using namespace RC::Unreal;
namespace AH=DragonWilds::ActorHelper;
namespace Ref=DragonWilds::ReflectedContract;
namespace ID=PS::ItemDelivery;
using Clock=std::chrono::steady_clock;
using Json=nlohmann::json;

constexpr auto SavePath=TEXT("/Script/Dominion.PersistenceSubsystem:SavePlayerState");
constexpr auto DispatchPath=TEXT("/Script/Dominion.DominionPlayerControllerBase:Client_SaveToDisk");
constexpr auto SlotPath=TEXT("/Script/Dominion.DominionPlayerControllerBase:GetCharacterSaveSlot");
constexpr auto ResultPath=TEXT("/Script/Dominion.DominionPlayerControllerBase:Server_ClientSaveToDiskResult");
constexpr size_t ReceiptBytes=4*1024*1024;
constexpr auto InvalidFlags=static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|
    RF_NeedLoad|RF_NeedPostLoad|RF_BeginDestroyed|RF_FinishDestroyed);

void Report(Diagnostics::Id code,const std::string& reason,const ID::Item& item){
    PS::CodedLog<RC::LogLevel::Warning>(code,STR("Item Delivery | owner={} | delivery={} | item={} | {}\n"),
        PS::ToWideSafe(item.Owner),PS::ToWideSafe(item.Delivery),
        PS::ToWideSafe(item.Path),PS::ToWideSafe(reason));
}

std::vector<FProperty*> Inputs(UFunction* function){
    Ref::Frame(function);
    std::vector<FProperty*> fields;
    for(auto* field:TFieldRange<FProperty>(function,EFieldIterationFlags::IncludeDeprecated))
        if(Ref::Input(field,function))fields.push_back(field);
    std::ranges::sort(fields,{},[](auto* field){return field->GetOffset_Internal();});
    return fields;
}

std::string SlotText(FStrProperty* field,void* record){
    const auto& chars=field->ContainerPtrToValuePtr<FString>(record)->GetCharArray();
    if(chars.Num()<2||chars.Num()>513||!chars.GetData()||chars.GetData()[chars.Num()-1]!=0)
        throw std::runtime_error("Native save slot identity is unavailable");
    return RC::to_string(RC::StringType(chars.GetData(),chars.Num()-1));
}

struct SaveFrame {
    FStructProperty* Handle=nullptr;
    FStructProperty* Slot=nullptr;
    FNumericProperty* HandleValue=nullptr;
    FStrProperty* SlotName=nullptr;
    FProperty* Result=nullptr;
    FNumericProperty* ResultValue=nullptr;
    bool Opaque=false;

    static SaveFrame Bind(UFunction* function,bool completion){
        const auto fields=Inputs(function);
        SaveFrame frame;
        if(!completion&&fields.size()==1){
            auto* wrapped=CastField<FStructProperty>(fields.front());
            if(wrapped&&wrapped->GetStruct()&&wrapped->GetStruct()->GetName()==TEXT("DomClientDiskSave")){
                frame.Opaque=true;return frame;
            }
        }
        if(fields.size()<2||(completion&&fields.size()!=3))
            throw std::runtime_error("Native character save RPC parameters changed");
        frame.Handle=CastField<FStructProperty>(fields[0]);
        frame.Slot=CastField<FStructProperty>(fields[1]);
        auto* handle=frame.Handle?frame.Handle->GetStruct().Get():nullptr;
        auto* slot=frame.Slot?frame.Slot->GetStruct().Get():nullptr;
        if(!handle||!slot)throw std::runtime_error("Native save handle or slot is unavailable");
        frame.HandleValue=CastField<FNumericProperty>(handle->FindProperty(FName(TEXT("Handle"),FNAME_Find)));
        frame.SlotName=CastField<FStrProperty>(slot->FindProperty(FName(TEXT("DisplaySlotName"),FNAME_Find)));
        if(!Ref::Fits(frame.HandleValue,frame.Handle->GetElementSize())||!frame.HandleValue->IsInteger()
            ||!Ref::Fits(frame.SlotName,frame.Slot->GetElementSize())||frame.SlotName->GetElementSize()!=sizeof(FString))
            throw std::runtime_error("Native save handle or slot identity changed");
        if(completion){
            frame.Result=fields[2];
            frame.ResultValue=CastField<FNumericProperty>(frame.Result);
            if(auto* enumeration=CastField<FEnumProperty>(frame.Result))frame.ResultValue=enumeration->GetUnderlyingProperty();
            if(!frame.ResultValue||!frame.ResultValue->IsInteger()||frame.ResultValue->GetSize()!=frame.Result->GetSize())
                throw std::runtime_error("Native character save result changed");
        }
        return frame;
    }
    ID::SaveTicket Ticket(void* parameters)const{
        const auto id=HandleValue->GetSignedIntPropertyValue(
            HandleValue->ContainerPtrToValuePtr<void>(Handle->ContainerPtrToValuePtr<void>(parameters)));
        return {id,SlotText(SlotName,Slot->ContainerPtrToValuePtr<void>(parameters))};
    }
};

ID::Receipts ReadReceipts(const std::filesystem::path& path){
    CacheBundleIO::Safe(path);
    std::error_code error;const auto status=std::filesystem::symlink_status(path,error);
    if(error==std::errc::no_such_file_or_directory)return ID::Receipts(ID::Receipts::Empty());
    if(error||!std::filesystem::is_regular_file(status))throw std::runtime_error("Item delivery receipt is unreadable");
    return ID::Receipts(Json::parse(CacheBundleIO::Read(path,ReceiptBytes)));
}
void WriteReceipts(const std::filesystem::path& path,const ID::Receipts& receipts){
    const auto bytes=receipts.Document().dump();
    if(bytes.size()>ReceiptBytes)throw std::runtime_error("Item delivery receipt exceeds its size limit");
    CacheBundleIO::Publish(path,bytes);
}
bool CallBool(UObject* object,const TCHAR* path){
    AH::FunctionCall call(object,path);call.Invoke();return call.Result<bool>();
}
bool Live(UObject* object){return object&&!object->HasAnyFlags(InvalidFlags);}

class Service {
    struct Join {WeakObjectHandle Controller;std::string Character;size_t Next=0;};
    struct PendingSave {
        WeakObjectHandle Controller;
        std::string Character;
        std::filesystem::path Journal;
        const ID::Item* Item=nullptr;
        Clock::time_point Deadline;
        ID::SaveReceipt Receipt;
    };
    struct DispatchCount {WeakObjectHandle Controller;unsigned Count=0;};
    std::map<int32_t,DispatchCount> m_dispatches;
    std::vector<ID::Item> m_items;
    std::vector<Join> m_joins;
    std::set<std::pair<Diagnostics::Id,std::string>> m_reported;
    WeakObjectHandle m_world;
    Clock::time_point m_next{};
    std::optional<PendingSave> m_pending;
    UFunction* m_save=nullptr;
    UFunction* m_dispatch=nullptr;
    UFunction* m_result=nullptr;
    UFunction* m_slot=nullptr;
    FStructProperty* m_slotReturn=nullptr;
    FStrProperty* m_slotName=nullptr;
    RC::StringType m_saveArgument;
    SaveFrame m_dispatchFrame,m_resultFrame;
    Hook::GlobalCallbackId m_dispatchHook=Hook::ERROR_ID,m_resultHook=Hook::ERROR_ID;
    DWORD m_gameThread=0;
    bool m_requested=false,m_broken=false,m_always=false;

    void Once(Diagnostics::Id code,const std::string& reason,const ID::Item& item){
        if(m_reported.emplace(code,item.Key()).second)Report(code,reason,item);
    }
    static UFunction* Find(const TCHAR* path){
        return UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path);
    }
    DispatchCount& Dispatches(UObject* controller){
        auto& entry=m_dispatches[controller->GetInternalIndex()];
        if(entry.Controller.Get()!=controller)entry={WeakObjectHandle(controller),0};
        return entry;
    }
    std::string CurrentSlot(UObject* controller)const{
        std::vector<std::byte> slot(m_slotReturn->GetSize());
        AH::FunctionCall call(controller,m_slot);call.Invoke();
        call.MoveResult(slot.data(),slot.size());
        struct Cleanup {FStructProperty* Field;void* Value;~Cleanup(){Field->DestroyValue(Value);}} cleanup{m_slotReturn,slot.data()};
        return SlotText(m_slotName,slot.data());
    }
    void Bind(){
        m_save=Find(SavePath);m_dispatch=Find(DispatchPath);m_result=Find(ResultPath);
        const auto fields=Inputs(m_save);
        if(fields.size()!=1||!CastField<FObjectPropertyBase>(fields.front())||fields.front()->GetSize()!=sizeof(UObject*))
            throw std::runtime_error("Native SavePlayerState controller parameter changed");
        m_saveArgument=fields.front()->GetName();
        m_dispatchFrame=SaveFrame::Bind(m_dispatch,false);
        m_resultFrame=SaveFrame::Bind(m_result,true);
        if(m_dispatchFrame.Opaque){
            m_slot=Find(SlotPath);
            if(!Inputs(m_slot).empty())throw std::runtime_error("Native character slot getter changed");
            m_slotReturn=CastField<FStructProperty>(m_slot->GetReturnProperty());
            auto* type=m_slotReturn?m_slotReturn->GetStruct().Get():nullptr;
            m_slotName=type?CastField<FStrProperty>(type->FindProperty(FName(TEXT("DisplaySlotName"),FNAME_Find))):nullptr;
            if(!Ref::Output(m_slotReturn,m_slot)||!Ref::Fits(m_slotName,m_slotReturn->GetElementSize())
                ||m_slotName->GetElementSize()!=sizeof(FString))
                throw std::runtime_error("Native character slot identity changed");
        }
        Hook::FCallbackOptions options{};
        options.OwnerModName=TEXT("RuneSchema");options.HookName=TEXT("ItemDeliverySaveDispatch");
        m_dispatchHook=Hook::RegisterProcessEventPreCallback(
            [this](Hook::TCallbackIterationData<void>&,UObject* controller,UFunction* fn,void* parameters){
                if(fn!=m_dispatch||GetCurrentThreadId()!=m_gameThread||!controller||!parameters)return;
                const bool ours=m_pending&&m_pending->Controller.Get()==controller;
                try{
                    if(m_dispatchFrame.Opaque){
                        const auto previous=Dispatches(controller).Count++;
                        if(ours){
                            // The current RPC wraps an opaque payload. Pair it only when our
                            // call dispatched the sole outstanding save for this controller.
                            if(m_requested)m_pending->Receipt.DispatchOpaque(previous==0);
                            else m_pending->Receipt.Ambiguous=true;
                        }
                    }else if(ours&&m_requested)m_pending->Receipt.Dispatch(m_dispatchFrame.Ticket(parameters));
                }catch(...){if(ours)m_pending->Receipt.Ambiguous=true;}
            },options);
        options.HookName=TEXT("ItemDeliverySaveResult");
        m_resultHook=Hook::RegisterProcessEventPostCallback(
            [this](Hook::TCallbackIterationData<void>&,UObject* controller,UFunction* fn,void* parameters){
                if(fn!=m_result||GetCurrentThreadId()!=m_gameThread||!controller||!parameters)return;
                const bool ours=m_pending&&m_pending->Controller.Get()==controller;
                try{
                    if(m_dispatchFrame.Opaque){
                        auto& dispatches=Dispatches(controller);
                        if(ours&&dispatches.Count!=1)m_pending->Receipt.Ambiguous=true;
                        if(dispatches.Count)--dispatches.Count;
                    }
                    if(!ours)return;
                    const auto ticket=m_resultFrame.Ticket(parameters);
                    const auto result=m_resultFrame.ResultValue->GetSignedIntPropertyValue(
                        m_resultFrame.Result->ContainerPtrToValuePtr<void>(parameters));
                    m_pending->Receipt.Complete(ticket,result);
                }catch(...){if(ours)m_pending->Receipt.Ambiguous=true;}
            },options);
        if(m_dispatchHook==Hook::ERROR_ID||m_resultHook==Hook::ERROR_ID)
            throw std::runtime_error("Native character save observation could not be registered");
    }

    template<class Guid>
    void Finish(const Guid& guid,Clock::time_point now){
        auto& save=*m_pending;
        if(!save.Receipt.Result&&!save.Receipt.Ambiguous&&now<save.Deadline&&save.Controller.Get())return;
        try{
            if(!save.Receipt.Confirmed()||!save.Controller.Get()||guid(save.Controller.Get())!=save.Character)
                throw std::runtime_error("Native character save was not confirmed");
            CacheBundleIO::MetadataLock lock(save.Journal);
            auto journal=ReadReceipts(save.Journal);
            if(!journal.Commit(save.Item->Key()))throw std::runtime_error("Held receipt changed");
            WriteReceipts(save.Journal,journal);
        }catch(const std::exception& error){Once(Diagnostics::Id::ItemDeliveryHeld,error.what(),*save.Item);}
        m_pending.reset();
    }

public:
    explicit Service(bool always):m_gameThread(GetCurrentThreadId()),m_always(always){}
    void Start(){
        try{Bind();}
        catch(const std::exception& error){m_broken=true;Once(Diagnostics::Id::ItemDeliveryUnavailable,error.what(),m_items.front());}
    }
    ~Service(){
        if(m_dispatchHook!=Hook::ERROR_ID)Hook::UnregisterCallback(m_dispatchHook);
        if(m_resultHook!=Hook::ERROR_ID)Hook::UnregisterCallback(m_resultHook);
    }
    void Add(std::vector<ID::Item> items){
        if(m_items.size()+items.size()>4096)throw std::runtime_error("Installation exceeds the item delivery limit");
        for(auto& item:items)m_items.push_back(std::move(item));
    }
    void ResetWorld(){m_world.Reset();m_joins.clear();m_pending.reset();m_next={};}
    template<class Guid,class Grant>
    void Tick(UObject* world,const Guid& guid,const Grant& grant){
        if(!world||m_broken||m_items.empty())return;
        const auto now=Clock::now();
        if(now<m_next)return;
        m_next=now+std::chrono::seconds(1);
        if(m_world.Get()!=world){
            ResetWorld();m_world.Assign(world);
            if(!m_world.Get())return;
            m_next=now+std::chrono::seconds(1);
        }
        try{
            if(m_pending){Finish(guid,now);if(m_pending)return;}
            if(!Network::OwnsGameplay(Network::Detect(world).Mode))return;
            auto* controllerType=AH::ResolveClass(TEXT("/Script/Dominion.DominionPlayerController"));
            auto* inventoryType=AH::ResolveClass(TEXT("/Script/Dominion.InventoryComponent"));
            auto* persistenceType=AH::ResolveClass(TEXT("/Script/Dominion.PersistenceSubsystem"));
            auto* itemType=AH::ResolveClass(TEXT("/Script/Dominion.ItemData"));
            if(!controllerType||!inventoryType||!persistenceType||!itemType)return;
            if(!m_save||GetCurrentThreadId()!=m_gameThread)return;
            std::erase_if(m_dispatches,[](const auto& entry){return !entry.second.Controller.Get();});

            TArray<UObject*> controllers;
            UECustom::UObjectGlobals::GetObjectsOfClass(controllerType,controllers,true,InvalidFlags);
            std::erase_if(m_joins,[&](const Join& join){
                auto* controller=join.Controller.Get();
                return !Live(controller)||controller->GetWorld()!=world;
            });
            std::map<std::string,UObject*> ready;
            for(auto* controller:controllers){
                if(!Live(controller)||controller->GetWorld()!=world
                    ||!CallBool(controller,TEXT("/Script/Engine.Actor:HasAuthority")))continue;
                auto* pawn=AH::GetObjectRef(controller,TEXT("Pawn"));
                auto* inventory=AH::GetObjectRef(controller,TEXT("InventoryComponent"));
                if(!Live(pawn)||pawn->GetWorld()!=world||!Live(inventory)
                    ||!inventory->IsA(inventoryType)||inventory->GetOuterPrivate()!=controller)continue;
                auto* loaded=CastField<FBoolProperty>(DragonWilds::PropertyHelper::GetPropertyByName(
                    inventory->GetClassPrivate(),TEXT("bHasLoadedFromSave")));
                if(!loaded||loaded->GetArrayDim()!=1||!loaded->GetPropertyValue(loaded->ContainerPtrToValuePtr<void>(inventory)))continue;
                const auto character=guid(controller);
                if(character.size()!=32||character.find_first_not_of("0123456789ABCDEF")!=character.npos)continue;
                const auto [it,inserted]=ready.emplace(character,controller);
                if(!inserted)it->second=nullptr;
            }
            for(const auto& [character,controller]:ready){
                if(!controller)continue;
                auto join=std::ranges::find_if(m_joins,[&](const Join& entry){
                    return entry.Character==character&&entry.Controller.Get()==controller;
                });
                if(join==m_joins.end()){
                    if(m_joins.size()>=256)return;
                    join=m_joins.insert(m_joins.end(),{WeakObjectHandle(controller),character,0});
                }
                if(!join->Controller.Get()||join->Next>=m_items.size())continue;
                auto* persistence=AH::GetObjectRef(controller,TEXT("PersistenceSubsystem"));
                if(!Live(persistence)||!persistence->IsA(persistenceType))continue;
                if(m_dispatchFrame.Opaque&&Dispatches(controller).Count)continue;
                const auto& item=m_items[join->Next++];
                try{
                    const auto journalPath=HostServices::ProgressDirectory()/"item-delivery"/(character+".json");
                    CacheBundleIO::MetadataLock lock(journalPath);
                    auto journal=ReadReceipts(journalPath);
                    const auto key=item.Key();
                    if(!journal.CanAttempt(key,m_always))continue;
                    auto* asset=AH::ResolveObject(RC::to_generic_string(item.Path));
                    if(!Live(asset)||!asset->IsA(itemType)||AssetMetadata::IsIncomplete(asset)
                        ||ID::Fold(RC::to_string(asset->GetPathName()))!=ID::Fold(item.Path)){
                        Once(Diagnostics::Id::ItemDeliveryUnresolved,"ItemData path could not be resolved",item);continue;
                    }
                    ID::SaveReceipt receipt;
                    if(m_dispatchFrame.Opaque)receipt.OpaqueSlot=CurrentSlot(controller);
                    AH::FunctionCall save(persistence,m_save);
                    save.Arg(m_saveArgument.c_str(),controller);
                    auto* pawn=AH::GetObjectRef(controller,TEXT("Pawn"));
                    auto* inventory=AH::GetObjectRef(controller,TEXT("InventoryComponent"));
                    grant(controller,item.Path,[&]{
                        if(controller->GetWorld()!=world||guid(controller)!=character
                            ||!Live(pawn)||AH::GetObjectRef(controller,TEXT("Pawn"))!=pawn
                            ||!Live(inventory)||AH::GetObjectRef(controller,TEXT("InventoryComponent"))!=inventory
                            ||!journal.Hold(key,m_always))throw std::runtime_error("Item delivery target or receipt changed");
                        WriteReceipts(journalPath,journal);
                    });
                    m_pending.emplace(PendingSave{WeakObjectHandle(controller),character,journalPath,&item,
                        now+std::chrono::seconds(15),std::move(receipt)});
                    m_requested=true;
                    try{save.Invoke();}catch(...){if(m_pending)m_pending->Receipt.Ambiguous=true;}
                    m_requested=false;
                    // A local save can complete before SavePlayerState returns.
                    if(m_pending&&!m_pending->Receipt.Dispatched)m_pending->Receipt.Ambiguous=true;
                    return;
                }catch(const PS::HelpyGrants::Rejected&){
                    // Capacity and preflight rejections leave the receipt eligible on the next join.
                }catch(const std::exception& error){Once(Diagnostics::Id::ItemDeliveryHeld,error.what(),item);}
            }
        }catch(const std::exception& error){Once(Diagnostics::Id::ItemDeliveryUnavailable,error.what(),m_items.front());}
    }
};
}
