#include "Runtime/ModLocalServices.h"
#include "Runtime/MultiplayerRuntime.h"
#include "Core/ModDataRelay.h"
#include "Core/ModCompute.h"
#include "Core/CookedPakRegistryManifest.h"
#include "SDK/Helper/LocalServiceFrame.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/WeakObjectHandle.h"
#include "Runtime/Storefront.h"
#include "Utility/Logging.h"
#include "Utility/StructuredDiagnosticB3D.h"
#include "Unreal/Hooks.hpp"
#include "Unreal/World.hpp"
#include "Unreal/ULocalPlayer.hpp"
#include "Unreal/UnrealInitializer.hpp"
#include <shlobj.h>
#include <unordered_set>
#pragma comment(lib,"shell32.lib")
#pragma comment(lib,"ole32.lib")

namespace PS::ModLocalServices {
namespace {
using Json=nlohmann::json;using Definition=CookedPakRegistryManifest::Manifest::LocalService;
namespace AH=DragonWilds::ActorHelper;
const std::map<std::wstring,std::vector<Pin>>& Contracts(){
    using T=Type;static const std::map<std::wstring,std::vector<Pin>> value={
        {L"RuneSchemaTemplateList",{{L"Success",T::Bool,true},{L"Templates",T::Strings,true},{L"Labels",T::Strings,true},{L"Error",T::String,true}}},
        {L"RuneSchemaTemplateRead",{{L"TemplateId",T::String},{L"Success",T::Bool,true},{L"Document",T::Int64,true},{L"Error",T::String,true}}},
        {L"RuneSchemaJsonCreate",{{L"Success",T::Bool,true},{L"Document",T::Int64,true},{L"Error",T::String,true}}},
        {L"RuneSchemaJsonRead",{{L"Document",T::Int64},{L"Pointer",T::String},{L"Success",T::Bool,true},{L"Kind",T::Byte,true},{L"Number",T::Double,true},{L"Text",T::String,true},{L"Boolean",T::Bool,true},{L"Count",T::Int32,true},{L"Error",T::String,true}}},
        {L"RuneSchemaJsonSet",{{L"Document",T::Int64},{L"Pointer",T::String},{L"Kind",T::Byte},{L"Number",T::Double},{L"Text",T::String},{L"Boolean",T::Bool},{L"Success",T::Bool,true},{L"Error",T::String,true}}},
        {L"RuneSchemaJsonKeys",{{L"Document",T::Int64},{L"Pointer",T::String},{L"Success",T::Bool,true},{L"Keys",T::Strings,true},{L"Error",T::String,true}}},
        {L"RuneSchemaJsonReadNumbers",{{L"Document",T::Int64},{L"Pointer",T::String},{L"Success",T::Bool,true},{L"Values",T::Numbers,true},{L"Error",T::String,true}}},
        {L"RuneSchemaJsonSetNumbers",{{L"Document",T::Int64},{L"Pointer",T::String},{L"Values",T::Numbers},{L"Success",T::Bool,true},{L"Error",T::String,true}}},
        {L"RuneSchemaJsonReadStrings",{{L"Document",T::Int64},{L"Pointer",T::String},{L"Success",T::Bool,true},{L"Values",T::Strings,true},{L"Error",T::String,true}}},
        {L"RuneSchemaJsonSetStrings",{{L"Document",T::Int64},{L"Pointer",T::String},{L"Values",T::Strings},{L"Success",T::Bool,true},{L"Error",T::String,true}}},
        {L"RuneSchemaTemplateCreate",{{L"Document",T::Int64},{L"Success",T::Bool,true},{L"TemplateId",T::String,true},{L"Error",T::String,true}}},
        {L"RuneSchemaDocumentClose",{{L"Document",T::Int64},{L"Success",T::Bool,true},{L"Error",T::String,true}}},
        {L"RuneSchemaCompute",{{L"Resource",T::String},{L"Weights",T::Numbers},{L"Generation",T::Int64},{L"Accepted",T::Bool,true},{L"Ticket",T::Int64,true},{L"Error",T::String,true}}},
        {L"RuneSchemaComputePoll",{{L"Ticket",T::Int64},{L"Ready",T::Bool,true},{L"Success",T::Bool,true},{L"Generation",T::Int64,true},{L"Values",T::Numbers,true},{L"Error",T::String,true}}}
    };return value;
}
bool Usable(UObject* object){return object&&!object->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed|RF_NeedLoad|RF_NeedPostLoad));}
void Warning(const std::string& detail){if(!StructuredDiagnosticB3D::Report("RS-MOD-001","Warning","Rejected","Mod-local service unavailable",detail,"ModLocalServices","Application"))Log<RC::LogLevel::Warning>(TEXT("[RS-MOD-001] {}\n"),ToWideSafe(detail));}
struct Session {WeakObjectHandle Source,Controller,World;std::string Owner;std::map<int64_t,Json> Documents;};
struct State {
    ModCompute::Broker Compute;std::map<std::string,RuneSchemaModApiV1> Apis;
    std::unordered_map<void*,std::string> NetworkOwners;
    std::map<std::string,Definition> Routes;std::unordered_set<FName> ServiceNames;std::map<uint64_t,Session> Sessions;int64_t NextDocument=0;
    Hook::GlobalCallbackId Script=Hook::ERROR_ID,Tick=Hook::ERROR_ID;
    static uint64_t Key(const WeakObjectHandle& value){return (uint64_t(uint32_t(value.ObjectIndex))<<32)|uint32_t(value.ObjectSerialNumber);}
    void Prune(){for(auto it=Sessions.begin();it!=Sessions.end();)if(!Usable(it->second.Source.Get())||!Usable(it->second.Controller.Get())||!Usable(it->second.World.Get())||it->second.Source.Get()->GetWorld()!=it->second.World.Get()){Compute.Cancel(it->first);it=Sessions.erase(it);}else ++it;}
    Session& Scope(UObject* source,const Definition& route){
        if(!IsInGameThread()||Storefront::IsDedicatedServer()||!Usable(source))throw std::runtime_error("Mod-local services require a live local component on the game thread");
        auto* componentType=AH::ResolveClass(TEXT("/Script/Engine.ActorComponent")),*controllerType=AH::ResolveClass(TEXT("/Script/Engine.PlayerController"));
        if(!componentType||!controllerType||!source->IsA(componentType))throw std::runtime_error("Service object is not an ActorComponent");
        AH::FunctionCall getOwner(source,TEXT("/Script/Engine.ActorComponent:GetOwner"));getOwner.Invoke();auto* controller=getOwner.Result<UObject*>();
        if(!Usable(controller)||!controller->IsA(controllerType)||source->GetOuterPrivate()!=controller)throw std::runtime_error("Service component must belong to its actual PlayerController");
        AH::FunctionCall local(controller,TEXT("/Script/Engine.Controller:IsLocalController"));local.Invoke();
        auto* playerField=CastField<FObjectPropertyBase>(controller->GetPropertyByNameInChain(TEXT("Player")));
        auto* localType=AH::ResolveClass(TEXT("/Script/Engine.LocalPlayer"));
        if(!local.Result<bool>()||!localType||!DragonWilds::ReflectedContract::Fits(playerField,controller->GetClassPrivate()->GetPropertiesSize())||playerField->GetElementSize()!=sizeof(UObject*))throw std::runtime_error("Actual local player is unavailable");
        auto* player=AH::GetObjectRef(controller,TEXT("Player"));auto* world=controller->GetWorld();
        if(!Usable(player)||!player->IsA(localType)||!Usable(world)||source->GetWorld()!=world)throw std::runtime_error("Local service world identity changed");
        WeakObjectHandle identity(source),controllerIdentity(controller),worldIdentity(world);
        if(identity.Get()!=source||controllerIdentity.Get()!=controller||worldIdentity.Get()!=world)throw std::runtime_error("Local service weak identities are unavailable");
        Prune();const auto key=Key(identity);auto found=Sessions.find(key);
        if(found!=Sessions.end()&&(found->second.Controller.Get()!=controller||found->second.World.Get()!=world||found->second.Owner!=route.Owner)){Compute.Cancel(key);Sessions.erase(found);found=Sessions.end();}
        if(found==Sessions.end()){if(Sessions.size()>=256)throw std::runtime_error("Too many local service components");found=Sessions.emplace(key,Session{identity,controllerIdentity,worldIdentity,route.Owner,{}}).first;}
        return found->second;
    }
    Json& Document(Session& session,int64_t handle){const auto found=session.Documents.find(handle);if(handle<=0||found==session.Documents.end())throw std::runtime_error("Document is closed or belongs to another component");return found->second;}
    int64_t CreateDocument(Session& session,Json value){if(session.Documents.size()>=8||NextDocument==INT64_MAX)throw std::runtime_error("Close a document before opening another");const auto handle=++NextDocument;session.Documents.emplace(handle,std::move(value));return handle;}
    static std::filesystem::path Documents(){
#if defined(RUNESCHEMA_PRIVATE_TEMPLATE_TEST_DOCUMENTS)
        const auto root=std::filesystem::path{RUNESCHEMA_PRIVATE_TEMPLATE_TEST_DOCUMENTS};
        CacheBundleIO::Safe(root);return root;
#else
        PWSTR path=nullptr;if(FAILED(SHGetKnownFolderPath(FOLDERID_Documents,KF_FLAG_DEFAULT,nullptr,&path))||!path)throw std::runtime_error("Known Documents folder is unavailable");
        struct Free {PWSTR Value;~Free(){CoTaskMemFree(Value);}} free{path};return std::filesystem::path(path);
#endif
    }
    bool Handle(UObject* source,FFrame& stack){
        // Most Blueprint calls are unrelated; reject them without allocating a class path.
        if(!stack.Node()||!ServiceNames.contains(stack.Node()->GetFName()))return false;
        if(!Usable(source)||!source->GetClassPrivate())return false;
        const auto route=Routes.find(RC::to_string(source->GetClassPrivate()->GetPathName()));if(route==Routes.end())return false;
        const auto name=stack.Node()->GetName();const auto contract=Contracts().find(name);if(contract==Contracts().end())return false;
        if(!stack.Locals())throw std::runtime_error("Local service Blueprint frame is unavailable");
        Frame frame{stack.Node(),stack};
        // Clear ordinary status outputs even when another pin fails validation.
        for(const auto* flag:{L"Success",L"Accepted",L"Ready"})if(auto* p=stack.Node()->FindProperty(FName(flag,FNAME_Find));DragonWilds::ReflectedContract::Fits(p,stack.Node()->GetParmsSize())&&CastField<FBoolProperty>(p)&&p->HasAllPropertyFlags(CPF_Parm|CPF_OutParm)&&!p->HasAnyPropertyFlags(CPF_ReferenceParm|CPF_ConstParm|CPF_ReturnParm))frame.Boolean(flag,false);
        if(auto* p=stack.Node()->FindProperty(FName(L"Error",FNAME_Find));DragonWilds::ReflectedContract::Fits(p,stack.Node()->GetParmsSize())&&CastField<FStrProperty>(p)&&p->HasAllPropertyFlags(CPF_Parm|CPF_OutParm)&&!p->HasAnyPropertyFlags(CPF_ReferenceParm|CPF_ConstParm|CPF_ReturnParm))frame.String(L"Error",std::string{"Local service function contract is unavailable"});
        Signature(stack.Node(),contract->second);
        for(const auto& pin:contract->second)if(pin.Output)switch(pin.Kind){case Type::Bool:frame.Boolean(pin.Name,false);break;case Type::Double:frame.Number(pin.Name,0);break;case Type::String:frame.String(pin.Name,std::string{});break;case Type::Numbers:case Type::Strings:frame.Array(pin.Name,Json::array(),pin.Kind==Type::Numbers);break;default:frame.Integer(pin.Name,0);break;}
        try{
            if(stack.Node()->GetOuterPrivate()!=source->GetClassPrivate())throw std::runtime_error("Service handler belongs to a different class");
            auto& session=Scope(source,route->second);const auto key=Key(session.Source);
            if(name==L"RuneSchemaCompute"){
                const auto resource=frame.String(L"Resource",290);const auto slash=resource.rfind('/');
                if(slash==resource.npos||std::find(route->second.ComputeProviders.begin(),route->second.ComputeProviders.end(),resource.substr(0,slash))==route->second.ComputeProviders.end())throw std::runtime_error("Native compute provider is not allowed for this component");
                const auto generation=frame.Integer(L"Generation");if(generation<=0)throw std::runtime_error("Generation must be positive");
                const auto ticket=Compute.Submit(session.Owner,key,resource,frame.Array(L"Weights",true).get<std::vector<double>>(),static_cast<uint64_t>(generation));frame.Integer(L"Ticket",ticket);frame.Boolean(L"Accepted",true);return true;
            }
            if(name==L"RuneSchemaComputePoll"){
                if(route->second.ComputeProviders.empty())throw std::runtime_error("Native compute is not enabled for this component");
                const auto ticket=frame.Integer(L"Ticket");if(ticket<=0)throw std::runtime_error("Ticket must be positive");const auto result=Compute.Poll(key,ticket);
                frame.Boolean(L"Ready",result.Ready);frame.Boolean(L"Success",result.Success);frame.Integer(L"Generation",result.Generation);frame.Array(L"Values",result.Values,true);frame.String(L"Error",result.Error);return true;
            }
            if(!route->second.Templates)throw std::runtime_error("Template services are not enabled for this component");
            if(name==L"RuneSchemaJsonCreate")frame.Integer(L"Document",CreateDocument(session,Json::object()));
            else if(name==L"RuneSchemaTemplateList"){
                const auto list=ModTemplates::Store(Documents(),session.Owner).List();frame.Array(L"Templates",list.Ids,false);frame.Array(L"Labels",list.Labels,false);
            }else if(name==L"RuneSchemaTemplateRead")frame.Integer(L"Document",CreateDocument(session,ModTemplates::Store(Documents(),session.Owner).Read(frame.String(L"TemplateId",240))));
            else{
                const auto handle=frame.Integer(L"Document");auto& document=Document(session,handle);
                if(name==L"RuneSchemaTemplateCreate")frame.String(L"TemplateId",ModTemplates::Store(Documents(),session.Owner).Create(document));
                else if(name==L"RuneSchemaDocumentClose")session.Documents.erase(handle);
                else{
                    const auto pointer=frame.String(L"Pointer",512);
                    if(name==L"RuneSchemaJsonSet")ModTemplates::Set(document,pointer,ModTemplates::Scalar(static_cast<unsigned>(frame.Integer(L"Kind")),frame.Number(L"Number"),frame.String(L"Text"),frame.Boolean(L"Boolean")));
                    else if(name==L"RuneSchemaJsonSetNumbers"||name==L"RuneSchemaJsonSetStrings")ModTemplates::Set(document,pointer,frame.Array(L"Values",name==L"RuneSchemaJsonSetNumbers"));
                    else{
                        const auto& value=ModTemplates::Read(document,pointer);
                        if(name==L"RuneSchemaJsonRead"){
                            frame.Integer(L"Kind",ModTemplates::Kind(value));frame.Integer(L"Count",value.is_structured()?value.size():0);
                            if(value.is_boolean())frame.Boolean(L"Boolean",value.get<bool>());else if(value.is_number())frame.Number(L"Number",value.get<double>());else if(value.is_string())frame.String(L"Text",value.get<std::string>());
                        }else if(name==L"RuneSchemaJsonKeys"){
                            if(!value.is_object())throw std::runtime_error("JSON keys require an object");auto keys=Json::array();for(const auto& [key,unused]:value.items())keys.push_back(key);frame.Array(L"Keys",keys,false);
                        }else frame.Array(L"Values",value,name==L"RuneSchemaJsonReadNumbers");
                    }
                }
            }
            frame.Boolean(L"Success",true);
        }catch(const std::exception& error){frame.String(L"Error",error.what());}
        return true;
    }
    void Complete(){
        const auto completed=Compute.TakeCompleted();if(completed.empty())return;Prune();
        static const std::vector<Pin> signature={{L"Ticket",Type::Int64},{L"Success",Type::Bool},{L"Generation",Type::Int64},{L"Values",Type::Numbers},{L"Error",Type::String},{L"ResourceGeneration",Type::Int64}};
        for(const auto& result:completed)try{
            const auto found=Sessions.find(result.Component);if(found==Sessions.end())continue;auto* source=found->second.Source.Get();if(!Usable(source))continue;
            auto* function=source->GetFunctionByNameInChain(TEXT("RuneSchemaComputeCompleted"));if(!function)continue;
            if(function->GetOuterPrivate()!=source->GetClassPrivate())throw std::runtime_error("Compute completion handler belongs to another class");Signature(function,signature);
            AH::FunctionCall call(source,function);call.Arg(TEXT("Ticket"),static_cast<int64_t>(result.Ticket)).Arg(TEXT("Success"),result.Success)
                .Arg(TEXT("Generation"),static_cast<int64_t>(result.Generation)).JsonArg(TEXT("Values"),result.Values).StringArg(TEXT("Error"),result.Error)
                .Arg(TEXT("ResourceGeneration"),static_cast<int64_t>(result.ResourceGeneration)).Invoke();
        }catch(const std::exception& error){Warning(error.what());}
    }
};
// Kept alive if late teardown cannot run provider release callbacks on the game thread.
State& Current(){static auto* state=new State;return *state;}
int32_t RS_MOD_CALL RegisterProvider(void* owner,const RuneSchemaModProviderV1* provider,uint64_t* handle){
    if(handle)*handle=0;if(!IsInGameThread()||!provider||!handle)return RS_MOD_INVALID_ARGUMENT;
    try{*handle=Current().Compute.RegisterProvider(owner,*provider);return RS_MOD_OK;}catch(...){return RS_MOD_FAILED;}
}
int32_t RS_MOD_CALL RegisterResource(void* owner,uint64_t provider,const RuneSchemaModResourceV1* resource,uint64_t* handle){
    if(handle)*handle=0;if(!IsInGameThread()||!resource||!handle)return RS_MOD_INVALID_ARGUMENT;
    try{*handle=Current().Compute.RegisterResource(owner,provider,*resource);return RS_MOD_OK;}catch(...){return RS_MOD_FAILED;}
}
int32_t RS_MOD_CALL ReleaseResource(void* owner,uint64_t resource){if(!IsInGameThread())return RS_MOD_UNAVAILABLE;try{Current().Compute.Release(owner,resource);return RS_MOD_OK;}catch(...){return RS_MOD_FAILED;}}
int32_t RS_MOD_CALL PublishNetworkSnapshot(void* context,const char* channel,const char* payload,uint32_t bytes){
    if(!IsInGameThread())return RS_MOD_UNAVAILABLE;
    if(!channel||!payload||!bytes||bytes>ModDataRelay::MaxBytes)return RS_MOD_INVALID_ARGUMENT;
    const auto size=strnlen_s(channel,129);
    if(!size||size>128||std::string_view(channel,size).find(':')!=std::string_view::npos)return RS_MOD_INVALID_ARGUMENT;
    const auto& owners=Current().NetworkOwners;
    const auto owner=owners.find(context);
    if(owner==owners.end())return RS_MOD_UNAVAILABLE;
    try{return MultiplayerRuntime::PublishSnapshot(owner->second,std::string(channel,size),std::string(payload,bytes))
        ?RS_MOD_OK:RS_MOD_NOT_FOUND;}catch(...){return RS_MOD_FAILED;}
}
}
void Bind(const std::string& plugin,const std::string& owner){
    if(!IsInGameThread())throw std::runtime_error("Mod-owned plugins initialize on the game thread");
    auto& state=Current();auto* context=state.Compute.Bind(plugin,owner);
    state.NetworkOwners.insert_or_assign(context,owner);
    state.Apis.insert_or_assign(plugin,RuneSchemaModApiV1{sizeof(RuneSchemaModApiV1),1,context,
        RegisterProvider,RegisterResource,ReleaseResource,PublishNetworkSnapshot});
}
const RuneSchemaModApiV1* Api(const char* plugin){if(!plugin||!IsInGameThread())return nullptr;const auto found=Current().Apis.find(plugin);return found==Current().Apis.end()||!found->second.OwnerContext?nullptr:&found->second;}
bool Drop(const std::string& plugin) noexcept {
    auto& state=Current();if(!state.Apis.contains(plugin)||!state.Apis.at(plugin).OwnerContext)return true;
    if(!IsInGameThread()){try{state.Compute.Pause(plugin);}catch(...){}Warning("Late teardown retained a mod-owned DLL after draining workers: game-thread resource release is unavailable");return false;}
    try{state.Compute.Drop(plugin);auto& api=state.Apis.at(plugin);state.NetworkOwners.erase(api.OwnerContext);api.OwnerContext=nullptr;return true;}catch(...){Warning("Native provider teardown could not be completed; its DLL was retained");return false;}
}
void Stop() noexcept {
    auto& state=Current();if(state.Script!=Hook::ERROR_ID)Hook::UnregisterCallback(state.Script);if(state.Tick!=Hook::ERROR_ID)Hook::UnregisterCallback(state.Tick);state.Script=state.Tick=Hook::ERROR_ID;
    for(const auto& [key,session]:state.Sessions)state.Compute.Cancel(key);state.Sessions.clear();state.Routes.clear();state.ServiceNames.clear();
}
void Start(){
    auto& state=Current();Stop();if(Storefront::IsDedicatedServer())return;
    std::set<std::string> conflicts;
    for(const auto& route:CookedPakRegistryManifest::LocalServiceDefinitions()){
        const auto [found,inserted]=state.Routes.emplace(route.Class,route);
        if(!inserted&&(found->second.Owner!=route.Owner||found->second.Id!=route.Id||found->second.Templates!=route.Templates||found->second.ComputeProviders!=route.ComputeProviders))conflicts.insert(route.Class);
    }
    for(const auto& path:conflicts){state.Routes.erase(path);Warning("Conflicting local service class declarations: "+path);}if(state.Routes.empty())return;
    state.ServiceNames.reserve(Contracts().size());
    for(const auto& [name,pins]:Contracts())if(!state.ServiceNames.insert(FName(name.c_str(),FNAME_Add)).second)throw std::runtime_error("Duplicate local service function name");
    Hook::FCallbackOptions options{};options.OwnerModName=TEXT("RuneSchema");options.HookName=TEXT("ModLocalServices");
    state.Script=Hook::RegisterProcessLocalScriptFunctionPreCallback([&state](auto& iteration,UObject* source,FFrame& stack,void*){
        try{if(state.Handle(source,stack))iteration.PreventOriginalFunctionCall();}catch(const std::exception& error){iteration.PreventOriginalFunctionCall();Warning(error.what());}
    },options);
    if(state.Script==Hook::ERROR_ID){Stop();Warning("Local service script hook is unavailable");return;}
    if(std::none_of(state.Routes.begin(),state.Routes.end(),[](const auto& row){return !row.second.ComputeProviders.empty();}))return;
    state.Tick=Hook::RegisterEngineTickPostCallback([&state](auto&,auto*,float,bool){try{state.Complete();}catch(const std::exception& error){Warning(error.what());}},options);
    if(state.Tick==Hook::ERROR_ID){Stop();Warning("Local service completion hook is unavailable");}
}
}
