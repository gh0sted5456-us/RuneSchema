#include "Core/JournalPlacement.h"
#include <iostream>
using nlohmann::json;
using PS::JournalPlacement::Parse;
static void Require(bool value){if(!value)throw std::runtime_error("Journal placement test failed");}
static void Reject(const json& value){bool failed=false;try{Parse(value,"entry");}catch(const std::exception&){failed=true;}Require(failed);}
int main() {
    auto plain=Parse({{"SubCategory","/Game/UI/Category.Category"}},"entry");
    Require(!plain.TargetGroup && plain.Key=="entry");
    auto lore=Parse({{"Section","Knowledge"},{"Category","Lore Scraps"}},"lore");
    Require(lore.SubCategory=="/Game/UI/JournalData/JOURNAL_SC_Know_LoreScraps.JOURNAL_SC_Know_LoreScraps");
    auto recipe=Parse({{"Section","recipes"},{"Category","ammo & runes"}},"rune");
    Require(recipe.SubCategory=="/Game/UI/JournalData/JOURNAL_SC_Recipe_Ammo_Runes.JOURNAL_SC_Recipe_Ammo_Runes");
    auto existing=Parse({{"SubCategory","Processed Materials"},{"Group",{{"Id","group_cloth"}}}},"entry");
    Require(existing.TargetGroup && !existing.TargetGroup->CreateIfMissing);
    auto custom=Parse({{"SubCategory","Processed Materials"},{"Group",{{"Id","my_mod_materials"},{"DisplayName","Enchanted Materials"},{"CreateIfMissing",true}}}},"entry");
    Require(custom.TargetGroup->CreateIfMissing && custom.TargetGroup->DisplayName=="Enchanted Materials");
    Reject({{"SubCategory"," "}});
    Reject({{"Section","Knowledge"}});
    Reject({{"Section","Knowledge"},{"Category","Unknown"}});
    Reject({{"SubCategory","test"},{"Section","Knowledge"},{"Category","People"}});
    Reject({{"SubCategory","test"},{"Group","cloth"}});
    Reject({{"SubCategory","test"},{"Group",{{"Id","cloth"},{"CreateIfMissing",true}}}});
    Reject({{"SubCategory","test"},{"Group",{{"Id","cloth"},{"CreateIfMissing","true"}}}});
    Reject({{"SubCategory","test"},{"Group",{{"Id","cloth"},{"Displayname","typo"}}}});
    Reject({{"SubCategory","test"},{"Biome","old"}});
    std::cout<<"PASS: hierarchical journal placement schema and invalid input rejection.\n";
}
