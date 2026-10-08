#include "Loader/DragonWildsNiagaraLoader.h"
#include "Loader/DefinitionRegistry.h"
#include "Loader/VirtualDefinitionId.h"
#include "Loader/Spawn/RuntimeSupport.h"
#include "Utility/JsonHelpers.h"
#include "Utility/Logging.h"
#include "Helpers/String.hpp"
namespace DragonWilds {
DragonWildsNiagaraLoader::DragonWildsNiagaraLoader():DragonWildsModLoaderBase("niagara") {
    SetDisplayName(TEXT("Niagara Loader"));
}
DragonWildsNiagaraLoader::~DragonWildsNiagaraLoader() { DefinitionRegistry::Niagara.clear(); }
bool DragonWildsNiagaraLoader::CanInitialize(const EEngineLifecyclePhase& phase) { return phase==EEngineLifecyclePhase::PostEngineInit; }
bool DragonWildsNiagaraLoader::OnInitialize() { return true; }
void DragonWildsNiagaraLoader::OnAutoReload(const RC::StringType& owner,const std::filesystem::path&) {
    PS::Log<RC::LogLevel::Warning>(TEXT("{}: Niagara definitions require a game restart.\n"),owner);
}
void DragonWildsNiagaraLoader::OnLoad(const std::filesystem::path& path,const RC::StringType& owner,const EEngineLifecyclePhase& phase) {
    if(phase!=EEngineLifecyclePhase::PostEngineInit)return;
    PS::JsonHelpers::ParseJsonFilesInPath(path,[&](const nlohmann::json& data) {
        if(!data.is_object())throw std::runtime_error("Niagara document must map IDs to definitions");
        auto pending=DefinitionRegistry::Niagara;
        for(const auto& [key,body]:data.items()) {
            if(key=="$Comment")continue;
            const auto id=VirtualDefinitionId::Qualify(RC::to_string(owner),key);
            if(!VirtualDefinitionId::IsCanonical(id) || id.substr(0,id.find(':'))!=RC::to_string(owner))
                throw std::runtime_error("Niagara ID must belong to its mod");
            if(pending.contains(id))throw std::runtime_error("Duplicate Niagara ID: "+id);
            auto value=body;
            if(!value.is_object() || value.contains("Definition"))throw std::runtime_error("Niagara definition requires an inline system and parameters");
            value["Type"]="Niagara";
            value=SpawnRuntime::ValidateVisualEffect(value);
            pending.emplace(id,std::move(value));
        }
        DefinitionRegistry::Niagara.swap(pending);
    });
}
}
