#include "Generator/HelpyStandaloneWindow.h"
#include "Runtime/HelpyPaths.h"
#include <shellapi.h>
#include <filesystem>
#include <string>
#include "Runtime/PluginRuntimeServices.h"
#include "Core/AchievementSources.h"
#include <cstring>
namespace {
int32_t RS_PLUGIN_CALL PreviewCall(const char*,const char* service,const char* input,char* output,uint32_t capacity,uint32_t* size){
    if(!size||!service||!input||(!output&&capacity))return RS_PLUGIN_INVALID_ARGUMENT;
    *size=0;if(std::string_view(service)!="helpy.achievements")return RS_PLUGIN_NOT_FOUND;
    try{
        const auto request=nlohmann::json::parse(input);
        if(request.value("Op",std::string{})!="discover")return RS_PLUGIN_INVALID_ARGUMENT;
        const auto reply=PS::AchievementSources::Discover({},request.value("RuneSchema",nlohmann::json::array()),false,request.value("IncludeDisabled",false)).dump();
        *size=uint32_t(reply.size()+1);if(capacity<*size)return RS_PLUGIN_BUFFER_TOO_SMALL;
        std::memcpy(output,reply.c_str(),*size);return RS_PLUGIN_OK;
    }catch(...){return RS_PLUGIN_INVALID_ARGUMENT;}
}
}
int WINAPI wWinMain(HINSTANCE module,HINSTANCE,PWSTR,int){
    int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    bool capture=false;int width=1280,height=900;
    std::filesystem::path output=std::filesystem::current_path()/"preview",assets;
    for(int i=1;i<argc;++i){
        const std::wstring argument=argv[i];
        if(argument==L"--capture"&&i+1<argc){capture=true;output=argv[++i];}
        else if(argument==L"--assets"&&i+1<argc)assets=argv[++i];
        else if(argument==L"--size"&&i+2<argc){width=std::stoi(argv[++i]);height=std::stoi(argv[++i]);}
    }
    LocalFree(argv);
    if(assets.empty())return 2;
    PS::HelpyPaths::Configure(assets);
    static RuneSchemaHostApi host{};host.StructSize=sizeof(host);host.ApiVersion=RUNESCHEMA_PLUGIN_API_VERSION;host.CallService=PreviewCall;PS::PluginRuntimeServices::Set(&host);
    PS::HelpyStandaloneWindow window(module);
    return window.Preview(output,capture,width,height);
}
