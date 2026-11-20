#include "RuntimePaths.h"
#include "Runtime/HelpyPaths.h"
#include "Runtime/HostServices.h"
#include "Core/HelpyUiAssets.h"
#include "Core/RuneSchemaDataRoot.h"
#include "Core/HelpyEnvironment.h"
#include "Core/ConfigFiles.h"
#include <cwctype>
#include <string_view>
#include <windows.h>
#include <stdexcept>
#include <vector>

namespace {
std::filesystem::path PluginDirectory;
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
        const auto folder=RuneSchemaData::SharedDirectory("HELPY")/"ui-assets"/HelpyUiAssets::Version;
        const auto candidate=folder/name;
        std::error_code error;
        if(std::filesystem::is_regular_file(folder/"ready",error)&&std::filesystem::is_regular_file(candidate,error))return candidate;
    }catch(...){}
    return PluginRoot()/"assets"/name;
}
namespace {
std::filesystem::path Owned(const char* name){
    const auto folder=RuneSchemaData::SharedDirectory("HELPY")/name;RuneSchemaData::Safe(folder);
    std::error_code error;std::filesystem::create_directories(folder,error);return folder;
}
// One-time copy of user files from the old mod-side folder. Existing AppData files always win.
void Adopt(const std::filesystem::path& from,const std::filesystem::path& to,bool (*wanted)(const std::filesystem::path&)){
    std::error_code error;if(!std::filesystem::is_directory(from,error)||(GetFileAttributesW(from.c_str())&FILE_ATTRIBUTE_REPARSE_POINT))return;
    size_t count=0;for(std::filesystem::directory_iterator it(from,error),end;!error&&it!=end&&++count<=512;it.increment(error)){
        if(!it->is_regular_file(error)||(GetFileAttributesW(it->path().c_str())&FILE_ATTRIBUTE_REPARSE_POINT)||!wanted(it->path()))continue;
        std::error_code ignored;if(it->file_size(ignored)>20*1024*1024)continue;
        std::filesystem::copy_file(it->path(),to/it->path().filename(),std::filesystem::copy_options::skip_existing,ignored);
    }
}
std::wstring Lower(std::wstring text){for(auto& c:text)c=static_cast<wchar_t>(towlower(c));return text;}
}
std::filesystem::path BackgroundsFolder(){
    static const auto folder=[]{const auto result=Owned("backgrounds");
        // Older installs kept user pictures in the mod's Helpy/assets/backgrounds. The two shipped pictures are
        // managed artwork and stay out of the copy.
        try{Adopt(PluginRoot()/"assets"/"backgrounds",result,[](const std::filesystem::path& file){
            const auto name=Lower(file.filename().wstring()),extension=Lower(file.extension().wstring());
            return name!=L"runeschema.png"&&name!=L"dragonwilds.png"&&(extension==L".png"||extension==L".jpg"||extension==L".jpeg"||extension==L".bmp"||extension==L".webp");});}catch(...){}
        return result;}();
    return folder;
}
std::filesystem::path FontsFolder(){
    static const auto folder=[]{const auto result=Owned("fonts");
        try{Adopt(PluginRoot()/"assets"/"fonts",result,[](const std::filesystem::path& file){const auto extension=Lower(file.extension().wstring());return extension==L".ttf"||extension==L".otf";});}catch(...){}
        return result;}();
    return folder;
}
std::filesystem::path BuiltInScript(int id,const char* name){
    const auto resource=FindResourceW(nullptr,MAKEINTRESOURCEW(id),MAKEINTRESOURCEW(10)); // RT_RCDATA
    const auto bytes=resource?SizeofResource(nullptr,resource):0;const auto loaded=resource?LoadResource(nullptr,resource):nullptr;
    const auto data=loaded?static_cast<const char*>(LockResource(loaded)):nullptr;
    if(!data||!bytes)throw std::runtime_error("This script is built into Helpy.exe and is missing. Update Helpy.");
    const auto file=Owned("tools")/name;const std::string_view text(data,bytes);
    std::string current;try{current=ConfigFiles::Read(file,4*1024*1024);}catch(...){}
    if(current!=text)ConfigFiles::Write(file,text);
    return file;
}
void CacheLicenses(){
    try{
        const auto source=HostServices::ModDirectory()/"settings"/"licenses"/"RuneSchema-Licenses.txt";
        std::error_code error;if(!std::filesystem::is_regular_file(source,error))return;
        const auto text=ConfigFiles::Read(source,16*1024*1024);
        const auto folder=RuneSchemaData::Directory()/"core"/"licenses";RuneSchemaData::Safe(folder);std::filesystem::create_directories(folder,error);
        const auto copy=folder/"RuneSchema-Licenses.txt";std::string current;try{current=ConfigFiles::Read(copy,16*1024*1024);}catch(...){}
        if(current!=text)ConfigFiles::Write(copy,text);
    }catch(...){}
}
}
namespace PS::HostServices {
std::filesystem::path ModDirectory(){return RuntimePaths::ModFromHelpy(HelpyPaths::PluginRoot());}
std::filesystem::path WorkingDirectory(){return ModDirectory().parent_path().parent_path();}
std::filesystem::path SettingsDirectory(){return ModDirectory()/"settings";}
// Helpy-owned mutable data stays writable even in a protected game install.
std::filesystem::path StateDirectory(){return RuneSchemaData::SharedDirectory("HELPY");}
std::filesystem::path ProfileDirectory(){return RuneSchemaData::SharedDirectory("runeschema.profile");}
std::filesystem::path RuntimeDirectory(){return StateDirectory()/"runtime";}
std::filesystem::path SavedDirectory(){return RuntimeDirectory()/"saved";}
std::filesystem::path CacheDirectory(){
    using enum HelpyEnvironment::Kind;
    const auto kind=HelpyEnvironment::Current;
    const char* platform=kind==GamePass?"gamepass":kind==Dedicated?"server":kind==Epic?"epic":kind==Steam||kind==SteamEpic?"steam":"unknown";
    return RuneSchemaData::PlatformDirectory(platform)/"cache";
}
std::filesystem::path ProgressDirectory(){return SavedDirectory()/"progress";}
std::filesystem::path ReferencesDirectory(){return SavedDirectory()/"references";}
std::filesystem::path JobsDirectory(){return RuntimeDirectory()/"jobs";}
std::filesystem::path ExportsDirectory(){return JobsDirectory()/"exports";}
std::filesystem::path SearchesDirectory(){return JobsDirectory()/"searches";}
std::filesystem::path PresetsDirectory(){return SearchesDirectory()/"presets";}
std::filesystem::path TraceProfilesDirectory(){return SearchesDirectory()/"trace-profiles";}
}
