#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <cwctype>
#include <stdexcept>
#include <string>

namespace PS::HelpyGameProcess {
inline bool GameStopped(){
    const auto handle=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot verify whether Dragonwilds is closed");
    struct Close{HANDLE Value;~Close(){CloseHandle(Value);}}close{handle};PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);
    if(!Process32FirstW(handle,&entry))throw std::runtime_error("Cannot inspect active game processes");
    do{std::wstring name(entry.szExeFile);for(auto& c:name)c=towlower(c);if(name.starts_with(L"rsdragonwilds"))return false;}while(Process32NextW(handle,&entry));
    if(GetLastError()!=ERROR_NO_MORE_FILES)throw std::runtime_error("Game process inspection did not complete");return true;
}
}
