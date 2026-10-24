#include "Loader/DragonWildsNiagaraLoader.h"
#include "Loader/DefinitionRegistry.h"
#include "Loader/VirtualDefinitionId.h"
#include "Loader/Spawn/RuntimeSupport.h"
#include "Utility/JsonHelpers.h"
#include "Utility/Logging.h"
#include "Utility/StructuredDiagnosticB3C.h"
#include "Helpers/String.hpp"
namespace DragonWilds {
DragonWildsNiagaraLoader::DragonWildsNiagaraLoader():DragonWildsModLoaderBase("niagara") {
    SetDisplayName(TEXT("Niagara Loader"));
}
DragonWildsNiagaraLoader::~DragonWildsNiagaraLoader() { DefinitionRegistry::Niagara.clear(); }
bool DragonWildsNiagaraLoader::CanInitialize(const EEngineLifecyclePhase& phase) { return phase==EEngineLifecyclePhase::PostEngineInit; }
bool DragonWildsNiagaraLoader::OnInitialize() { return true; }
void DragonWildsNiagaraLoader::OnAutoReload(const RC::StringType& owner,const std::filesystem::path&) {
    if (!PS::StructuredDiagnosticB3C::Report("RS-NIA-004","Warning","Deferred","NiagaraLoader",
        "Niagara reload requires restart","Niagara definition changes require a game restart.",RC::to_string(owner),{}, {},"Reload"))
        PS::Log<RC::LogLevel::Warning>(TEXT("{}: Niagara definitions require a game restart.\n"),owner);
}
void DragonWildsNiagaraLoader::OnLoad(const std::filesystem::path& path,const RC::StringType& owner,const EEngineLifecyclePhase& phase) {
    if(phase!=EEngineLifecyclePhase::PostEngineInit)return;
    PS::JsonHelpers::ParseJsonFilesInPathWithSource(path,[&](const nlohmann::json& data,const std::filesystem::path& relative) {
        try {
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
        } catch(const std::exception& e) {
            const std::string_view reason(e.what());
            const auto code=reason=="Niagara document must map IDs to definitions"?"RS-NIA-001":
                reason.starts_with("Duplicate Niagara ID:")?"RS-NIA-002":"RS-NIA-003";
            PS::StructuredDiagnosticB3C::Report(code,"Error","Skipped","NiagaraLoader",
                "Niagara definition rejected",e.what(),RC::to_string(owner),
                PS::StructuredDiagnosticB2::Utf8Path(relative),{},"Validation");
            throw;
        }
    });
}
}
