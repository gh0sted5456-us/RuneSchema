#include "Generator/HelpyIconPacks.h"
#include <stdexcept>
int main(){
    using namespace PS;
    const auto check=[](bool value){if(!value)throw std::runtime_error("Cache handoff or polling regression");};
    {
        using namespace HelpyIconPacks;unsigned reads=0;
        Open=false;BuildState={{"Active",false}};PendingBuildExport.clear();NextBuildRead={};
        RemoteBuild=[&](const auto&,auto& reply){++reads;reply={{"Active",false}};return true;};
        for(int i=0;i<50;++i)PollBuild();check(reads==0);
        Open=true;PollBuild();check(reads==1);PollBuild();check(reads==1);
        Open=false;PollBuild();Open=true;PollBuild();check(reads==2);
        Open=false;BuildState={{"Active",true}};NextBuildRead={};PollBuild();check(reads==3);
        PollBuild();check(reads==3);RemoteBuild={};
    }
    HelpyPaths::Configure(std::filesystem::temp_directory_path()/"handoff-fixture"/"plugins"/"Helpy");
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
