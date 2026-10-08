#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    if(argc!=3)throw std::runtime_error("PlayerGhost source and weak handle required");
    std::ifstream file(argv[1]);
    if(!file)throw std::runtime_error("Cannot read PlayerGhost source");
    const std::string source{std::istreambuf_iterator<char>(file), {}};
    const auto require=[](bool value){if(!value)throw std::runtime_error("Preview discovery regression");};
    const auto start=source.find("previewTickCallback=Hook::RegisterEngineTickPostCallback");
    const auto end=source.find("// Declare after owners",start);
    require(start!=std::string::npos && end!=std::string::npos);
    const auto callback=source.substr(start,end-start);
    require(callback.find("if(!preview)preview=UECustom::UObjectGlobals::StaticFindObject")!=std::string::npos);
    require(callback.find("/Game/Maps/L_FrontEnd.L_FrontEnd:PersistentLevel.BP_PlayerCharacterPreview_C_1")!=std::string::npos);
    require(source.find("previewDiscoveryPending")==std::string::npos);
    require(callback.find("TrackPreview(preview)")!=std::string::npos);
    require(source.find("const auto items=PreviewItems(player);")!=std::string::npos);
    require(source.find("previewItems")==std::string::npos);
    require(source.find("AppearanceEvents::Subscribe(AppearanceEvents::Consumer::Ghost,Dirty)")!=std::string::npos);
    require(source.find("Hook::RegisterBeginPlayPostCallback")!=std::string::npos);
    const auto ref=source.substr(source.find("UObject* Ref("),source.find("struct Lease")-source.find("UObject* Ref("));
    require(ref.find("CastField<FObjectPropertyBase>")!=std::string::npos);
    require(ref.find("GetPropertiesSize()")!=std::string::npos);
    require(ref.find("GetElementSize()==sizeof(UObject*)")==std::string::npos);
    std::ifstream weakFile(argv[2]);
    if(!weakFile)throw std::runtime_error("Cannot read weak handle source");
    const std::string weak{std::istreambuf_iterator<char>(weakFile), {}};
    require(weak.find("class WeakObjectHandle")!=std::string::npos);
    require(weak.find("RC::Unreal::FWeakObjectPtr")==std::string::npos);
    require(source.find("FWeakObjectPtr")==std::string::npos);
    std::cout<<"PASS: production source retains late-arrival lookup, live item refresh and native hook subscription.\n";
}
