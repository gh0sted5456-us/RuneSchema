#include "Core/ProcessExitFuture.h"
#include <Windows.h>
#include <cassert>
#include <iostream>
#include <string>

int wmain(int argc,wchar_t** argv){
    if(argc==3&&std::wstring_view(argv[1])==L"--child"){
        const auto module=LoadLibraryW(argv[2]);if(!module)return 2;
        const auto start=reinterpret_cast<bool(*)()>(GetProcAddress(module,"BeginPendingBanner"));
        if(!start||!start())return 3;
        ExitProcess(0);
    }
    assert(argc==2);
    bool completed=false;
    {PS::ProcessExitFuture<void> work=std::async(std::launch::async,[&]{Sleep(30);completed=true;});}
    assert(completed);
    PS::ProcessExitFuture<int> first=std::async(std::launch::async,[]{return 17;});
    PS::ProcessExitFuture<int> second=std::move(first);assert(second.get()==17&&!first.valid());
    second={};
    wchar_t executable[32768]{};assert(GetModuleFileNameW(nullptr,executable,32768));
    std::wstring command=L"\""+std::wstring(executable)+L"\" --child \""+argv[1]+L"\"";
    STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION process{};
    assert(CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process));
    const auto waited=WaitForSingleObject(process.hProcess,5000);
    if(waited!=WAIT_OBJECT_0){TerminateProcess(process.hProcess,99);WaitForSingleObject(process.hProcess,2000);}
    DWORD code=99;GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hThread);CloseHandle(process.hProcess);
    if(waited!=WAIT_OBJECT_0||code!=0){std::cerr<<"Helpy's pending banner worker blocked process teardown\n";return 1;}
    std::cout<<"Pending banner work does not block DLL process teardown; normal destruction still waits\n";
}
