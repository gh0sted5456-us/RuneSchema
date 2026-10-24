#include "Runtime/HelpyWikiIcons.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
namespace PS::HostServices {static std::filesystem::path Root;std::filesystem::path CacheDirectory(){return Root;}}
int main(int argc,char** argv)try{
    if(argc<3)throw std::runtime_error("Use --download names.json cache-root [build] or --self-test cache-root");
    const bool download=std::string(argv[1])=="--download";if(download&&argc<4)throw std::runtime_error("Download requires a names file and cache root");PS::HostServices::Root=download?argv[3]:argv[2];
    ImGui::CreateContext();
    if(download){if(argc<4)throw std::runtime_error("Download requires a names file and cache root");std::ifstream input(argv[2]);const auto names=nlohmann::json::parse(input).get<std::vector<std::string>>();if(!PS::HelpyWikiIcons::Start(names,argc>4?argv[4]:""))throw std::runtime_error("Could not start wiki cache build");}
    else PS::HelpyWikiIcons::Initialize();
    std::string previous;while(PS::HelpyWikiIcons::Busy()){PS::HelpyWikiIcons::Poll();const auto status=PS::HelpyWikiIcons::Status();if(status!=previous){std::cout<<status<<std::endl;previous=status;}std::this_thread::sleep_for(std::chrono::milliseconds(100));}
    const auto count=PS::HelpyWikiIcons::Names().size();std::cout<<PS::HelpyWikiIcons::Status()<<"; entries="<<count<<std::endl;
    PS::HelpyWikiIcons::Shutdown();ImGui::DestroyContext();if(download&&!count)return 2;return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
