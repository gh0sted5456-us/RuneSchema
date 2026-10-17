#include "Loader/WearableBridgeDirective.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
using Json=nlohmann::json;
void Check(bool value){if(!value)throw std::runtime_error("Wearable schema contract failed");}
std::string Read(const char* path){std::ifstream file(path);if(!file)throw std::runtime_error("Missing contract input");std::ostringstream text;text<<file.rdbuf();return text.str();}
int main(int argc,char** argv){
    using PS::WearableBridgeDirective::Requested;
    Check(!Requested(Json::object()));Check(!Requested({{"$WearableBridge",false}}));Check(Requested({{"$WearableBridge",true}}));
    for(const auto& value:{Json("true"),Json(1),Json::object(),Json::array(),Json(nullptr)}){
        bool rejected=false;try{Requested({{"$WearableBridge",value}});}catch(const std::exception&){rejected=true;}Check(rejected);
    }
    Check(argc==4);const auto loader=Read(argv[1]),bridge=Read(argv[2]),clone=Read(argv[3]);
    const auto registration=loader.find("bool DragonWildsAssetModLoader::RegisterCreatedItem(");Check(registration!=std::string::npos);
    const auto verify=loader.find("if (!primaryVerified || !internalVerified)",registration);
    const auto enroll=loader.find("if(!networkDeferred)EquipmentWearableBridge::TryRegister(candidate,item);",registration);
    Check(verify!=std::string::npos&&enroll>verify);
    Check(loader.find("propertyName == \"$WearableBridge\"")!=std::string::npos);
    Check(loader.find("RefreshRegistered(object)")!=std::string::npos);
    Check(bridge.find("forwardValue->GetObjectPropertyValue(values.GetRawPtr(index))!=item")!=std::string::npos);
    Check(bridge.find("identityValue->GetObjectPropertyValue(stored)!=item")!=std::string::npos);
    Check(clone.find("AssetSearch::ItemMetadata(RC::to_string(iconSource->GetPathName()))")!=std::string::npos);
    Check(clone.find("{\"Icon\",actualIcon}")!=std::string::npos);
}
