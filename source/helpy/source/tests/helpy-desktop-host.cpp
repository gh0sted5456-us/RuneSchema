#define HELPY_DESKTOP_HOST_TEST 1
#include "../src/HelpyDesktopMain.cpp"
#include <cassert>
#include <iostream>
int main(int argc,char** argv)try{
    if(argc==2&&std::string_view(argv[1])=="--lifetime-child"){Sleep(30000);return 0;}
    wchar_t image[32768]{};GetModuleFileNameW(nullptr,image,32768);
    const auto root=std::filesystem::path(image).parent_path()/"ue4ss/Mods"/("DesktopHostFixture-"+std::to_string(GetCurrentProcessId()));
    assert(!std::filesystem::exists(root));std::filesystem::create_directories(root/"settings");
    struct Cleanup{std::filesystem::path Root;~Cleanup(){std::error_code error;std::filesystem::remove_all(Root,error);}} cleanup{root};
    AssignedRoot=root;OfflineEnvironment={{"ExternalDesktop",true}};
    PS::ConfigFiles::Write(root/"settings/settings.ini",PS::EncodeIniDocument(PS::SettingsDocument(PS::PSConfigSettings{})));
    auto before=PS::RuneSchemaSettingsEditor::Read(root/"settings");
    auto request=Json{{"Action","SettingsSave"},{"ExpectedContents",before.at("ExpectedContents")},{"Patch",{{"/helpyHotkey","F4"}}}};
    uint32_t size=0;auto input=request.dump();
    assert(Call("RuneSchema.Helpy","runeschema.tools",input.c_str(),nullptr,0,&size)==RS_PLUGIN_BUFFER_TOO_SMALL);
    assert(PS::RuneSchemaSettingsEditor::Read(root/"settings").at("ExpectedContents")==before.at("ExpectedContents"));
    std::string output(RUNESCHEMA_PLUGIN_MAX_MESSAGE,'\0');
    assert(Call("RuneSchema.Helpy","runeschema.tools",input.c_str(),output.data(),static_cast<uint32_t>(output.size()),&size)==RS_PLUGIN_OK);
    before=PS::RuneSchemaSettingsEditor::Read(root/"settings");assert(before.at("Document").at("helpyHotkey")=="F4");
    std::atomic<int> attempted=0;PS::HelpyDesktop::Server server;
    assert(server.Start(root,[&](const Json&){++attempted;Sleep(1000);return Json{{"Code",0}};},[]{}));
    assert(Bridge.Connect(root));request["ExpectedContents"]=before.at("ExpectedContents");request["Patch"]["/helpyHotkey"]="F6";input=request.dump();
    assert(Call("RuneSchema.Helpy","runeschema.tools",input.c_str(),output.data(),static_cast<uint32_t>(output.size()),&size)!=RS_PLUGIN_OK);
    assert(attempted==1&&PS::RuneSchemaSettingsEditor::Read(root/"settings").at("ExpectedContents")==before.at("ExpectedContents"));
    Bridge.Disconnect();server.Stop();
    {
        struct Child {PROCESS_INFORMATION Process{};~Child(){if(Process.hThread)CloseHandle(Process.hThread);if(Process.hProcess){if(WaitForSingleObject(Process.hProcess,0)==WAIT_TIMEOUT){TerminateProcess(Process.hProcess,0);WaitForSingleObject(Process.hProcess,1000);}CloseHandle(Process.hProcess);}}} child;
        STARTUPINFOW startup{sizeof(startup)};auto arguments=std::wstring(L"\"")+image+L"\" --lifetime-child";
        const auto need=[](bool value){if(!value)throw std::runtime_error("Game lifetime regression failed");};
        need(CreateProcessW(image,arguments.data(),nullptr,nullptr,FALSE,CREATE_SUSPENDED|CREATE_NO_WINDOW,nullptr,nullptr,&startup,&child.Process)!=FALSE);
        DWORD before=0,after=0;need(GetProcessHandleCount(GetCurrentProcess(),&before)!=FALSE);
        for(int cycle=0;cycle<50;++cycle){PS::HelpyGameLifetime watched;watched.Observe(child.Process.dwProcessId);need(watched.WaitHandle()!=nullptr&&!watched.Exited());
            const auto handle=watched.WaitHandle();watched.Observe(0);watched.Observe(child.Process.dwProcessId);need(watched.WaitHandle()==handle&&!watched.Exited());}
        need(GetProcessHandleCount(GetCurrentProcess(),&after)&&after==before);
        {PS::HelpyGameLifetime watched;need(!watched.Exited()&&!watched.WaitHandle());watched.Observe(child.Process.dwProcessId);
            need(TerminateProcess(child.Process.hProcess,0)&&WaitForSingleObject(child.Process.hProcess,5000)==WAIT_OBJECT_0);
            need(watched.Exited()&&!watched.Exited()&&!watched.WaitHandle());
            watched.Observe(GetCurrentProcessId());need(watched.WaitHandle()!=nullptr&&!watched.Exited());}
        need(GetProcessHandleCount(GetCurrentProcess(),&after)&&after==before);
        std::cout<<"Game exit follows the actual process, survives bridge disconnects and releases all 50 watcher handles.\n";
    }
    std::cout<<"Host probe is side-effect free; an unacknowledged live mutation is not retried offline.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
