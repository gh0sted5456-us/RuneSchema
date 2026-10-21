#include "Core/QuickLoadSettings.h"
#include <cassert>
#include <string>
using namespace PS::QuickLoadSettings;
int main(){
    const auto disabled=Parse("");assert(!disabled.Enabled&&disabled.SoftLoading&&disabled.Mode=="lessRestrictive");
    assert(StandaloneEntryEnabled("Other:1\nQuickLoad : 1 ; comment\n"));
    assert(StandaloneEntryEnabled("\xEF\xBB\xBFQuickLoad:1\n"));
    assert(StandaloneEntryEnabled("qUiCkLoAd:1\n"));
    assert(!StandaloneEntryEnabled("; QuickLoad:1\nQuickLoad:0\nQuickLoadOther:1\n"));
    const auto original=Parse("[QuickLoad]\r\nWorldName=Ashenvale\r\nCharacterName=BREWER\r\n");
    assert(!original.Enabled&&original.CharacterName=="BREWER"&&original.WorldName=="Ashenvale");
    Config config{true,"BREWER","Ashenvale"};const auto round=Parse(Encode(config));assert(round.Enabled&&round.CharacterName==config.CharacterName&&round.WorldName==config.WorldName);
    assert(!Name("NAMEHERE")&&!Name("")&&!Name("world\nEnabled=true")&&!Name(std::string("a\0b",3)));
    assert(Name("World (2)")&&Name("Character-1"));
    assert(!Name("../world")&&!Name("world\\file")&&!Name("C:world")&&!Name(" world")&&!Name("world "));
    assert(Name("World of \xE9\x9B\xAA"));
    for(bool enabled:{false,true})for(bool soft:{false,true}){
        const Config choice{enabled,"BREWER","Ashenvale",soft};
        const auto again=Parse(Encode(choice));assert(again.Enabled==enabled&&again.SoftLoading);
        const auto document=Document(again);assert(document.size()==3&&!document.contains("allowCorruptCharacterEntry"));
    }
    // Normal never requires a character/world, and changing launch mode retains selections.
    assert(!Parse(Encode({false,"","",true})).Enabled);
    config.Enabled=false;const auto normal=Parse(Encode(config));assert(!normal.Enabled&&normal.CharacterName=="BREWER"&&normal.WorldName=="Ashenvale");
    bool rejected=false;try{(void)Encode({true,"","Ashenvale"});}catch(...){rejected=true;}assert(rejected);
    rejected=false;try{(void)Parse(std::string(32769,'x'));}catch(...){rejected=true;}assert(rejected);
    rejected=false;try{(void)Parse("[QuickLoad]\nEnabled=perhaps\n");}catch(...){rejected=true;}assert(rejected);
    rejected=false;try{(void)Parse("[QuickLoad]\nSoftLoading=perhaps\n");}catch(...){rejected=true;}assert(rejected);
    const auto scoped=Parse("[Other]\nEnabled=true\nCharacterName=wrong\n[QuickLoad]\nEnabled=0\nCharacterName=BREWER\nWorldName=world\n");assert(!scoped.Enabled&&scoped.CharacterName=="BREWER");
    for(const auto* mode:{"vanilla","lessRestrictive","quickLaunch"}){
        const auto selected=Parse(Encode({false,"BREWER","World",false,mode}));
        assert(selected.Mode==mode&&selected.Enabled==(selected.Mode=="quickLaunch")&&selected.SoftLoading==(selected.Mode!="vanilla"));
    }
    assert(Parse("[QuickLoad]\nMode=lessRestrictive\n").SoftLoading);
    rejected=false;try{(void)Parse("[QuickLoad]\nMode=surprise\n");}catch(...){rejected=true;}assert(rejected);
}
