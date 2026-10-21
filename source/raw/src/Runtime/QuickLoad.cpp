#include "Runtime/QuickLoad.h"
#include "Core/QuickLoadSettings.h"
#include "Core/QuickLaunchConfiguration.h"
#include "Core/ConfigFiles.h"
#include "Runtime/HostServices.h"
#include "Runtime/Storefront.h"
#include "Runtime/NativeExecutableDiscovery.h"
#include "Runtime/NativePersistenceRecovery.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/ReflectedContract.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/WeakObjectHandle.h"
#include "Utility/Logging.h"
#include <Unreal/Hooks/Hooks.hpp>
#include <Unreal/FWorldContext.hpp>
#include <Unreal/World.hpp>
#include <Unreal/UFunctionStructs.hpp>
#include <Unreal/CoreUObject/UObject/FStrProperty.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Helpers/String.hpp>
#include <safetyhook.hpp>
#include <mutex>
#include <chrono>
#include <set>
#include <atomic>
#include <TlHelp32.h>
namespace PS::QuickLoad {
using namespace RC;using namespace RC::Unreal;using Json=nlohmann::json;
namespace AH=DragonWilds::ActorHelper;namespace C=DragonWilds::ReflectedContract;
namespace {
std::mutex mutex;QuickLoadSettings::Config config;std::string status="Vanilla Load selected.";
Json characters=Json::array(),worlds=Json::array();bool refresh=true,requested=false,finished=false,shift=false,early=false;
uint64_t started=0,boot=GetTickCount64();WeakObjectHandle menuWorld,currentController;SafetyHookInline loadingHook;void* selectCharacter=nullptr;
Hook::GlobalCallbackId mapHook=Hook::ERROR_ID,tickHook=Hook::ERROR_ID,possessHook=Hook::ERROR_ID;
void SetStatus(const std::string& value){std::lock_guard lock(mutex);status=value;PS::Log<LogLevel::Normal>(STR("[QUICK-LOAD] {}\n"),PS::ToWideSafe(value));}
bool StandaloneEnabledOrLoaded(){
    const auto mods=HostServices::WorkingDirectory()/"Mods";std::error_code error;
    if(std::filesystem::exists(mods/"QuickLoad"/"enabled.txt",error)||error)return true;
    const auto list=mods/"mods.txt";
    if(std::filesystem::exists(list)&&QuickLoadSettings::StandaloneEntryEnabled(ConfigFiles::Read(list,1024*1024)))return true;
    const auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,GetCurrentProcessId());
    if(snapshot==INVALID_HANDLE_VALUE)return true;
    MODULEENTRY32W module{};module.dwSize=sizeof(module);bool conflict=false;
    if(!Module32FirstW(snapshot,&module))conflict=true;
    else do{if(Storefront::Lower(std::wstring(module.szExePath)).find(L"\\quickload\\")!=std::wstring::npos){conflict=true;break;}}while(Module32NextW(snapshot,&module));
    CloseHandle(snapshot);return conflict;
}
UObject* Find(const TCHAR* path){return UECustom::UObjectGlobals::StaticFindObject<UObject*>(nullptr,nullptr,path,false);}
struct Frame {
    UObject* Self;UFunction* Fn;std::vector<uint8_t> Bytes;std::vector<FProperty*> Fields;
    Frame(UObject* self,const TCHAR* path):Self(self),Fn(static_cast<UFunction*>(Find(path))){
        if(!self||!Fn||!Fn->IsA<UFunction>())throw std::runtime_error("Quick Load native function unavailable");C::Frame(Fn);
        auto* owner=Fn->GetOuterPrivate();if(!owner||!owner->IsA<UClass>()||!self->IsA(static_cast<UClass*>(owner))||!Fn->HasAnyFunctionFlags(FUNC_Native)||Fn->HasAnyFunctionFlags(FUNC_Net))throw std::runtime_error("Quick Load native owner/flags changed");
        Bytes.resize(std::max(1,int(Fn->GetParmsSize())));
        try{for(auto* p:TFieldRange<FProperty>(Fn,EFieldIterationFlags::Default))if(p->HasAnyPropertyFlags(CPF_Parm)){if(!C::Fits(p,Fn->GetParmsSize()))throw std::runtime_error("Quick Load parameter extent changed");p->InitializeValue_InContainer(Bytes.data());Fields.push_back(p);}}catch(...){Destroy();throw;}
    }
    ~Frame(){Destroy();}void Destroy(){for(auto it=Fields.rbegin();it!=Fields.rend();++it)(*it)->DestroyValue_InContainer(Bytes.data());Fields.clear();}
    void Invoke(){Self->ProcessEvent(Fn,Bytes.data());}
    FProperty* Return(){auto* p=Fn->GetReturnProperty();if(!C::Output(p,Fn))throw std::runtime_error("Quick Load return layout changed");return p;}
};
FProperty* Member(UObject* object,const TCHAR* key){auto* p=object?object->GetClassPrivate()->GetPropertyByNameInChain(key):nullptr;if(!object||!C::Fits(p,object->GetClassPrivate()->GetPropertiesSize())||p->GetArrayDim()!=1)throw std::runtime_error("Quick Load native member unavailable");return p;}
std::string Text(UStruct* type,void* record,const TCHAR* key){
    auto* field=type?type->GetPropertyByNameInChain(key):nullptr;auto* p=CastField<FStrProperty>(field);
    if(!p||!C::Fits(p,type->GetPropertiesSize()))return {};const auto* value=p->ContainerPtrToValuePtr<FString>(record);
    if(value->Len()<0||value->Len()>1024)return {};return RC::to_string(RC::StringType(**value));
}
UObject* Controller(UObject* world){
    auto* library=Find(TEXT("/Script/Engine.Default__GameplayStatics"));AH::FunctionCall call(library,TEXT("/Script/Engine.GameplayStatics:GetPlayerController"));
    call.Arg(TEXT("WorldContextObject"),world).Arg(TEXT("PlayerIndex"),int32_t(0)).Invoke();auto* controller=call.Result<UObject*>();
    if(controller){AH::FunctionCall local(controller,TEXT("/Script/Engine.Controller:IsLocalController"));local.Invoke();if(!local.Result<bool>())return nullptr;}
    return controller;
}
UObject* Spud(UObject* controller){AH::FunctionCall call(Find(TEXT("/Script/Engine.Default__SubsystemBlueprintLibrary")),TEXT("/Script/Engine.SubsystemBlueprintLibrary:GetGameInstanceSubsystem"));
    call.Arg(TEXT("ContextObject"),controller).Arg(TEXT("Class"),static_cast<UClass*>(Find(TEXT("/Script/SPUD.SpudSubsystem")))).Invoke();return call.Result<UObject*>();}
void WorldSlot(UObject* controller,const std::string& name){
    auto* subsystem=AH::GetObjectRef(controller,TEXT("PersistenceSubsystem"));auto* settings=CastField<FStructProperty>(Member(subsystem,TEXT("WorldSaveSettings")));
    auto* type=settings?settings->GetStruct().Get():nullptr;auto* slot=type?CastField<FStrProperty>(type->GetPropertyByNameInChain(TEXT("WorldSlotName"))):nullptr;
    if(!settings||!type||!slot||!C::Fits(slot,settings->GetElementSize()))throw std::runtime_error("Quick Load native world-slot layout changed");
    *slot->ContainerPtrToValuePtr<FString>(settings->ContainerPtrToValuePtr<void>(subsystem))=FString(RC::to_wstring(name).c_str());
}
void Fade(UObject* controller,float amount){auto* camera=AH::GetObjectRef(controller,TEXT("PlayerCameraManager"));AH::FunctionCall call(camera,TEXT("/Script/Engine.PlayerCameraManager:SetManualCameraFade"));
    call.Arg(TEXT("InFadeAmount"),amount).JsonArg(TEXT("Color"),{{"R",0.f},{"G",0.f},{"B",0.f},{"A",1.f}}).Arg(TEXT("bInFadeAudio"),false).Invoke();}
void Finish(bool restore,const std::string& message,UObject* controller=nullptr){
    requested=false;finished=true;if(restore)loadingHook.reset();
    if(!controller)controller=currentController.Get();
    if(controller)try{Fade(controller,0.f);}catch(...){}
    SetStatus(message);
}
void Character(UObject* controller,const std::string& name,bool choose){
    Frame frame(AH::GetObjectRef(controller,TEXT("PersistenceSubsystem")),TEXT("/Script/Dominion.PersistenceSubsystem:GetPlayerCharacterSaveSlots"));
    if(frame.Fields.size()!=1)throw std::runtime_error("Character-slot provider parameters changed");frame.Invoke();
    auto* array=CastField<FArrayProperty>(frame.Return());auto* inner=array?CastField<FStructProperty>(array->GetInner()):nullptr;auto* type=inner?inner->GetStruct().Get():nullptr;
    if(!array||!inner||!type||inner->GetElementSize()!=type->GetPropertiesSize())throw std::runtime_error("Character-slot provider result changed");
    auto* values=array->ContainerPtrToValuePtr<FScriptArray>(frame.Bytes.data());if(!values||values->Num()<0||values->Num()>1024||(values->Num()&&!values->GetData()))throw std::runtime_error("Character-slot provider exceeds bound");
    Json rows=Json::array();void* selected=nullptr;size_t matches=0;
    for(int i=0;i<values->Num();++i){auto* row=static_cast<uint8_t*>(values->GetData())+size_t(i)*inner->GetElementSize();auto display=Text(type,row,TEXT("DisplaySlotName"));if(!QuickLoadSettings::Name(display))continue;
        rows.push_back({{"Name",display}});if(_wcsicmp(RC::to_wstring(display).c_str(),RC::to_wstring(name).c_str())==0){selected=row;++matches;}}
    {std::lock_guard lock(mutex);characters=rows;}
    if(choose){if(matches!=1||!selected||!selectCharacter)throw std::runtime_error("Character name is missing/ambiguous or native selector is unavailable");
        using Select=void(__fastcall*)(UObject*,const void*);reinterpret_cast<Select>(selectCharacter)(controller,selected);}
}
void Worlds(UObject* controller){
    Frame frame(Spud(controller),TEXT("/Script/SPUD.SpudSubsystem:GetSaveGameList"));
    for(auto* field:frame.Fields)if(!field->HasAnyPropertyFlags(CPF_ReturnParm)){
        if(!C::Input(field,frame.Fn))throw std::runtime_error("World provider has unsupported outputs");const auto name=field->GetName();
        if(name==TEXT("bIncludeQuickSave")||name==TEXT("bIncludeAutoSave")){auto* p=CastField<FBoolProperty>(field);if(!p)throw std::runtime_error("World provider flag changed");p->SetPropertyValue(p->ContainerPtrToValuePtr<void>(frame.Bytes.data()),true);}
        else if(name!=TEXT("Sorting")&&name!=TEXT("UserIndex"))throw std::runtime_error("World provider input changed");
    }frame.Invoke();auto* array=CastField<FArrayProperty>(frame.Return());auto* inner=array?CastField<FObjectPropertyBase>(array->GetInner()):nullptr;
    if(!inner||inner->GetElementSize()!=sizeof(UObject*))throw std::runtime_error("World-slot provider result changed");auto* values=array->ContainerPtrToValuePtr<FScriptArray>(frame.Bytes.data());
    if(!values||values->Num()<0||values->Num()>1024||(values->Num()&&!values->GetData()))throw std::runtime_error("World-slot provider exceeds bound");
    Json rows=Json::array();std::set<std::string> seen;
    for(int i=0;i<values->Num();++i){auto* info=inner->GetObjectPropertyValue(static_cast<uint8_t*>(values->GetData())+size_t(i)*inner->GetElementSize());if(!info)continue;
        auto name=Text(info->GetClassPrivate(),info,TEXT("SlotName"));if(QuickLoadSettings::Name(name)&&seen.insert(name).second)rows.push_back({{"Name",name}});}
    std::lock_guard lock(mutex);worlds=rows;
}
void Load(UObject* controller){
    if(shift||GetAsyncKeyState(VK_SHIFT)&0x8000){Finish(true,"Cancelled with Shift; normal menu restored.",controller);return;}
    if(finished||requested)return;QuickLoadSettings::Config selected;{std::lock_guard lock(mutex);selected=config;}
    if(!selected.Enabled)return;
    Character(controller,selected.CharacterName,false);
    // Verify loading and camera contracts before changing native selection.
    AH::FunctionCall load(Spud(controller),TEXT("/Script/SPUD.SpudSubsystem:LoadGame"));
    load.StringArg(TEXT("SlotName"),selected.WorldName).Arg(TEXT("bClientLoadingListen"),false);
    Fade(controller,0.f);NativePersistenceRecovery::EnsureSessionPolicy(controller);
    Character(controller,selected.CharacterName,true);WorldSlot(controller,selected.WorldName);
    requested=true;started=GetTickCount64();menuWorld.Assign(controller->GetWorld());
    load.Invoke();Fade(controller,1.f);SetStatus("Native load requested; waiting for character possession. Save acceptance remains game-owned.");
}
void SuppressLoadingScreen(void*,const void*){}
}
void InitializeEarly(){
    if(early)return;early=true;if(Storefront::IsDedicatedServer())return;
    try{
        const auto selected=QuickLaunchConfiguration::Read(HostServices::SettingsDirectory());
        if(!selected.value("Ok",false))throw std::runtime_error(selected.value("Status",std::string("Launch settings unavailable")));
        const auto& document=selected.at("Document");config=QuickLoadSettings::Normalize({false,document.at("CharacterName").get<std::string>(),document.at("WorldName").get<std::string>(),false,document.at("Mode").get<std::string>()});
        if(!config.Enabled){SetStatus(config.Mode=="lessRestrictive"?"Less Restrictive selected; session tolerant policy is requested when a native world context is available.":"Vanilla Load selected; native strictness is unchanged.");return;}if(!QuickLoadSettings::Name(config.CharacterName)||!QuickLoadSettings::Name(config.WorldName))throw std::runtime_error("Quick Load needs a character and world");
        shift=(GetAsyncKeyState(VK_SHIFT)&0x8001)!=0;if(shift){Finish(true,"Skipped at startup with Shift.");return;}
        if(StandaloneEnabledOrLoaded())throw std::runtime_error("Standalone QuickLoad is enabled or loaded; disable it before enabling integrated Quick Launch");
        if(!Storefront::AllowsEmbeddedAobSignatures())throw std::runtime_error("Quick Launch requires a verified Steam/Epic or Game Pass executable");
        const bool gp=Storefront::Current()==Storefront::Kind::GamePass;
        selectCharacter=NativeExecutableDiscovery::Resolve(gp?"48 89 5C 24 10 56 48 83 EC 20 48 8B D9 48 81 C1 50 08 00 00 E8 ?? ?? ?? ?? 48 8B 03 48 8B CB FF 90 E0 07 00 00 84 C0 74 ?? 48 83 BB 18 08 00 00 00 74 ?? 48 8B 03 48 8B CB 48 89 7C 24 30 48 8B BB 10 08 00 00 FF 90 E0 07 00 00 84 C0 74 ?? 48 8D 93 50 08 00 00 48 8D 4F 30 E8 ?? ?? ?? ?? 48 8D 57 30 48 8B CF E8 ?? ?? ?? ??":"48 89 5C 24 08 57 48 83 EC 20 48 8B F9 48 8B DA 48 8B CA E8 ?? ?? ?? ?? 48 8D 8F 50 08 00 00 48 8B D3 E8 ?? ?? ?? ?? 48 8B 07 48 8B CF FF 90 E0 07 00 00 84 C0 74 19 48 83 BF 18 08 00 00 00 74 0F 48 8B 8F 10 08 00 00 48 8B D7 E8 ?? ?? ?? ?? 48 8B 5C 24 30 48 83 C4 20 5F C3");
        auto* screen=NativeExecutableDiscovery::Resolve(gp?"4C 8B DC 55 53 49 8D AB ?? ?? ?? ?? 48 81 EC ?? ?? ?? ?? 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 ?? ?? ?? ?? 8B 5A 18 49 89 73 08 48 8B F2 49 89 7B 18 48 8B 7A 10":"4C 8B DC 55 49 8D AB ?? ?? ?? ?? 48 81 EC ?? ?? ?? ?? 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 ?? ?? ?? ?? 49 89 5B 08 48 8B 5A 10");
        if(!selectCharacter||!screen)throw std::runtime_error("Unique native QuickLoad signatures unavailable for this game build");
        loadingHook=safetyhook::create_inline(screen,reinterpret_cast<void*>(&SuppressLoadingScreen));if(!loadingHook)throw std::runtime_error("Loading-screen hook unavailable");SetStatus("Armed for native menu load. Hold Shift to cancel.");
    }catch(const std::exception& e){Finish(true,std::string("Not armed: ")+e.what());}
}
void Initialize(){
    Hook::FCallbackOptions options{};options.OwnerModName=TEXT("RuneSchema");options.HookName=TEXT("NativeLoadSessionPolicy");
    mapHook=Hook::RegisterLoadMapPostCallback([](auto&,auto*,FWorldContext& context,auto&&...){
        UObject* controller=nullptr;
        try{auto* world=context.GetThisCurrentWorld().Get();NativePersistenceRecovery::EnsureSessionPolicy(world);if(Storefront::IsDedicatedServer())return;controller=world?Controller(world):nullptr;if(!controller)return;currentController.Assign(controller);
        auto* type=Find(TEXT("/Script/Dominion.DominionPlayerControllerMenu"));if(type&&type->IsA<UClass>()&&controller->IsA(static_cast<UClass*>(type))){
            if(requested){Finish(true,"World load returned to the menu; normal loading restored.",controller);return;}
            if(!finished)Load(controller);
        }else if(requested)Fade(controller,1.f);}catch(const std::exception& e){Finish(true,std::string("Native load stopped: ")+e.what(),controller);}
    },options);
    if(Storefront::IsDedicatedServer()){
        if(mapHook==Hook::ERROR_ID)PS::Log<LogLevel::Warning>(STR("[SAVE-ENTRY][SESSION-POLICY][HOOK-UNAVAILABLE] Server world startup is unchanged; tolerant policy will be retried at character loading.\n"));
        return;
    }
    options.HookName=TEXT("QuickLoadPossession");possessHook=Hook::RegisterProcessEventPostCallback([](auto&,UObject*,UFunction* fn,void* params){
        if(!requested||!fn||!params||fn->GetPathName()!=TEXT("/Script/Dominion.SkillPerkComponent:HandlePossessedPawnChanged"))return;
        try{auto* p=CastField<FObjectPropertyBase>(fn->FindProperty(FName(TEXT("NewPawn"),FNAME_Find)));if(!C::Input(p,fn))return;auto* pawn=p->GetObjectPropertyValue(p->ContainerPtrToValuePtr<void>(params));if(!pawn||pawn->GetWorld()==menuWorld.Get())return;
            auto* controller=AH::GetObjectRef(pawn,TEXT("Controller"));if(!controller||Controller(pawn->GetWorld())!=controller||AH::GetObjectRef(controller,TEXT("Pawn"))!=pawn)return;
            WorldSlot(controller,config.WorldName);Finish(false,"Native character possession observed; camera fade cleared.",controller);
        }catch(const std::exception& e){Finish(true,std::string("Possession handoff failed: ")+e.what());}
    },options);
    options.HookName=TEXT("QuickLoadStatus");tickHook=Hook::RegisterEngineTickPostCallback([](auto&,auto*,float,bool){
        shift=shift||((GetAsyncKeyState(VK_SHIFT)&0x8001)!=0);
        if(shift&&loadingHook&&!finished){Finish(true,requested?"Shift restored the normal loading screen; the game's already requested load is not cancelled.":"Cancelled with Shift; normal startup restored.");return;}
        if(requested&&GetTickCount64()-started>120000){Finish(true,"Native load timed out; loading screen restored. No retry attempted.");return;}
        if(loadingHook&&!requested&&!finished&&GetTickCount64()-boot>60000){Finish(true,"Frontend startup timed out; normal loading screen restored.");return;}
        bool read=false;{std::lock_guard lock(mutex);read=refresh;refresh=false;}if(!read)return;
        try{auto* type=static_cast<UClass*>(Find(TEXT("/Script/Dominion.DominionPlayerControllerMenu")));if(!type)return;TArray<UObject*> objects;UECustom::UObjectGlobals::GetObjectsOfClass(type,objects,true);if(objects.Num()>16)return;
            for(auto* object:objects)if(object&&!object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed))&&object->GetWorld()&&Controller(object->GetWorld())==object){Character(object,"",false);Worlds(object);break;}
        }catch(const std::exception& e){SetStatus(std::string("Native save choices unavailable: ")+e.what());}
    },options);
    if(mapHook==Hook::ERROR_ID||tickHook==Hook::ERROR_ID||possessHook==Hook::ERROR_ID)Finish(true,"Native lifecycle callbacks unavailable; automatic loading disabled.");
}
void Shutdown()noexcept{NativePersistenceRecovery::ReleaseSessionPolicy(currentController.Get());loadingHook.reset();for(auto id:{mapHook,tickHook,possessHook})if(id!=Hook::ERROR_ID)Hook::UnregisterCallback(id);mapHook=tickHook=possessHook=Hook::ERROR_ID;}
Json Read(){std::lock_guard lock(mutex);refresh=true;auto result=QuickLaunchConfiguration::Read(HostServices::SettingsDirectory());
    if(!result.value("Ok",false))return result;
    result["Characters"]=characters;result["Worlds"]=worlds;result["Status"]=status;
    result["DedicatedServer"]=Storefront::IsDedicatedServer();result["Storefront"]=Storefront::Name(Storefront::Current());return result;}
Json Save(const Json& request){
    if(Storefront::IsDedicatedServer())return {{"Ok",false},{"Status","Character/world launch options are only available on game clients."}};
    std::lock_guard lock(mutex);auto result=QuickLaunchConfiguration::Save(HostServices::SettingsDirectory(),request);
    if(result.value("Ok",false)){result["Characters"]=characters;result["Worlds"]=worlds;}
    return result;
}
}
