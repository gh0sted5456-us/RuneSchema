#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
static std::string Read(const std::filesystem::path& path){std::ifstream file(path);if(!file)throw std::runtime_error("Missing server Helpy contract source");return {std::istreambuf_iterator<char>(file),{}};}
static void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(int argc,char** argv){
    Check(argc==2,"Expected source root");const std::filesystem::path root=argv[1];
    const auto tools=Read(root/"raw/src/Loader/SpawnTools.inl"),catalog=Read(root/"raw/src/Loader/QuickMenuCatalog.inl");
    const auto items=Read(root/"raw/src/Generator/AssetSearch.cpp"),host=Read(root/"raw/src/Runtime/PluginHost.cpp");
    const auto life=Read(root/"raw/src/Loader/DragonWildsSpawnLoader.cpp"),travel=Read(root/"raw/src/Loader/MapTravel.inl");
    const auto lookup=Read(root/"raw/src/SDK/Classes/Custom/UObjectGlobals.cpp");
    Check(lookup.contains("if(!FUObjectHashTables::IsAvailable() && !PS::Storefront::IsDedicatedServer()"),"Public class-hash iteration must precede client AOB fallback; no client iterator ABI guesses on servers");
    Check(tools.find("if(action==\"CatalogWindow\")")<tools.find("auto* controllerClass="),"Catalogue queries must not require a connected or local player");
    Check(tools.contains("HelpyQueryService::PumpOne(PS::SpawnToolRequests::Waiting.load(),runCatalogWindow)"),"Accepted catalogue reads must dispatch without a ready spawn world");
    Check(tools.contains("if(!catalogRead&&!PS::SpawnToolRequests::Waiting.load())PS::AssetSearch::PumpItemCatalog()"),"Background catalogue refresh must yield to both item mutations and dispatched reads");
    Check(items.contains("ItemPages,ItemPreviewPages")&&items.contains("ItemRefreshPending&&!ItemRefreshPumping"),"A stale-scope presentation cache must survive bounded background refresh without evicting strict-scope read state");
    Check(!tools.contains("if(m_readyWorld&&IsWorldStillLoaded(m_readyWorld))if(auto queryRequest="),"World-gated metadata query starvation returned");
    Check(tools.contains("UWorld* catalogWorld=m_readyWorld&&IsWorldStillLoaded(m_readyWorld)?m_readyWorld:nullptr"),"Empty dedicated worlds must still have a live catalogue context");
    Check(items.contains("CookedAssets::MetadataSnapshot()")&&items.contains("F2Catalog::MountedCandidate")&&!items.contains("IsDedicatedServer"),"Mounted item/armor discovery must not depend on client rendering");
    const auto mounted=catalog.substr(catalog.find("ToolMountedDefinitionHints()"),catalog.find("struct ToolCatalogIndex")-catalog.find("ToolMountedDefinitionHints()"));
    Check(mounted.find("CookedAssets::TryEnsure()")<mounted.find("generation=current;"),"Starting registries must not seal an empty AI/NPC/resource list");
    Check(tools.contains("Only the authoritative server may run spawn tools")&&tools.contains("ToolMapTravelBegin(world"),"Travel and world mutations must retain server authority");
    Check(!travel.contains("GetFirstLocalPlayer")&&travel.contains("TeleportationSubsystem:TeleportWithParams"),"Dedicated travel must use the selected player and native service, not a local viewport");
    Check(host.contains("Storefront::IsDedicatedServer()&&manifest.Id!=\"RuneSchema.Helpy\""),"Server desktop Helpy must still start while ordinary client plugins stay excluded");
    const auto validation=life.substr(life.find("bool DragonWildsSpawnLoader::IsWorldStillLoaded"),2000);
    Check(validation.find("m_validatedWorld.Get()==world")<validation.find("GetObjectsOfClass")&&validation.contains("m_validatedWorld.Assign(candidate)"),"Known worlds must use serial-valid handles before a whole-object scan");
}
