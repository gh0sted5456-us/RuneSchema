#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const char* path) {
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error(std::string("Cannot read ")+path);
    return {std::istreambuf_iterator<char>(file),{}};
}

static void Require(bool value,const char* message) {
    if(!value)throw std::runtime_error(message);
}

int main(int argc,char** argv) {
    if(argc!=4)throw std::runtime_error("spawn loader, additional drops and settings panel sources required");
    const auto loader=Read(argv[1]);
    const auto drops=Read(argv[2]);
    const auto panel=Read(argv[3]);

    Require(loader.find("additionalDropType!=\"AISpawnPoint\" && additionalDropType!=\"Actor\"")!=std::string::npos,
        "ordinary Actor resources are not accepted by AdditionalDrops validation");
    Require(loader.find("ApplyAdditionalDrops(existing, spawn.AdditionalDrops)")!=std::string::npos,
        "existing resource actors do not receive additional drops");
    Require(loader.find("ApplyAdditionalDrops(actor, spawn.AdditionalDrops)")!=std::string::npos,
        "new resource actors do not receive additional drops");
    Require(loader.find("AdditionalDrops currently requires AISpawnPoint")==std::string::npos,
        "legacy AI-only AdditionalDrops gate returned");

    Require(drops.find("ItemDropOnDestructionComponent")!=std::string::npos,
        "resource drop routing no longer prefers destruction/depletion drops");
    Require(drops.find("ItemDropComponent")!=std::string::npos && drops.find("ItemDropOnSplitComponent")!=std::string::npos,
        "resource drop fallbacks are incomplete");
    Require(drops.find("ItemDataClass")!=std::string::npos && drops.find("MinToDrop")!=std::string::npos
        && drops.find("MaxToDrop")!=std::string::npos && drops.find("ProbabilityOfDrop")!=std::string::npos,
        "native resource ItemsToDrop contract is no longer recognized");
    Require(drops.find("drop.at(\"ChancePercent\").get<double>()/100.0")!=std::string::npos,
        "UI percentage is no longer converted to native resource probability");
    Require(drops.find("MaximumGrouping")!=std::string::npos,
        "resource maximum grouping safety field is not populated");
    Require(drops.find("materialize every entry before mutating the live")!=std::string::npos,
        "preflight-before-mutation contract was removed");

    Require(panel.find("Override additional drops (AI / resources)")!=std::string::npos,
        "Settings still presents AdditionalDrops as AI-only");

    std::cout<<"PASS: resource AdditionalDrops is accepted, routed through native ItemsToDrop, and exposed in Settings.\n";
}
