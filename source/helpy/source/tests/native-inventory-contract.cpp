#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <iostream>
static std::string Read(const std::filesystem::path& path){std::ifstream input(path);if(!input)throw std::runtime_error("Missing inventory contract input");return {std::istreambuf_iterator<char>(input),{}};}
static void Check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main(int argc,char**argv){
    try{
    Check(argc==2,"Expected source directory");const auto root=std::filesystem::path(argv[1]);
    const auto widgets=Read(root/"raw/include/SDK/Helper/NativeInventoryWidgets.h");
    const auto loader=Read(root/"raw/src/Loader/DragonWildsSpawnLoader.cpp");
    Check(!loader.contains("NativeDeleteObserver")&&!loader.contains("NativeScrapInput")&&!loader.contains("m_deleteObserver")&&!loader.contains("m_scrapInput"),"Retired deletion/Scrap hooks must not be installed");
    const auto markers=Read(root/"raw/include/SDK/Helper/NativeInventoryMarks.h");
    for(const auto* proof:{"CycleMark","NativeInventoryMarkClick","IsPressed","IsHovered","GetCurrentThreadId()!=Thread","Item(hovered->Slot.Get(),controller,character)",
        "copies!=1","owner.Object()!=controller","if(copies!=1)","created>=8","budget=16","milliseconds(100)","ClearRootSet",
        "VK_MBUTTON","MiddleClick.Step(middleDown,middleKey)","itemHover(marker.Slot.Get()","if(down&&middleDown)","CancelClicks()"})
        Check((markers+widgets).find(proof)!=std::string::npos,"Inventory marks must consume only their own button clicks, revalidate stack ownership and bound UI work");
    for(const auto* forbidden:{"OnItemUsed","PreventOriginalFunctionCall","DestroyItemFromInventory","UseItem","AddToViewport","ForEachUObject"})
        Check(markers.find(forbidden)==markers.npos,"Inventory marking must not intercept item actions, delete items or create an extra menu");
    Check(!markers.contains("PlayerInventoryScrap"),"Favorites must not depend on Scrap");
    const auto quick=Read(root/"helpy/source/include/Generator/RuneSchemaSettingsPanel.h");
    Check(quick.find("Extend Helpy Favorites to Game")==quick.npos,"Favorites are a built-in feature, not a Quick Settings toggle");
    const auto saves=Read(root/"raw/src/Generator/CharacterSaveEditor.inl");
    Check(saves.find("XboxCharacterSaves::Candidates")!=saves.npos&&saves.find("XboxCharacterSaves::Character(source)")!=saves.npos&&saves.find("if(xbox||copyOnly)path=WriteSaveCopy")!=saves.npos,"Xbox and active character opening must validate and snapshot provider-owned payloads");
    Check(saves.find("CharacterSaveEditor::Identity(source)!=expected")<saves.find("if(xbox||copyOnly)path=WriteSaveCopy"),"Current character identity must be checked before its isolated snapshot is written");
    Check(saves.contains("HostServices::ProfileDirectory()/\"characters\"")&&saves.contains("OriginalPath"),"Profile character copies must retain source identification without replacing native saves");
    const auto platform=Read(root/"raw/include/SDK/Helper/PlayerStatus.h");
    Check(platform.contains("AccountName(UObject* state,bool local=false)")&&platform.contains("TEXT(\"DisplayName\")")&&platform.contains("chars.Num()>129")&&platform.contains("DomPlatformData"),"Account display names require the exact bounded native platform-data contract");
    Check(platform.contains("if(local){const auto steam=SteamBuild::PersonaName();")&&platform.contains("return AccountName(AH::GetObjectRef(controller,TEXT(\"PlayerState\")),local);"),"Steam persona fallback must be limited to the verified local controller");
    const auto relay=Read(root/"raw/include/SDK/Helper/InventoryMarkRelay.h"),authority=Read(root/"raw/src/Loader/SpawnTools.inl");
    Check(relay.contains("ProfileSerial")&&relay.contains("ack.at(\"CharacterGuid\")==ProfilePending.at(\"CharacterGuid\")")&&relay.contains("ClearPublicProfiles")&&relay.contains("ProfileAttempts++>=3"),"Public profile replies must bind to a current owned-player request and expire on world change");
    Check(authority.contains("ReceivePublicProfile")&&authority.contains("{\"CharacterGuid\",guid}")&&authority.contains("Profile target is not connected"),"Only the authenticated owned player may publish a profile; reads require a connected target");
    const auto editor=Read(root/"helpy/source/include/Generator/SaveEditorPanel.h");
    const auto studio=Read(root/"helpy/source/include/Generator/CharacterStudioPanel.inl");
    Check(editor.find("PlayerStatus::LocalCharacter(catalog)")!=editor.npos&&studio.find("Follow current character")!=studio.npos,"Character editor must follow the verified current character");
    Check(studio.find("StudioCurrentSave")!=studio.npos&&studio.find("{\"CopyOnly\",true},{\"CharacterGuid\",live}")!=studio.npos,"The live character must open an isolated copy automatically, with identity verification");
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
