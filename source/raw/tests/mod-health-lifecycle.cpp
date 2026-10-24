#include "Runtime/ModHealthService.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <cstdlib>

namespace { std::filesystem::path Root; }
namespace PS::HostServices { std::filesystem::path ModDirectory(){return Root;} }
static void Start(){
    PS::ModHealthService::Initialize();
    for(unsigned attempt=0;attempt<1000;++attempt){
        const auto snapshot=PS::ModHealthService::Read();
        if(snapshot.contains("File"))return;
        if(snapshot.contains("StorageError"))throw std::runtime_error(snapshot.at("StorageError").get<std::string>());
        Sleep(5);
    }
    throw std::runtime_error("Mod-health worker failed to publish a snapshot");
}
int wmain(int argc,wchar_t** argv){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    try{
        if(argc!=2&&argc!=3)throw std::runtime_error("Expected a private fixture directory");
        const bool child=argc==3&&std::wstring_view(argv[1])==L"--exit-with-monitor";
        const auto fixture=std::filesystem::absolute(argv[argc-1]);
        const auto local=fixture/L"LocalAppData";Root=fixture/L"RuneSchema";
        std::filesystem::create_directories(Root/L"mods");std::filesystem::create_directories(local);
        if(!SetEnvironmentVariableW(L"LOCALAPPDATA",local.c_str()))throw std::runtime_error("Private AppData unavailable");
        if(child){Start();return 0;} // The engine can exit without calling uninstall_mod.
        Start();PS::ModHealthService::Shutdown();
        DWORD before=0,after=0;if(!GetProcessHandleCount(GetCurrentProcess(),&before))throw std::runtime_error("Handle-count check unavailable");
        for(int i=0;i<10;++i){Start();Start();PS::ModHealthService::Shutdown();PS::ModHealthService::Shutdown();}
        if(!GetProcessHandleCount(GetCurrentProcess(),&after)||after!=before)throw std::runtime_error("Mod-health restart leaked a thread or file handle");
        wchar_t executable[32768]{};if(!GetModuleFileNameW(nullptr,executable,32768))throw std::runtime_error("Test executable unavailable");
        auto command=L"\""+std::wstring(executable)+L"\" --exit-with-monitor \""+fixture.wstring()+L"\"";
        STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION process{};
        if(!CreateProcessW(executable,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process))throw std::runtime_error("Cannot start exit regression child");
        CloseHandle(process.hThread);const auto wait=WaitForSingleObject(process.hProcess,10000);
        if(wait!=WAIT_OBJECT_0){TerminateProcess(process.hProcess,2);WaitForSingleObject(process.hProcess,5000);CloseHandle(process.hProcess);throw std::runtime_error("Exit regression child hung");}
        DWORD code=1;const auto read=GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hProcess);
        if(!read||code){std::cerr<<"Active mod-health monitor failed process exit: 0x"<<std::hex<<code<<"\n";return 1;}
        std::cout<<"Mod-health restart, repeated shutdown and active-monitor process exit passed\n";return 0;
    }catch(const std::exception& error){PS::ModHealthService::Shutdown();std::cerr<<error.what()<<"\n";return 1;}
}
