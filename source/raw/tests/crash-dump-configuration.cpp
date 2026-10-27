#include "Core/CrashDumpConfiguration.h"
#include <chrono>
#include <iostream>
#define Need(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
int main()try{
    namespace C=PS::CrashDumpConfiguration;namespace F=PS::ConfigFiles;using Json=nlohmann::json;
    const auto root=std::filesystem::temp_directory_path()/("rs-crash-options-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto file=root/"UE4SS-settings.ini";
    const std::string original="; untouched\r\n[CrashDump]\r\nEnableDumping = 0 ; preserve comment\r\nFullMemoryDump = 0\r\n[Debug]\r\nEnabled = 1\r\n";
    F::Write(file,original);auto before=C::Read(root);Need(before.at("Ok")==true&&!before.at("FullMemoryDump").get<bool>());
    const auto save=[&](const Json& full,const Json& expected){return C::Save(root,{{"FullMemoryDump",full},{"ExpectedContents",expected}});};
    auto on=save(true,original);Need(on.at("Ok")==true&&on.at("FullMemoryDump")==true&&on.at("EnableDumping")==true&&on.at("RestartRequired")==true);
    Need(F::Read(on.at("Backup").get<std::string>())==original);
    const auto enabled=F::Read(file);Need(enabled=="; untouched\r\n[CrashDump]\r\nEnableDumping = 1 ; preserve comment\r\nFullMemoryDump = 1\r\n[Debug]\r\nEnabled = 1\r\n");
    Need(save(false,original).at("Ok")==false&&F::Read(file)==enabled);
    Need(save("true",enabled).at("Ok")==false&&F::Read(file)==enabled);
    auto off=save(false,enabled);Need(off.at("Ok")==true&&off.at("FullMemoryDump")==false&&off.at("EnableDumping")==true);
    Need(F::Read(file)==C::Encode(enabled,false));
    for(const std::string text:{"[Other]\nValue=kept","[CrashDump]\n","[CrashDump]\n; empty\n[Other]\nValue=kept\n"}){
        const auto encoded=C::Encode(text,true);const auto parsed=C::Parse(encoded);Need(parsed.Full.Value&&parsed.Dumping.Value);
        Need(encoded.find("Value=kept")!=encoded.npos||text.find("Value=kept")==text.npos);
    }
    Need(!C::Parse("[Other]\nFullMemoryDump=1\n").Full.Value);
    Need(C::Parse("\xef\xbb\xbf[CrashDump]\nFullMemoryDump=true # comment\n").Full.Value);
    for(const std::string text:{"[CrashDump]\nFullMemoryDump=1\nFullMemoryDump=0\n","[CrashDump]\n[CrashDump]\n","[CrashDump]\nFullMemoryDump=maybe\n"}){
        F::Write(file,text);Need(C::Read(root).at("Ok")==false&&save(true,text).at("Ok")==false&&F::Read(file)==text);
    }
    F::Write(file,std::string(C::MaxBytes+1,' '));Need(C::Read(root).at("Ok")==false);
    std::filesystem::remove_all(root);std::cout<<"Crash capture toggle, backups, independent options, restart semantics and stale-write guards passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
