#include "Runtime/Storefront.h"
#include <cassert>
#include <iostream>

int main()
{
    using namespace PS::Storefront;
    const auto steam = DetectFrom(
        LR"(D:\SteamLibrary\steamapps\common\RSDragonwilds\RSDragonwilds\Binaries\Win64\RSDragonwilds-Win64-Shipping.exe)", false);
    assert(steam.Value == Kind::SteamEpic);
    assert(steam.SteamLauncher&&!steam.EpicLauncher);
    assert(std::string(PlatformName(steam))=="Steam");
    assert(std::string(Name(steam.Value))=="Steam/Epic");
    assert(!steam.DedicatedServer);

    const auto dedicated = DetectFrom(
        LR"(D:\SteamLibrary\steamapps\common\RSDragonwilds Dedicated Server\RSDragonwilds\Binaries\Win64\RSDragonwildsServer-Win64-Shipping.exe)", false);
    assert(dedicated.Value == Kind::SteamEpic);
    assert(dedicated.DedicatedServer);
    assert(std::string(PlatformName(dedicated))=="Dedicated server");

    const auto epic=DetectFrom(LR"(D:\Epic Games\RSDragonwilds\RSDragonwilds\Binaries\Win64\RSDragonwilds-Win64-Shipping.exe)",false);
    assert(epic.Value==Kind::SteamEpic&&epic.EpicLauncher&&!epic.SteamLauncher);
    assert(std::string(PlatformName(epic))=="Epic Games");
    const auto epicNamedPackage=DetectFrom(LR"(D:\EpicGames\RSDragonwilds\Binaries\WinGDK\RSDragonwilds-WinGDK-Shipping.exe)",false);
    assert(epicNamedPackage.Value==Kind::GamePass&&!epicNamedPackage.EpicLauncher);

    const auto fixture=std::filesystem::temp_directory_path()/(L"RuneSchema-epic-marker-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
    if(!std::filesystem::create_directory(fixture))throw std::runtime_error("Private storefront fixture already exists");
    std::filesystem::create_directory(fixture/L".egstore");
    std::filesystem::create_directories(fixture/L"CustomGame"/L"Binaries"/L"Win64");
    const auto custom=DetectFrom(fixture/L"CustomGame"/L"Binaries"/L"Win64"/L"RSDragonwilds-Win64-Shipping.exe",false);
    assert(custom.Value==Kind::SteamEpic&&custom.EpicLauncher&&!custom.SteamLauncher);
    std::filesystem::remove(fixture/L".egstore");
    std::filesystem::remove(fixture/L"CustomGame"/L"Binaries"/L"Win64");
    std::filesystem::remove(fixture/L"CustomGame"/L"Binaries");
    std::filesystem::remove(fixture/L"CustomGame");std::filesystem::remove(fixture);

    const auto gamePass = DetectFrom(
        LR"(D:\Steam Backups\RSDragonwilds\Binaries\WinGDK\RSDragonwilds-WinGDK-Shipping.exe)", false);
    assert(gamePass.Value == Kind::GamePass);
    assert(!gamePass.DedicatedServer);

    const auto packaged = DetectFrom(
        LR"(D:\Games\RSDragonwilds\Binaries\Win64\RSDragonwilds-Win64-Shipping.exe)", true);
    assert(packaged.Value == Kind::GamePass);

    assert(PlayerPlatform("Unknown",true,gamePass)=="Game Pass");
    assert(PlayerPlatform("Unknown",true,packaged)=="Game Pass");
    assert(PlayerPlatform("Unknown",true,steam)=="Steam");
    assert(PlayerPlatform("Unknown",true,epic)=="Epic");
    assert(PlayerPlatform("Unknown",false,gamePass)=="Unknown");
    assert(PlayerPlatform("Unknown",true,dedicated)=="Unknown");
    assert(PlayerPlatform("Unknown",true,Detection{})=="Unknown");
    assert(PlayerPlatform("PS5",true,gamePass)=="PS5");
    assert(PlayerPlatform("Steam",false,gamePass)=="Steam");

    std::cout << "Storefront lane selection passed.\n";
}
