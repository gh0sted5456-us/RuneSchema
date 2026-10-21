#pragma once
#include "SDK/Helper/NativeItemState.h"
#include "SDK/Helper/EquipmentDelivery.h"
#include "SDK/WeakObjectHandle.h"
#include "Runtime/PluginHost.h"
#include "Utility/NativeFunctionHook.h"
#include <Unreal/CoreUObject/UObject/FStrProperty.hpp>
#include <windows.h>
#include <atomic>
#include <optional>
#include <cstdio>
namespace PS {
class NativeDeleteObserver {
    using Object=RC::Unreal::UObject;using Function=RC::Unreal::UFunction;using Json=nlohmann::json;
    struct Hook {Function* FunctionPtr{};int Pre=0,Post=0;bool UI=false;};
    struct Capture {WeakObjectHandle Controller,World,Pawn;std::array<WeakObjectHandle,3> Inventories;std::string Character,Token;EquipmentDelivery::Guid Item{};};
    std::vector<Hook> Hooks;std::vector<std::optional<Capture>> Frames;
    std::function<std::string(Object*)> Identify;DWORD Thread=0;size_t Overflow=0;
    inline static std::atomic<uint64_t> Sequence=0;
    inline static thread_local Object* ScrapController=nullptr;
    inline static thread_local EquipmentDelivery::Guid ScrapIdentity{};
    static std::array<Object*,3> Inventories(Object* controller){
        namespace E=EquipmentDelivery;std::array<Object*,3> result{};size_t i=0;
        for(const auto* name:{TEXT("InventoryComponent"),TEXT("LoadoutComponent"),TEXT("PersonalInventoryComponent")}){
            const auto* type=i==1?TEXT("/Script/Dominion.LoadoutComponent"):i==2?TEXT("/Script/Dominion.PersonalInventoryComponent"):TEXT("/Script/Dominion.InventoryComponent");
            if(E::AH::GetObjectRef(controller,name))result[i]=E::Owned(controller,name,type);++i;
        }return result;
    }
    static Json Service(const Json& request){auto value=Json::parse(PluginHost::Call("RuneSchema.Core","helpy.inventory",request.dump()));
        if(value.contains("Error"))throw std::runtime_error(value.at("Error").get<std::string>());return value;}
    static std::string Guid(const EquipmentDelivery::Guid& value){std::string text;for(auto word:value){char part[9]{};std::snprintf(part,sizeof(part),"%08x",word);text+=part;}return text;}
    static Object* ObjectArg(Function* function,void* parameters,const TCHAR* name){
        namespace C=DragonWilds::ReflectedContract;using namespace RC::Unreal;
        auto* p=CastField<FObjectProperty>(function->FindProperty(FName(name,FNAME_Find)));
        if(!C::Input(p,function)||p->GetSize()!=sizeof(Object*))throw std::runtime_error("Native Delete object argument changed");
        return p->GetObjectPropertyValue(p->ContainerPtrToValuePtr<void>(parameters));
    }
    static int SlotArg(Function* function,void* parameters,const TCHAR* name){
        namespace C=DragonWilds::ReflectedContract;using namespace RC::Unreal;
        auto* p=CastField<FIntProperty>(function->FindProperty(FName(name,FNAME_Find)));
        if(!C::Input(p,function))throw std::runtime_error("Native Delete slot argument changed");return *p->ContainerPtrToValuePtr<int32_t>(parameters);
    }
    static Json Item(Object* item){
        namespace E=EquipmentDelivery;using namespace RC::Unreal;auto* data=E::Data(item);if(!data)throw std::runtime_error("Deleted item data unavailable");
        auto* p=CastField<FStrProperty>(DragonWilds::PropertyHelper::GetPropertyByName(data->GetClassPrivate(),TEXT("PersistenceID")));
        if(!E::C::Fits(p,data->GetClassPrivate()->GetPropertiesSize()))throw std::runtime_error("Deleted item persistence identity unavailable");
        const auto& chars=p->GetPropertyValue(p->ContainerPtrToValuePtr<void>(data)).GetCharArray();
        if(chars.Num()<2||chars.Num()>257||!chars.GetData()||chars.GetData()[chars.Num()-1])throw std::runtime_error("Invalid item persistence identity");
        const auto id=RC::to_string(RC::StringType(chars.GetData(),chars.Num()-1)),instance=Guid(E::Identity(item));
        E::Call count(item,TEXT("/Script/Dominion.Item:GetStackSize"),1);count.Invoke();const auto n=count.Integer();
        if(n<1||n>1000000)throw std::runtime_error("Deleted stack count unavailable");
        return {{"AssetPath",RC::to_string(data->GetPathName())},{"PersistenceID",id},{"InstanceGuid",instance},{"Count",n},
            {"NativeItem",{{"ItemData",id},{"GUID",instance},{"Count",n},{"HelpyState",NativeItemState::Read(item)}}}};
    }
    void Before(Function* function,bool ui,RC::Unreal::UnrealScriptFunctionCallableContext& context)noexcept{
        if(GetCurrentThreadId()!=Thread)return;if(Frames.size()>=16){++Overflow;return;}Frames.emplace_back();
        try{
            using namespace RC::Unreal;namespace E=EquipmentDelivery;auto* parameters=context.TheStack.Locals();auto* source=context.Context;
            if(!source||!parameters)return;
            auto* controller=ui?E::AH::GetObjectRef(source,TEXT("PlayerController")):source->GetOuterPrivate();
            if(!controller)return;const auto character=Identify(controller);if(character.empty())return;
            auto* inventory=ObjectArg(function,parameters,ui?TEXT("SlotInventory"):TEXT("Inventory"));
            const auto inventories=Inventories(controller);
            if(!inventory||std::find(inventories.begin(),inventories.end(),inventory)==inventories.end())throw std::runtime_error("Recovery supports only the current character's inventories");
            if(!ui&&E::Owned(controller,TEXT("InventoryController"),TEXT("/Script/Dominion.InventoryController"))!=source)throw std::runtime_error("Native Delete controller ownership changed");
            const auto slots=E::Slots(inventory);const int slot=SlotArg(function,parameters,ui?TEXT("SlotNum"):TEXT("SlotIndex"));
            if(slot<0||size_t(slot)>=slots.size()||!slots[slot])return;auto* item=slots[slot];const auto instance=E::Identity(item);
            if(controller==ScrapController&&instance==ScrapIdentity)return;
            for(const auto& active:Frames)if(active&&active->Controller.Get()==controller&&active->Item==instance)return;
            const auto token=std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64())+"-"+std::to_string(++Sequence);
            Capture capture;capture.Controller=WeakObjectHandle(controller);capture.World=WeakObjectHandle(controller->GetWorld());
            capture.Pawn=WeakObjectHandle(E::AH::GetObjectRef(controller,TEXT("Pawn")));capture.Character=character;capture.Item=instance;
            for(size_t i=0;i<inventories.size();++i)capture.Inventories[i]=WeakObjectHandle(inventories[i]);Frames.back()=std::move(capture);
            const auto result=Service({{"Action","PrepareDelete"},{"CharacterGuid",character},{"CaptureId",token},{"Item",Item(item)}});
            if(!result.value("Captured",false))throw std::runtime_error("Helpy did not commit a recovery copy");
            Frames.back()->Token=token;
        }catch(const std::exception& error){Log<RC::LogLevel::Warning>(STR("[HELPY][STORAGE][NOT-CAPTURED] No recovery copy: {}. Native Delete proceeds.\n"),RC::to_generic_string(error.what()));}
        catch(...){Log<RC::LogLevel::Warning>(STR("[HELPY][STORAGE][NOT-CAPTURED] Recovery capture failed. Native Delete proceeds.\n"));}
    }
    void After()noexcept{
        if(GetCurrentThreadId()!=Thread)return;if(Overflow){--Overflow;return;}if(Frames.empty())return;
        auto capture=std::move(Frames.back());Frames.pop_back();if(!capture||capture->Token.empty())return;
        std::string outcome="Unknown";try{
            namespace E=EquipmentDelivery;auto* controller=capture->Controller.Get();
            if(controller&&capture->World.Get()==controller->GetWorld()&&Identify(controller)==capture->Character
                &&capture->Pawn.Get()==E::AH::GetObjectRef(controller,TEXT("Pawn"))){
                const auto inventories=Inventories(controller);bool present=false;
                for(size_t i=0;i<inventories.size();++i){
                    if(inventories[i]!=capture->Inventories[i].Get())throw std::runtime_error("Inventory changed during native Delete");
                    if(inventories[i])for(auto* item:E::Slots(inventories[i]))if(item&&E::Identity(item)==capture->Item)present=true;
                }outcome=present?"Retained":"Removed";
            }
        }catch(...){}
        try{Service({{"Action","ObserveDelete"},{"CharacterGuid",capture->Character},{"CaptureId",capture->Token},{"Result",outcome}});}
        catch(...){Log<RC::LogLevel::Warning>(STR("[HELPY][STORAGE][UNCONFIRMED] Delete proceeded, but its recovery receipt could not be committed. No automatic restore.\n"));}
    }
public:
    struct ScrapScope {
        ScrapScope(Object* controller,const EquipmentDelivery::Guid& item){if(ScrapController)throw std::runtime_error("A Scrap deletion is already active");ScrapController=controller;ScrapIdentity=item;}
        ~ScrapScope(){ScrapController=nullptr;ScrapIdentity={};}
        ScrapScope(const ScrapScope&)=delete;ScrapScope& operator=(const ScrapScope&)=delete;
    };
    explicit NativeDeleteObserver(std::function<std::string(Object*)> identify):Identify(std::move(identify)),Thread(GetCurrentThreadId()){}
    void Install()noexcept{try{
        using namespace RC::Unreal;
        if(!Hooks.empty())return;Frames.reserve(16);Hooks.reserve(3);
        for(const auto* path:{TEXT("/Script/Dominion.InventoryUIAPI:HandleDestroyItem"),TEXT("/Script/Dominion.InventoryController:DestroyItemFromInventory"),TEXT("/Script/Dominion.InventoryController:Server_DestroyItemFromInventory")}){
            auto* function=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path,false);if(!CanHookNative(function))continue;
            const bool ui=function->GetName()==TEXT("HandleDestroyItem");
            Hook hook{function};try{
                hook.Pre=RegisterNativePreHook(function,[this,function,ui](auto& context,void*){Before(function,ui,context);});
                hook.Post=RegisterNativePostHook(function,[this](auto&,void*){After();});
                if(hook.Pre&&hook.Post){Hooks.push_back(hook);continue;}
            }catch(...){}
            if(hook.Pre)function->UnregisterHook(hook.Pre);if(hook.Post)function->UnregisterHook(hook.Post);
        }
        Log<RC::LogLevel::Verbose>(STR("[HELPY][STORAGE] Installed {} observational Delete boundaries; native deletion is never canceled.\n"),Hooks.size());
    }catch(...){}}
    ~NativeDeleteObserver(){for(const auto& hook:Hooks)try{hook.FunctionPtr->UnregisterHook(hook.Pre);hook.FunctionPtr->UnregisterHook(hook.Post);}catch(...){}}
    void Reset()noexcept{Frames.clear();Overflow=0;}
};
}
