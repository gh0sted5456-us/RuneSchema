#include "Generator/HelpyIconPacks.h"
#include <stdexcept>
int main(){
    using namespace PS;
    HelpyPaths::Configure(std::filesystem::temp_directory_path()/"handoff-fixture"/"plugins"/"Helpy");
    const auto check=[](bool value){if(!value)throw std::runtime_error("Pending download was lost before import could start");};
    HelpyIconPacks::PendingDownload="download.rsmk-cache";
    HelpyIconPacks::BuildState={{"Active",true}};
    HelpyIconPacks::Poll();check(!HelpyIconPacks::PendingDownload.empty());
    HelpyIconPacks::BuildState={{"Active",false}};
    HelpySourceCache::Current.Authoritative=true;HelpySourceCache::Current.Provisional=true;
    HelpySourceCache::Current.Key="0123456789abcdef";HelpySourceCache::Current.SourceKeys={{"game","0123456789abcdef"}};
    HelpyIconPacks::Poll();check(HelpyIconPacks::PendingDownload.empty()&&HelpyIconPacks::Busy());
    HelpyIconPacks::Shutdown();HelpyIconPacks::PendingDownload="download.rsmk-cache";
    HelpySourceCache::Current.Provisional=false;HelpyRuntimePreferences::IconCacheLimitMB=0;
    HelpyIconPacks::Poll();check(!HelpyIconPacks::PendingDownload.empty());
    check(!HelpyIconPacks::Start("invalid.txt",true));
    HelpyIconPacks::PendingDownload.clear();
    unsigned notices=0;
    HelpyIconPacks::RemoteBuild=[&](const nlohmann::json& request,nlohmann::json& reply){
        if(request.at("Action")=="CacheBundleImported")++notices;
        reply={{"Accepted",true}};return true;
    };
    std::promise<HelpyIconPack::Result> imported;HelpyIconPacks::Worker=imported.get_future();
    HelpyIconPack::Result metadata;metadata.RecordsImported=1;
    const auto revision=HelpyIconPacks::ReferencesRevision;HelpyIconPacks::Importing=true;
    imported.set_value(metadata);HelpyIconPacks::Poll();
    check(notices==1&&HelpyIconPacks::ReferencesRevision==revision+1);
    HelpyIconPacks::Poll();check(notices==1);HelpyIconPacks::RemoteBuild={};
    HelpySourceCache::Current.Provisional=true;HelpyRuntimePreferences::IconCacheLimitMB=200;
    check(HelpyIconPacks::Start(std::filesystem::temp_directory_path()/"handoff-fixture"/"current-cache.rsmk-cache",false));
    HelpyIconPacks::Shutdown();
}
