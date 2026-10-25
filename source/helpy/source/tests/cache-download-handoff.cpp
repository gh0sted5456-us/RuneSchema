#include "Generator/HelpyIconPacks.h"
#include <stdexcept>
int main(){
    using namespace PS;
    const auto check=[](bool value){if(!value)throw std::runtime_error("Pending download was lost before import could start");};
    HelpyIconPacks::PendingDownload="download.rsmk-cache";
    HelpyIconPacks::BuildState={{"Active",true}};
    HelpyIconPacks::Poll();check(!HelpyIconPacks::PendingDownload.empty());
    HelpyIconPacks::BuildState={{"Active",false}};
    HelpySourceCache::Current.Authoritative=true;HelpySourceCache::Current.Provisional=true;
    HelpySourceCache::Current.Key="0123456789abcdef";HelpySourceCache::Current.SourceKeys={{"game","0123456789abcdef"}};
    HelpyIconPacks::Poll();check(!HelpyIconPacks::PendingDownload.empty());
    HelpySourceCache::Current.Provisional=false;HelpyRuntimePreferences::IconCacheLimitMB=0;
    HelpyIconPacks::Poll();check(!HelpyIconPacks::PendingDownload.empty());
    check(!HelpyIconPacks::Start("invalid.txt",true));
    HelpyIconPacks::PendingDownload.clear();
}
