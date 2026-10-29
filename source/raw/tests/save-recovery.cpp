#include "Core/SaveRecovery.h"
#include "Core/SavePruneBackup.h"
#include <iostream>
#include <cassert>
#undef assert
#define assert(value) ((value)?static_cast<void>(0):throw std::runtime_error(#value))
int main()try{
    namespace R=PS::SaveRecovery;namespace S=PS::SaveCleanup;using J=nlohmann::json;
    PS::RegistryEnrollment::Reset();S::RegistrySnapshot registry;
    for(int i=0;i<500;++i)registry.Items.insert("NativeItem"+std::to_string(i));
    for(int i=0;i<300;++i)registry.Recipes.insert("NativeRecipe"+std::to_string(i));
    registry.Items.insert("ModItem");registry.Recipes.insert("ModRecipe");registry.Quests={"ModQuest"};registry.Journals={"ModJournal"};registry.QuestsComplete=true; registry.QuestCompletedState=2;registry.JournalsComplete=true;
    for(const auto* type:{"Item","Recipe","Quest","Journal"})registry.Bindings[std::string(type)+":Mod"+type]={std::string("/Example/Mod")+type+".Mod"+type,"/Script/Dominion.Data","mods/example"};
    const J source={{"meta_data",{{"char_guid","test-character"}}},{"GameProgress",{
        {"Inventory",{{"0",{{"ItemData","ModItem"},{"Count",7}}},{"1",{{"ItemData","NativeItem1"},{"Count",3}}}}},
        {"PersonalInventory",J::object()},{"Loadout",J::object()},
        {"Progress",{{"ItemsPickedUp",{"ModItem","NativeItem1"}},{"RecipesUnlocked",{"ModRecipe","NativeRecipe1"}},{"RecipesNew",{"ModRecipe"}}}},
        {"QuestProgress",{{"Quests",J::array({{{"QuestId","ModQuest"},{"QuestInts",J::array({{{"QuestVariableName","Progress"},{"QuestVariableValue",9}}})}}})},{"QuestTracked","ModQuest"},{"QuestLocations",J::array()}}},
        {"Journal",{{"UnlockedEntries",{"ModJournal"}},{"UnreadEntries",{"ModJournal"}}}}
    }}};
    const auto initial=R::Prepare(source,source,R::New("steam",R::Character(source)),registry,"steam");assert(initial.Archive["Bindings"].size()==4&&initial.Captured==0&&initial.Restored==0);
    auto absent=registry;absent.Items.erase("ModItem");absent.Recipes.erase("ModRecipe");absent.Quests.clear();absent.Journals.clear();absent.Bindings.clear();
    const auto cleanup=S::Plan(source,{},false,&absent,false,true,true);
    auto captured=R::Prepare(source,cleanup.Save,initial.Archive,absent,"steam");assert(captured.Captured==8&&captured.Restored==0);
    assert(captured.Archive["Records"].size()==8&&captured.Save==cleanup.Save);
    auto restored=R::Prepare(cleanup.Save,cleanup.Save,captured.Archive,registry,"steam");assert(restored.Restored>=7);
    assert(restored.Save["GameProgress"]["Inventory"]["0"]==source["GameProgress"]["Inventory"]["0"]);
    assert(restored.Save["GameProgress"]["Progress"]["RecipesUnlocked"].size()==2);
    assert(restored.Save["GameProgress"]["QuestProgress"]["Quests"][0]["QuestInts"][0]["QuestVariableValue"]==9);
    // Claimed before native writeback: a failed native load cannot repeatedly mint archived items.
    auto replay=R::Prepare(cleanup.Save,cleanup.Save,restored.Archive,registry,"steam");assert(replay.Restored<=1&&replay.Save["GameProgress"]["Inventory"].size()==1);
    const auto replayAgain=R::Prepare(cleanup.Save,cleanup.Save,replay.Archive,registry,"steam");assert(replayAgain.Restored==0);
    auto occupied=cleanup.Save;occupied["GameProgress"]["Inventory"]["0"]={{"ItemData","NativeItem2"},{"Count",1}};
    const auto blocked=R::Prepare(occupied,occupied,captured.Archive,registry,"steam");assert(blocked.Save["GameProgress"]["Inventory"]["0"]["ItemData"]=="NativeItem2"&&blocked.Blocked>0);
    auto reassigned=registry;for(auto& [key,binding]:reassigned.Bindings)binding.Asset="/Different/Asset.Asset";
    assert(R::Prepare(cleanup.Save,cleanup.Save,captured.Archive,reassigned,"steam").Restored==0);
    auto variant=source;variant["GameProgress"]["Inventory"]["0"]["Count"]=11;
    auto variants=R::Prepare(variant,cleanup.Save,captured.Archive,absent,"steam");
    const auto oneSlot=R::Prepare(cleanup.Save,cleanup.Save,variants.Archive,registry,"steam");
    assert(oneSlot.Blocked>0&&oneSlot.Save["GameProgress"]["Inventory"].size()==2);
    size_t claimedItems=0;for(const auto& row:oneSlot.Archive["Records"])if(row["Kind"]=="Inventory"&&row["State"]=="ClaimedForNativeLoad")++claimedItems;assert(claimedItems==1);
    auto equipped=source;equipped["GameProgress"]["Loadout"]["Weapon"]={{"PlayerInventoryItemIndex",0}};
    auto unequipped=cleanup.Save;const auto equipmentArchive=R::Prepare(equipped,unequipped,initial.Archive,absent,"steam");
    assert(R::Prepare(unequipped,unequipped,equipmentArchive.Archive,registry,"steam").Save["GameProgress"]["Loadout"].contains("Weapon"));
    auto replacement=unequipped;replacement["GameProgress"]["Inventory"]["0"]={{"ItemData","ModItem"},{"Count",1}};
    assert(!R::Prepare(replacement,replacement,equipmentArchive.Archive,registry,"steam").Save["GameProgress"]["Loadout"].contains("Weapon"));
    auto appearance=source;appearance["Customization"]["CustomizationData"]["Hair"]={{"RowName","ModHair"}};
    auto defaultAppearance=source;defaultAppearance["Customization"]["CustomizationData"]["Hair"]={{"RowName","DefaultHair"}};
    const auto appearanceArchive=R::Prepare(appearance,defaultAppearance,initial.Archive,registry,"steam");assert(appearanceArchive.Captured==1&&appearanceArchive.Restored==0);
    assert(R::Prepare(defaultAppearance,defaultAppearance,appearanceArchive.Archive,registry,"steam").Save==defaultAppearance);
    auto noHistory=R::Prepare(source,cleanup.Save,R::New("steam",R::Character(source)),absent,"steam");
    assert(R::Prepare(cleanup.Save,cleanup.Save,noHistory.Archive,registry,"steam").Restored==0);
    auto other=cleanup.Save;other["meta_data"]["char_guid"]="another-character";bool wrongCharacter=false;try{R::Prepare(other,other,captured.Archive,registry,"steam");}catch(...){wrongCharacter=true;}assert(wrongCharacter);
    bool wrongRuntime=false;try{R::Prepare(cleanup.Save,cleanup.Save,captured.Archive,registry,"GAMEPASS");}catch(...){wrongRuntime=true;}assert(wrongRuntime);
    auto incomplete=registry;incomplete.Items.clear();bool notReady=false;try{R::Prepare(cleanup.Save,cleanup.Save,captured.Archive,incomplete,"steam");}catch(...){notReady=true;}assert(notReady);
    const auto directory=std::filesystem::temp_directory_path()/("save-recovery-"+std::to_string(GetCurrentProcessId()));
    auto written=R::Update(directory,"steam",source,source,registry);written=R::Update(directory,"steam",source,cleanup.Save,absent);assert(written.Captured==8);
    auto durable=R::Update(directory,"steam",cleanup.Save,cleanup.Save,registry);assert(durable.Restored>=7);
    auto once=R::Update(directory,"steam",cleanup.Save,cleanup.Save,registry);assert(once.Save["GameProgress"]["Inventory"].size()==1);
    const auto archive=directory/"characters"/(PS::CacheBundleIO::Hash("test-character")+".runeschema-save");
    assert(std::filesystem::exists(archive)&&std::filesystem::exists(archive.wstring()+L".previous"));
    {R::Lock inUse(archive.wstring()+L".lock");bool locked=false;try{R::Update(directory,"steam",source,source,registry);}catch(...){locked=true;}assert(locked);}
    const auto lastGood=PS::CacheBundleIO::Read(archive,R::MaximumBytes),deep=std::string(80,'[')+"0"+std::string(80,']');
    PS::CacheBundleIO::Publish(archive,deep);bool nested=false;try{R::Update(directory,"steam",source,source,registry);}catch(...){nested=true;}assert(nested&&PS::CacheBundleIO::Read(archive,R::MaximumBytes)==deep);
    PS::CacheBundleIO::Publish(archive,lastGood);
    const std::vector<uint8_t> world{'S','A','V','E',0,1,2,3};R::ArchiveWorld(directory,"steam","world-slot",world,{"MissingBuilding"});
    const auto worldFolder=directory/"worlds"/PS::CacheBundleIO::Hash("world-slot");size_t worlds=0;for(const auto& file:std::filesystem::directory_iterator(worldFolder))if(file.path().extension()==".world")++worlds;assert(worlds==1);
    R::ArchiveWorld(directory,"steam","world-slot",world,{"MissingBuilding"});
    {
        namespace B=PS::SavePruneBackup;
        const std::string guid="1234567890ABCDEF1234567890ABCDEF";auto native=source;native["meta_data"]["char_guid"]=guid;
        const auto bytes=native.dump();const auto backups=directory/"whole-save-backups";
        const auto copy=B::Write(backups,bytes,guid);assert(PS::CacheBundleIO::Read(copy,B::MaximumBytes)==bytes);
        assert(B::Write(backups,bytes,guid)==copy);
        const auto provider=directory/"native-character.json";PS::CacheBundleIO::Publish(provider,bytes);
        assert(B::Provider(backups,{provider},guid)==1&&PS::CacheBundleIO::Read(provider,B::MaximumBytes)==bytes);
        std::string utf16="\xff\xfe";for(unsigned char c:bytes){utf16+=char(c);utf16+='\0';}
        assert(B::Decode(utf16)==native&&PS::CacheBundleIO::Read(B::Write(backups,utf16,guid),B::MaximumBytes)==utf16);
        bool rejected=false;try{B::Write(backups,bytes,"ABCDEF1234567890ABCDEF1234567890");}catch(...){rejected=true;}assert(rejected);
        PS::CacheBundleIO::Publish(copy,"corrupt existing backup");rejected=false;try{B::Write(backups,bytes,guid);}catch(...){rejected=true;}
        assert(rejected&&PS::CacheBundleIO::Read(provider,B::MaximumBytes)==bytes);
        rejected=false;try{B::Provider(backups,{},guid);}catch(...){rejected=true;}assert(rejected);
    }
    std::filesystem::remove_all(PS::CacheBundleIO::NativePath(directory));
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
