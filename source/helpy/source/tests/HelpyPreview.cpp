#include "Generator/HelpyStandaloneWindow.h"
#include "Runtime/HelpyPaths.h"
#include <shellapi.h>
#include <filesystem>
#include <string>
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
    PS::HelpyStandaloneWindow window(module);
    return window.Preview(output,capture,width,height);
}
