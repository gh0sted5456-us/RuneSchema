#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
int main(int argc,char** argv){
    if(argc!=3)return 2;
    nlohmann::json result={{"files",nlohmann::json::array()},{"errors",nlohmann::json::array()}};
    for(auto& entry:std::filesystem::recursive_directory_iterator(argv[1])){
        if(!entry.is_regular_file())continue;
        auto ext=entry.path().extension();if(ext!=".json"&&ext!=".jsonc")continue;
        try{std::ifstream f(entry.path());auto data=nlohmann::json::parse(f,nullptr,true,true);
            result["files"].push_back({{"path",entry.path().generic_string()},{"rootType",data.type_name()}});
        }catch(const std::exception& e){result["errors"].push_back({{"path",entry.path().generic_string()},{"error",e.what()}});}
    }
    std::ofstream(argv[2])<<result.dump(2);
    std::cout<<"Parsed "<<result["files"].size()<<" installed JSON/JSONC files; "<<result["errors"].size()<<" errors.\n";
    return result["errors"].empty()?0:1;
}
