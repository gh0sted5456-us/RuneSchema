#pragma once
#include "Core/ServerModList.h"
#include "Generator/ToolRequest.h"
#include "SDK/Helper/ActorHelper.h"
#include "SDK/Helper/ReflectedContract.h"
#include "SDK/Helper/PropertyHelper.h"
#include "SDK/WeakObjectHandle.h"
#include "Unreal/Property/FEnumProperty.hpp"
#include "Unreal/UObjectGlobals.hpp"
#include "Unreal/World.hpp"
#include "Utility/Logging.h"

namespace PS::Network {
// Only the live WorldModdedWarning text is changed; game buttons and callbacks remain game-owned.
class ServerModNotice {
    WeakObjectHandle m_dialog,m_world;
    nlohmann::json m_mods;
    std::string m_original;
    uint64_t m_generation=0;
    static std::string Text(RC::Unreal::UStruct* type,void* owner,const wchar_t* name){
        namespace C=DragonWilds::ReflectedContract;using namespace RC::Unreal;
        auto* field=type->FindProperty(FName(name,FNAME_Find));
        if(!C::Text(field)||!C::Fits(field,type->GetPropertiesSize()))throw std::runtime_error("Native mod notice text field changed");
        return RC::to_string(DragonWilds::PropertyHelper::GetTextAsString(*field->ContainerPtrToValuePtr<FText>(owner)));
    }
    static RC::Unreal::UObject* Object(RC::Unreal::UObject* owner,const wchar_t* name){
        namespace C=DragonWilds::ReflectedContract;using namespace RC::Unreal;
        auto* field=CastField<FObjectProperty>(owner->GetClassPrivate()->GetPropertyByNameInChain(name));
        if(!C::Fits(field,owner->GetClassPrivate()->GetPropertiesSize())||field->GetSize()!=sizeof(UObject*))throw std::runtime_error("Native mod notice object contract changed");
        return field->GetObjectPropertyValue(field->ContainerPtrToValuePtr<void>(owner));
    }
    void Discover(){
        using namespace RC::Unreal;using Call=DragonWilds::ActorHelper::FunctionCall;namespace C=DragonWilds::ReflectedContract;
        if(!m_world.Get())return;
        std::vector<UObject*> bases;UObjectGlobals::FindAllOf(L"GenericDialogBase",bases);if(bases.size()>64)return;
        for(auto* base:bases){
            if(!base||base->GetWorld()!=m_world.Get()||base->HasAnyFlags(static_cast<EObjectFlags>(RF_ClassDefaultObject|RF_ArchetypeObject|RF_BeginDestroyed|RF_FinishDestroyed)))continue;
            auto* stack=Object(base,L"DialogStack");auto* asset=Object(base,L"DialogMessageData");if(!stack||!asset)continue;
            Call current(stack,L"GetActiveWidget");current.Invoke();auto* dialog=current.Result<UObject*>();if(!dialog)continue;
            auto* body=Object(dialog,L"BodyText");if(!body)continue;const auto shown=Text(body->GetClassPrivate(),body,L"Text");
            auto* map=CastField<FMapProperty>(asset->GetClassPrivate()->GetPropertyByNameInChain(L"DialogMessageData"));
            if(!C::Fits(map,asset->GetClassPrivate()->GetPropertiesSize()))continue;
            auto* key=CastField<FEnumProperty>(map->GetKeyProp());auto* value=CastField<FStructProperty>(map->GetValueProp());
            if(!key||!key->GetUnderlyingProperty()||key->GetEnum()->GetPathName()!=L"/Script/Dominion.EDialogMessageType"
                ||!value||value->GetStruct()->GetPathName()!=L"/Script/Dominion.MessageData")continue;
            auto* entries=map->ContainerPtrToValuePtr<FScriptMap>(asset);if(entries->GetMaxIndex()<0||entries->GetMaxIndex()>1024)continue;
            const auto layout=FScriptMap::GetScriptLayout(key->GetSize(),key->GetMinAlignment(),value->GetSize(),value->GetMinAlignment());
            for(int i=0;i<entries->GetMaxIndex();++i)if(entries->IsValidIndex(i)){
                auto* entry=static_cast<unsigned char*>(entries->GetData(i,layout));
                const auto ordinal=key->GetUnderlyingProperty()->GetSignedIntPropertyValue(entry);
                if(key->GetEnum()->GetNameByValue(ordinal).ToString()!=L"EDialogMessageType::WorldModdedWarning")continue;
                const auto expected=Text(value->GetStruct(),entry+layout.ValueOffset,L"BodyText");
                if(!expected.empty()&&expected==shown){m_original=shown;m_dialog.Assign(dialog);return;}
            }
        }
    }
    void Update(){
        using namespace RC::Unreal;using Call=DragonWilds::ActorHelper::FunctionCall;
        auto* dialog=m_dialog.Get();if(!dialog||!m_mods.is_array()||dialog->GetWorld()!=m_world.Get()||m_generation!=SpawnToolRequests::Generation.load())return;
        Call active(dialog,TEXT("/Script/CommonUI.CommonActivatableWidget:IsActivated"));active.Invoke();if(!active.Result<bool>())return;
        auto* body=Object(dialog,L"BodyText");if(!body)return;
        bool scrollable=false;
        auto* scrollType=UObjectGlobals::StaticFindObject<UClass*>(nullptr,nullptr,L"/Script/CommonUI.CommonHierarchicalScrollBox");
        auto* parent=body;for(int depth=0;depth<8&&parent&&scrollType;++depth){
            Call get(parent,L"GetParent");get.Invoke();parent=get.Result<UObject*>();if(parent&&parent->IsA(scrollType)){scrollable=true;break;}
            if(!parent||parent==dialog)break;
        }
        std::string text=m_original+"\n\nRuneSchema — "+std::to_string(m_mods.size())+" enabled mod(s):";
        const auto count=scrollable?m_mods.size():std::min(m_mods.size(),size_t{3});
        for(size_t i=0;i<count;++i){const auto& row=m_mods[i];text+="\n• "+row.at("Name").get<std::string>();const auto version=row.at("Version").get<std::string>();if(!version.empty())text+=" ("+version+")";}
        if(m_mods.size()>count)text+="\n… and "+std::to_string(m_mods.size()-count)+" more.";
        text+="\nView the full list in Helpy → Mods → Server comparison.";
        auto* fn=body->GetFunctionByNameInChain(L"SetText");namespace C=DragonWilds::ReflectedContract;
        if(!fn||C::Count(fn)!=1||!C::Text(fn->FindProperty(FName(L"InText",FNAME_Find)))||!C::Input(fn->FindProperty(FName(L"InText",FNAME_Find)),fn))throw std::runtime_error("Native mod notice text contract changed");
        Call(body,fn).JsonArg(L"InText",text).Invoke();
    }
public:
    void Clear(){m_dialog.Reset();m_world.Reset();m_mods=nullptr;m_original.clear();m_generation=0;SpawnToolRequests::CatalogProgress({{"ServerMods",nullptr}});}
    void Receive(RC::Unreal::UObject* context,const nlohmann::json& mods){
        m_mods=ServerModList::Validate(mods);SpawnToolRequests::CatalogProgress({{"ServerMods",m_mods}});
        m_generation=SpawnToolRequests::Generation.load();if(context)m_world.Assign(context->GetWorld());
        if(auto* dialog=m_dialog.Get();dialog&&dialog->GetWorld()!=m_world.Get())m_dialog.Reset();
        try{if(!m_dialog.Get())Discover();Update();}catch(const std::exception&){PS::Log<RC::LogLevel::Verbose>(STR("[NETWORK][MOD-NOTICE] Native warning unavailable; full server list remains in Helpy.\n"));}
    }
    void Observe(RC::Unreal::UObject* source,RC::Unreal::UFunction* function,void* parameters){
        using namespace RC::Unreal;using Call=DragonWilds::ActorHelper::FunctionCall;
        static const FName open(L"OpenDialog",FNAME_Add);
        if(function->GetFName()!=open||function->GetPathName()!=L"/Script/Dominion.GenericDialogUIAPI:OpenDialog"||!parameters)return;
        if(m_generation!=SpawnToolRequests::Generation.load()||m_world.Get()!=source->GetWorld())return;
        auto* field=CastField<FEnumProperty>(function->FindProperty(FName(L"Type",FNAME_Find)));
        if(!field||!field->GetUnderlyingProperty()||!DragonWilds::ReflectedContract::Input(field,function))return;
        const auto ordinal=field->GetUnderlyingProperty()->GetSignedIntPropertyValue(field->ContainerPtrToValuePtr<void>(parameters));
        if(field->GetEnum()->GetPathName()!=L"/Script/Dominion.EDialogMessageType"||field->GetEnum()->GetNameByValue(ordinal).ToString()!=L"EDialogMessageType::WorldModdedWarning")return;
        Call base(source,L"GetDialogBase");base.Invoke();if(!base.Result<UObject*>())return;
        auto* stack=Object(base.Result<UObject*>(),L"DialogStack");if(!stack)return;
        Call current(stack,L"GetActiveWidget");current.Invoke();auto* dialog=current.Result<UObject*>();if(!dialog)return;
        auto* body=Object(dialog,L"BodyText");if(!body)return;
        auto* text=body->GetClassPrivate()->GetPropertyByNameInChain(L"Text");
        if(!DragonWilds::ReflectedContract::Text(text)||!DragonWilds::ReflectedContract::Fits(text,body->GetClassPrivate()->GetPropertiesSize()))throw std::runtime_error("Native mod notice body contract changed");
        m_original=RC::to_string(DragonWilds::PropertyHelper::GetTextAsString(*text->ContainerPtrToValuePtr<FText>(body)));m_dialog.Assign(dialog);Update();
    }
};
}
