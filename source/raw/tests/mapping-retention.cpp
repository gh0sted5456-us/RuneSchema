#include "Core/MappingRetention.h"
#include <cassert>
#include <chrono>
#include <fstream>
using namespace PS::MappingRetention;
int main(){const Json old={{"Build","0123456789abcdef"},{"Fingerprint","1111111111111111"}},fresh={{"Build","1123456789abcdef"},{"Fingerprint","2222222222222222"}};
    const auto retired=Retired(old,fresh);assert(retired.size()==1);assert(File("C:/cache/mappings/automatic",retired[0]).generic_string()=="C:/cache/mappings/automatic/0123456789abcdef/1111111111111111.usmap");assert(Retired(fresh,fresh).empty());
    auto malformed=old;malformed["Build"]="../../";assert(Retired(malformed,fresh).empty());malformed=old;malformed["Fingerprint"]="C:/Mappings";assert(Retired(malformed,fresh).empty());
    auto chain=old;chain["Retired"]=Json::array({old,fresh,{{"Build","0123456789abcdef"},{"Fingerprint","3333333333333333"}}});assert(Retired(chain,fresh).size()==2);
    const auto root=fs::temp_directory_path()/("mapping-retention-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));const auto oldFile=File(root,old),newFile=File(root,fresh);fs::create_directories(oldFile.parent_path());fs::create_directories(newFile.parent_path());{std::ofstream f(oldFile);f<<"old";}{std::ofstream f(newFile);f<<"new";}const auto unrelated=root/"custom.usmap";{std::ofstream f(unrelated);f<<"user provided";}
    auto receipt=fresh;receipt["Retired"]=Json::array({old,fresh,{{"Build","../../"},{"Fingerprint","1111111111111111"}}});bool failed=false;try{RemoveRetired(root,receipt,[](auto&,auto&){throw std::runtime_error("bad replacement");},[](auto&){});}catch(...){failed=true;}assert(failed&&fs::exists(oldFile)&&fs::exists(newFile));
    assert(RemoveRetired(root,receipt,[&](const auto& file,const auto& fingerprint){assert(file==newFile&&fingerprint==fresh["Fingerprint"].get<std::string>());},[](auto&){})==1);assert(!fs::exists(oldFile)&&fs::exists(newFile)&&fs::exists(unrelated));assert(RemoveRetired(root,receipt,[](auto&,auto&){},[](auto&){})==0);
    fs::remove(newFile);fs::remove(unrelated);fs::remove(oldFile.parent_path());fs::remove(newFile.parent_path());fs::remove(root);
}
