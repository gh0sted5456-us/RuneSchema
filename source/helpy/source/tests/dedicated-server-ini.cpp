#include "Core/DedicatedServerIni.h"
#include <cassert>
#include <fstream>
using namespace PS::DedicatedServerIni;
int main(){
    const std::string source="\xef\xbb\xbf; comment\r\n[Server]\r\nServerName = Example\r\nPassword=private#token\r\nUnknown=unchanged\r\n+AdminGuids=first\r\n+AdminGuids=second";
    Document document(source);assert(document.Serialize()==source&&!document.Dirty());
    for(auto& line:document.Lines)if(line.Key=="ServerName")line.Value=" New name";
    assert(document.Dirty());const auto changed=document.Serialize();assert(changed.find("ServerName = New name\r\n")!=changed.npos&&changed.ends_with("+AdminGuids=second"));
    assert(Sensitive("ServerPassword")&&Sensitive("APIKey")&&Sensitive("AuthToken")&&!Sensitive("ServerName"));
    assert(Writable("/Script/Dominion.DedicatedServerSettings","AdminPassword")&&!Writable("/Script/Dominion.DedicatedServerSettings","KnownPlayerList")&&!Writable("Other","OwnerId"));
    assert(Managed("+KnownPlayerList")&&Managed("AdminList")&&Managed("ServerGuid"));
    Document native("[/Script/Dominion.DedicatedServerSettings]\nServerName=Example\nKnownPlayerList=(LastAdminPassword=fixture)\n");native.Lines[1].Value="Reviewed";native.ValidateChanges();
    native.Lines[2].Value="not-allowed";bool managed=false;try{native.ValidateChanges();}catch(...){managed=true;}assert(managed);
    Document invalid("Name=ok\n");invalid.Lines[0].Value="bad\nOther=inject";bool rejected=false;try{(void)invalid.Serialize();}catch(...){rejected=true;}assert(rejected);
    const auto directory=std::filesystem::temp_directory_path()/("rsmk-server-ini-"+std::to_string(GetCurrentProcessId()))/"WindowsServer";std::filesystem::create_directories(directory);const auto file=directory/"DedicatedServer.ini";
    {std::ofstream stream(file,std::ios::binary);stream<<source;}
    assert(Read(file)==source);assert(!ValidPath(directory/"Other.ini"));
    bool stale=false;try{(void)Save(file,"stale",changed);}catch(...){stale=true;}assert(stale&&Read(file)==source);
    const auto backup=Save(file,source,changed);assert(Read(file)==changed);
    {std::ifstream stream(backup,std::ios::binary);const std::string bytes((std::istreambuf_iterator<char>(stream)),{});assert(bytes==source);}
    // An active writer prevents a save instead of being overwritten.
    const auto writer=CreateFileW(file.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);assert(writer!=INVALID_HANDLE_VALUE);
    bool busy=false;try{(void)Save(file,changed,source);}catch(...){busy=true;}assert(busy);CloseHandle(writer);assert(Read(file)==changed);
    std::filesystem::remove(backup);std::filesystem::remove(file);std::filesystem::remove(directory);std::filesystem::remove(directory.parent_path());
}
