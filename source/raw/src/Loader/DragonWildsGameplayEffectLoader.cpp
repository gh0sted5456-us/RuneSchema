#include "Loader/DragonWildsGameplayEffectLoader.h"
#include "Loader/GameplayEffectDocument.h"
#include "Loader/DefinitionRegistry.h"
#include "SDK/Classes/Custom/UObjectGlobals.h"
#include "SDK/Classes/KismetSystemLibrary.h"
#include "SDK/Classes/TSoftObjectPtr.h"
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Helpers/String.hpp"
#include "Utility/JsonHelpers.h"
#include "Utility/Logging.h"
#include <stdexcept>

using namespace RC;
using namespace RC::Unreal;
namespace fs = std::filesystem;
namespace DragonWilds {

DragonWildsGameplayEffectLoader::DragonWildsGameplayEffectLoader()
    : DragonWildsModLoaderBase("effects") { SetDisplayName(TEXT("Effects Loader")); }
DragonWildsGameplayEffectLoader::~DragonWildsGameplayEffectLoader() { DefinitionRegistry::Effects.clear(); }
bool DragonWildsGameplayEffectLoader::CanInitialize(const EEngineLifecyclePhase& phase) {
    return phase==EEngineLifecyclePhase::PostEngineInit;
}
bool DragonWildsGameplayEffectLoader::OnInitialize() { return true; }
void DragonWildsGameplayEffectLoader::OnLoad(const fs::path& path,const RC::StringType& owner,
    const EEngineLifecyclePhase& phase) {
    if(phase!=EEngineLifecyclePhase::PostEngineInit)return;
    PS::JsonHelpers::ParseJsonFilesInPath(path,[&](const nlohmann::json& data){Queue(data,owner);});
}
void DragonWildsGameplayEffectLoader::OnAutoReload(const RC::StringType& owner,const fs::path&) {
    PS::Log<LogLevel::Warning>(TEXT("{}: effect class aliases require a game restart.\n"),owner);
}
void DragonWildsGameplayEffectLoader::Queue(const nlohmann::json& data,const RC::StringType& owner) {
    for(auto& d:GameplayEffectDocument::Parse(data,to_string(owner)))
        m_definitions.push_back({owner,std::move(d.Key),std::move(d.ClassPath)});
}
void DragonWildsGameplayEffectLoader::Apply(const Definition& definition) {
    if(DefinitionRegistry::Effects.contains(definition.Key))
        throw std::runtime_error("duplicate effect definition ID");
    const auto path=to_generic_string(definition.ClassPath);
    auto* object=UECustom::UObjectGlobals::StaticFindObject(nullptr,nullptr,path.c_str(),false);
    if(!object) {
        auto soft=UECustom::TSoftObjectPtr<UObject>(UECustom::FSoftObjectPath(path));
        object=UECustom::UKismetSystemLibrary::LoadAsset_Blocking(soft);
    }
    if(!object || !object->IsA<UClass>())throw std::runtime_error("Class did not resolve to a cooked Blueprint class");
    auto* type=static_cast<UClass*>(object);
    DefinitionRegistry::Effects.emplace(definition.Key,type);
    PS::Log<LogLevel::Verbose>(TEXT("{}: registered effect class '{}' -> {}.\n"),definition.Owner,
        to_generic_string(definition.Key),type->GetPathName());
}
void DragonWildsGameplayEffectLoader::OnFinalizeLoad(const EEngineLifecyclePhase& phase) {
    if(phase!=EEngineLifecyclePhase::PostEngineInit)return;
    for(const auto& definition:m_definitions)try { Apply(definition); }
    catch(const std::exception& error) { PS::Log<LogLevel::Error>(TEXT("Gameplay effect '{}' rejected: {}\n"),to_generic_string(definition.Key),PS::ToWideSafe(error.what())); }
    m_definitions.clear();
}
UClass* DragonWildsGameplayEffectLoader::Resolve(const std::string& key) const {
    const auto found=DefinitionRegistry::Effects.find(key);
    return found==DefinitionRegistry::Effects.end()?nullptr:found->second;
}
}
