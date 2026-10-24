#include "Core/RegistrationDiagnostics.h"
#include "Core/ItemCategory.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
static std::string Read(const char* path){std::ifstream file(path);assert(file);return {std::istreambuf_iterator<char>(file),{}};}
int main(int argc,char** argv){
    using namespace PS::RegistrationDiagnostics;
    assert(Source("/ColorsOfMoney/Armor/A.A")=="ColorsOfMoney");
    assert(Source("/Game/Mods/Conjurer/Items/A.A")=="Game/Mods/Conjurer");
    assert(Source("/Game/RuneSchema/MaxCape/Items/maxcape.maxcape")=="Game/RuneSchema/MaxCape");
    assert(Source("/Game/Gameplay/Items/A.A")=="Game");
    assert(Source("bad")=="<unknown>");
    Sources counts;
    for(int index=0;index<40000;++index)counts.Add("/ColorsOfMoney/Armor/Item"+std::to_string(index),index<35000);
    assert(counts.Rows().size()==1&&counts.Rows().at("ColorsOfMoney").Added==35000&&counts.Rows().at("ColorsOfMoney").Existing==5000);
    const auto ready=counts.Take();assert(ready.size()==1&&counts.Rows().empty());
    for(int index=0;index<1000;++index)counts.Add("/Root"+std::to_string(index)+"/Item.Item",true);
    std::size_t total=0;for(const auto& [_,row]:counts.Rows())total+=row.Added;
    assert(total==1000&&counts.Rows().size()<=129);
    assert(argc==4);const auto wearable=Read(argv[1]),registrar=Read(argv[2]),catalogue=Read(argv[3]);
    assert(wearable.find("[DISCOVERABLE]")==std::string::npos);
    assert(wearable.find("[SERVER-DATA][ITEM]")==std::string::npos);
    assert(wearable.find("Registered.find(item)")!=std::string::npos&&wearable.find("PendingDiagnostics.Add")!=std::string::npos);
    assert(wearable.find("IsDebugLoggingEnabled()")!=std::string::npos&&wearable.find("[WEARABLE-BRIDGE][SUMMARY]")!=std::string::npos);
    assert(wearable.find("[WEARABLE-BRIDGE][INACTIVE]")!=std::string::npos&&wearable.find("[WEARABLE-BRIDGE][NOT-ENROLLED]")!=std::string::npos);
    assert(registrar.find("[REGISTRY][{}][IDENTITY]")==std::string::npos&&registrar.find("[SOURCE-SUMMARY]")!=std::string::npos);
    assert(registrar.find("EquipmentWearableBridge::ReportDiagnostics();")!=std::string::npos);
    for(const auto* phase:{"discovery","registration","confirmation","cleanup"})
        assert(registrar.find(std::string("data registrar ")+phase+" complete")!=std::string::npos);
    assert(registrar.find("[PERF][REGISTRY] discovery_ms={}")!=std::string::npos);
    const auto roster=catalogue.substr(catalogue.find("json ItemRoster()"),catalogue.find("json Capture(")-catalogue.find("json ItemRoster()"));
    assert(roster.find("objects.Num()>32768")==std::string::npos&&catalogue.find("AssetProvenance::LivePaths()")!=std::string::npos);
    assert(catalogue.find("segment==1&&prioritySet.contains(path)")!=std::string::npos);
    assert(catalogue.find("ItemCategory::Classify(path,name,internal,tags,classes)")!=std::string::npos);
    assert(PS::ItemCategory::Classify("/Game/Mods/RuneSchema/ITEM_Cape.ITEM_Cape","Max Cape","","","")=="Armor");
    assert(PS::ItemCategory::Classify("/Game/Mods/Test.Item","Unknown","","","WearableEquipmentData ItemData")=="Armor");
    assert(PS::ItemCategory::Classify("/Game/Mods/Runescape/Test.Test","Unknown","","","").empty());
}
