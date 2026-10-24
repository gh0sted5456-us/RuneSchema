#pragma once
#include "Loader/DragonWildsNpcLoader.h"
#include "Utility/JsonHelpers.h"
#include "Utility/Logging.h"
#include "Utility/StructuredDiagnosticB3C.h"
namespace DragonWilds {
class DragonWildsDialogueLoader final : public DragonWildsModLoaderBase {
public:
    explicit DragonWildsDialogueLoader(DragonWildsNpcLoader* npcs)
        :DragonWildsModLoaderBase("dialogue"),m_npcs(npcs) {SetDisplayName(TEXT("Dialogue Loader"));}
protected:
    bool CanInitialize(const EEngineLifecyclePhase& phase) override {return phase==EEngineLifecyclePhase::PostEngineInit;}
    bool OnInitialize() override {return m_npcs && m_npcs->HasInitialized();}
    void OnLoad(const std::filesystem::path& path,const RC::StringType& mod,const EEngineLifecyclePhase& phase) override {
        if(phase==EEngineLifecyclePhase::PostEngineInit)
            PS::JsonHelpers::ParseJsonFilesInPath(path,[&](const nlohmann::json& data){m_npcs->LoadDialogues(data,mod);});
    }
    void OnAutoReload(const RC::StringType& mod,const std::filesystem::path&) override {
        PS::StructuredDiagnosticB3C::Report("RS-CORE-037","Warning","Deferred","LoaderCore",
            "Dialogue reload requires restart","Dialogue changes are not applied through live reload.",RC::to_string(mod),{},"DialogueReload","Reload");
        PS::Log<RC::LogLevel::Warning>(TEXT("Dialogue changes in {} require a restart.\n"),mod);
    }
private:
    DragonWildsNpcLoader* m_npcs;
};
}
