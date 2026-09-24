#include "Runtime/Storefront.h"
#include <cassert>
#include <iostream>

int main()
{
    using namespace PS::Storefront;
    const auto steam = DetectFrom(
        LR"(D:\SteamLibrary\steamapps\common\RSDragonwilds\RSDragonwilds\Binaries\Win64\RSDragonwilds-Win64-Shipping.exe)", false);
    assert(steam.Value == Kind::SteamGog);

    const auto gamePass = DetectFrom(
        LR"(D:\Steam Backups\RSDragonwilds\Binaries\WinGDK\RSDragonwilds-WinGDK-Shipping.exe)", false);
    assert(gamePass.Value == Kind::GamePass);

    const auto packaged = DetectFrom(
        LR"(D:\Games\RSDragonwilds\Binaries\Win64\RSDragonwilds-Win64-Shipping.exe)", true);
    assert(packaged.Value == Kind::GamePass);

    std::cout << "Storefront lane selection passed.\n";
}
