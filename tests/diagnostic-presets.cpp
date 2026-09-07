#include "Generator/DiagnosticPreset.h"
#include <iostream>
#include <filesystem>
#include <fstream>
using nlohmann::json;
int main(int argc, char** argv) {
    int checks = 0;
    auto check = [&](const json& preset, bool valid, bool extended = false) {
        bool accepted = true;
        try { PS::InspectionTools::ValidatePreset(preset, extended); } catch (const std::exception&) { accepted = false; }
        if (accepted != valid) throw std::runtime_error("Unexpected preset validation result: " + preset.dump());
        ++checks;
    };
    check({{"Name","PlayerMagic"},{"IncludePlayer",true}},true);
    check({{"Name","AirRune"},{"Objects",{"/Game/Items/Rune.Rune"}}},true);
    check({{"Name","Inventory"},{"ControllerProperties",{"InventoryComponent"}}},true);
    check({{"Name","../escape"},{"IncludePlayer",true}},false);
    check({{"Name","x/y"},{"IncludePlayer",true}},false);
    check({{"Name","x\\y"},{"IncludePlayer",true}},false);
    check({{"Name","Empty"},{"Objects",json::array()}},false);
    check({{"Name","BadBool"},{"IncludePlayer","yes"}},false);
    check({{"Name","BadPath"},{"Objects",{"relative"}}},false);
    check({{"Name","Controls"},{"Objects",{std::string("/Game/a\0b",9)}}},false);
    check({{"Name","NoCode"},{"IncludePlayer",true},{"Command","anything"}},false);
    check({{"Name",std::string(65,'a')},{"IncludePlayer",true}},false);
    json many = json::array(); for (int i=0; i<65; ++i) many.push_back("/Game/A.A");
    check({{"Name","TooMany"},{"Objects",many}},false);
    check({{"Name","BadType"},{"Objects",{42}}},false);
    check(json::array(),false);
    const json focused={{"Name","Scope"},{"PropertyCaptures",json::array({{{"Root","Player"},{"Path",{"PlayerCombatMagicComponent","*"}}}})}};
    check(focused,true);
    auto selected=focused; selected["PropertyCaptures"][0]["Root"]="Selected"; check(selected,true);
    auto changed=focused;
    changed["CaptureLimits"]={{"MaxDepth",8},{"MaxEntries",512},{"MaxNodes",16384},{"MaxSparseSlots",16384},{"FollowObjectReferences",true}};
    check(changed,true);
    changed["CaptureLimits"]["MaxDepth"]=10; check(changed,true);
    changed["CaptureLimits"]["MaxDepth"]=11; check(changed,false); check(changed,true,true);
    changed["CaptureLimits"]["MaxDepth"]=16; check(changed,true,true); check(changed,false);
    changed["CaptureLimits"]["MaxDepth"]=17; check(changed,false,true);
    changed["CaptureLimits"]={{"MaxDepth",16},{"MaxNodes",16385}}; check(changed,false,true);
    changed=focused; changed["CaptureLimits"]={{"MaxNodes",-1}}; check(changed,false);
    changed=focused; changed["CaptureLimits"]={{"MaxEntries",0}}; check(changed,false);
    changed=focused; changed["CaptureLimits"]={{"FollowObjectReferences",1}}; check(changed,false);
    changed=focused; changed["CaptureLimits"]={{"ExecuteFunctions",true}}; check(changed,false);
    changed=focused; changed["PropertyCaptures"][0]["Path"]={"*","Mesh"}; check(changed,false);
    changed=focused; changed["PropertyCaptures"][0]["Path"]=json::array(); check(changed,false);
    changed=focused; changed["PropertyCaptures"][0]["Path"]={"GetActor()"}; check(changed,false);
    changed=focused; changed["PropertyCaptures"][0]["Root"]="relative"; check(changed,false);
    changed=focused; changed["PropertyCaptures"][0]["Write"]=true; check(changed,false);
    changed=focused; changed["PropertyCaptures"]=json::array(); check(changed,false);
    changed=focused; changed["PropertyCaptures"]=json::array();
    for(int i=0;i<33;++i) changed["PropertyCaptures"].push_back(focused["PropertyCaptures"][0]);
    check(changed,false);
    std::cout << checks << " preset validation checks passed\n";
    if (argc == 2) {
        unsigned count=0;
        for (const auto& entry:std::filesystem::directory_iterator(argv[1])) {
            if (entry.path().extension()!=".json") continue;
            if (entry.file_size()>16383) throw std::runtime_error("Oversized preset file");
            std::ifstream stream(entry.path());
            PS::InspectionTools::ValidatePreset(json::parse(stream));
            ++count;
        }
        std::cout<<count<<" actual preset files validated\n";
    }
}
