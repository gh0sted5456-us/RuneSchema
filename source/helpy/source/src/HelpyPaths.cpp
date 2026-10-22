#include "Runtime/HelpyPaths.h"
#include "Runtime/HostServices.h"
#include "Core/HelpyUiAssets.h"
#include <windows.h>
#include <stdexcept>
#include <vector>

namespace {
std::filesystem::path PluginDirectory;
std::filesystem::path LocalData() {
    const DWORD length=GetEnvironmentVariableW(L"LOCALAPPDATA",nullptr,0);
    if(!length)throw std::runtime_error("LOCALAPPDATA is unavailable");
    std::vector<wchar_t> value(length);
    if(GetEnvironmentVariableW(L"LOCALAPPDATA",value.data(),length)+1!=length)
        throw std::runtime_error("LOCALAPPDATA changed while being read");
    return value.data();
}
}
namespace PS::HelpyPaths {
void Configure(const std::filesystem::path& pluginRoot) {
    PluginDirectory=std::filesystem::absolute(pluginRoot).lexically_normal();
}
const std::filesystem::path& PluginRoot() {
    if(PluginDirectory.empty())throw std::runtime_error("Helpy plugin root has not been configured");
    return PluginDirectory;
}
std::filesystem::path Asset(const char* name){
    // Only the compiled, image-only UI list is eligible for managed downloads.
    // Native scripts, fonts and executable paths never resolve through it.
    if(HelpyUiAssets::Known(name))try{
        const auto folder=LocalData()/"RuneSchema"/"HELPY"/"ui-assets"/HelpyUiAssets::Version;
        const auto candidate=folder/name;
        std::error_code error;
        if(std::filesystem::is_regular_file(folder/"ready",error)&&std::filesystem::is_regular_file(candidate,error))return candidate;
    }catch(...){}
    return PluginRoot()/"assets"/name;
}
}
namespace PS::HostServices {
std::filesystem::path ModDirectory(){return HelpyPaths::PluginRoot().parent_path().parent_path();}
std::filesystem::path WorkingDirectory(){return ModDirectory().parent_path().parent_path();}
std::filesystem::path SettingsDirectory(){return ModDirectory()/"settings";}
// Helpy-owned mutable data stays writable even in a protected game install.
std::filesystem::path StateDirectory(){return LocalData()/"RuneSchema"/"HELPY";}
std::filesystem::path ProfileDirectory(){return LocalData()/"RuneSchema"/"runeschema.profile";}
std::filesystem::path RuntimeDirectory(){return StateDirectory()/"runtime";}
std::filesystem::path SavedDirectory(){return RuntimeDirectory()/"saved";}
std::filesystem::path CacheDirectory(){return SavedDirectory()/"cache";}
std::filesystem::path ProgressDirectory(){return SavedDirectory()/"progress";}
std::filesystem::path ReferencesDirectory(){return SavedDirectory()/"references";}
std::filesystem::path JobsDirectory(){return RuntimeDirectory()/"jobs";}
std::filesystem::path ExportsDirectory(){return JobsDirectory()/"exports";}
std::filesystem::path SearchesDirectory(){return JobsDirectory()/"searches";}
std::filesystem::path PresetsDirectory(){return SearchesDirectory()/"presets";}
std::filesystem::path TraceProfilesDirectory(){return SearchesDirectory()/"trace-profiles";}
}
