#include "Loader/DragonWildsVendorLoader.h"
#include "Loader/DragonWildsNpcLoader.h"
#include "Utility/JsonHelpers.h"
#include "Utility/Logging.h"
using namespace RC;
namespace DragonWilds {
DragonWildsVendorLoader::DragonWildsVendorLoader(DragonWildsNpcLoader* npcs)
    : DragonWildsModLoaderBase("vendors"),m_npcs(npcs) { SetDisplayName(TEXT("Store Loader")); }
bool DragonWildsVendorLoader::CanInitialize(const EEngineLifecyclePhase& phase) {
    return phase==EEngineLifecyclePhase::PostEngineInit;
}
bool DragonWildsVendorLoader::OnInitialize() { return m_npcs && m_npcs->HasInitialized(); }
void DragonWildsVendorLoader::OnLoad(const std::filesystem::path& path,const RC::StringType& mod,const EEngineLifecyclePhase& phase) {
    if(phase!=EEngineLifecyclePhase::PostEngineInit)return;
    PS::JsonHelpers::ParseJsonFilesInPath(path,[&](const nlohmann::json& data){m_npcs->LoadVendors(data,mod);});
}
void DragonWildsVendorLoader::OnAutoReload(const RC::StringType& mod,const std::filesystem::path&) {
    PS::Log<LogLevel::Warning>(TEXT("Store changes in {} require a restart.\n"),mod);
}
}
