#include "Core/HelpyVulkanSetup.h"
#include <stdexcept>
static void Check(bool value){if(!value)throw std::runtime_error("Vulkan startup enrollment regression");}
int main(){using namespace PS::HelpyVulkanSetup;using Json=nlohmann::json;
    const std::filesystem::path steam=L"C:/Games/Steam/RSDragonwilds-Win64-Shipping.exe",xbox=L"C:/Games/Game Pass/RSDragonwilds-WinGDK-Shipping.exe",bridge=L"C:/Games/RuneSchema.Helpy.dll";
    auto entries=Bind(Json::array(),steam,bridge,true);Check(entries.size()==1);entries=Bind(entries,xbox,bridge,true);Check(entries.size()==2);entries=Bind(entries,L"c:/games/steam/rsdragonwilds-win64-shipping.exe",bridge,true);Check(entries.size()==2);entries=Bind(entries,steam,bridge,false);Check(entries.size()==1&&entries.at(0).at("Executable")==Utf8(xbox));
    Check(Bind(entries,xbox,bridge,false).empty());bool refused=false;try{Bind(Json::object(),steam,bridge,true);}catch(...){refused=true;}Check(refused);refused=false;try{Bind(entries,L"relative.exe",bridge,true);}catch(...){refused=true;}Check(refused);Check(entries.size()==1);
}
