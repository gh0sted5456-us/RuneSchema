#include <windows.h>
#include <appmodel.h>
#include <shellapi.h>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    int count=0;auto values=CommandLineToArgvW(GetCommandLineW(),&count);if(!values||count!=2)return 2;
    const std::filesystem::path output=values[1];LocalFree(values);UINT32 bytes=0;
    wchar_t inherited[128]{};GetEnvironmentVariableW(L"SteamGameId",inherited,128);
    std::ofstream(output)<<nlohmann::json{{"PackageResult",GetCurrentPackageFullName(&bytes,nullptr)},{"InheritedSteamContext",std::wstring_view(inherited)==L"RuneSchema-launch-test"},{"Pid",GetCurrentProcessId()}}.dump();return 0;
}
