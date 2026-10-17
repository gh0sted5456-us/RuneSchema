#include "Core/ItemSource.h"
#include "Core/ItemFamily.h"
#include "Core/BoundedLru.h"
#include <cassert>
int main(){
    PS::BoundedLru<int,int> cache(16384);
    for(int i=0;i<16384;++i)cache.Put(i,i);
    assert(*cache.Find(0)==0);cache.Put(16384,16384);
    assert(cache.Size()==16384&&cache.Find(0)&&!cache.Find(1)&&cache.Find(16384));
    for(int i=16385;i<20000;++i){assert(cache.Find(0));cache.Put(i,i);}
    assert(cache.Size()==16384&&cache.Find(0));
    cache.Clear();assert(cache.Size()==0&&!cache.Find(0));
    using namespace PS;using K=ItemSource::Kind;
    HelpyCacheSources::Plan plan;plan.Owners["game"]={"Dye"};plan.Owners["custommount"]={"CapeMod"};plan.DeclaredOwners["custommount"]={"CapeMod"};
    plan.Owners["borrowedicons"]={"CapeMod"};
    const std::set<std::string> native{"agility","futurefeature"};
    assert(ItemSource::Describe("/Game/Items/I.I",plan,native).Group==K::Vanilla);
    assert(ItemSource::Describe("/Game/Mods/Dye/Items/I.I",plan,native).Group==K::Modded);
    assert(ItemSource::Describe("/Game/RuneSchema/Clones/I.I",plan,native).Group==K::Modded);
    assert(ItemSource::Describe("/CustomMount/Items/I.I",plan,native).Name=="CapeMod");
    assert(ItemSource::Describe("/CustomMount/Items/I.I",plan,native).Group==K::Modded);
    assert(ItemSource::Describe("/Agility/Items/I.I",plan,native).Group==K::Expansion);
    assert(ItemSource::Describe("/FutureFeature/Items/I.I",plan,native).Group==K::Expansion);
    assert(ItemSource::Describe("/UnclaimedMount/Items/I.I",plan,native).Group==K::Unknown);
    assert(ItemSource::Describe("/BorrowedIcons/Items/I.I",plan,native).Group==K::Unknown);
    assert(ItemSource::Describe("/Engine/Transient.Item",plan,native).Group==K::Unknown);
    assert(ItemSource::Row({{"Path","/UnclaimedMount/Items/I.I"},{"Cooked",true}}).Group==K::Unknown);
    assert(ItemSource::Describe("/Game/Items/Clone.Clone",plan,native,"MyMod").Group==K::Modded);
    assert(ItemSource::Describe("/Game/Items/Override.Override",plan,native,{},true).Group==K::Modded);
    assert(ItemSource::Describe("/Game/../Private/I.I",plan,native).Group==K::Unknown);
    auto query=ItemSource::Query(K::Modded,"Armor","steel helm");
    assert(query=="__helpysource:modded __helpycat:armor steel helm");
    assert(ItemSource::TakeFilter(query)==K::Modded&&query=="__helpycat:armor steel helm");
    assert(ItemSource::Matches(K::All,{K::Unknown,{},{}})&&!ItemSource::Matches(K::Vanilla,{K::Modded,{},{}}));
    assert(ItemSource::Row({{"Path","/Game/Items/X.X"},{"SourceKind","modded"},{"SourceName","Dye"}}).Group==K::Modded);
    assert(ItemSource::Row({{"Path","/Game/Items/X.X"},{"RuntimeClone",true}}).Group==K::Modded);
    HelpyCacheSources::Reset();HelpyCacheSources::Declare("MyCape","/Cape/Items/C.C");HelpyCacheSources::Registered("/Cape/Items/C.C");
    HelpyCacheSources::Reference("/Cape/Items/C.C","/BorrowedArt/Icons/I.I");
    const auto ownership=HelpyCacheSources::Snapshot();
    assert(ownership.Owners.contains("borrowedart")&&!ownership.DeclaredOwners.contains("borrowedart"));
    assert(ItemSource::Describe("/Cape/Items/C.C",ownership).Name=="mycape");
    assert(ItemSource::Describe("/BorrowedArt/Items/Other.Other",ownership).Group==K::Unknown);
    const auto family=ItemFamily::Key("Fallen Hoplite's Helm (Dyed Chrome)");
    assert(family=="fallen_hoplite_dyed_chrome");
    assert(ItemFamily::Matches(family,"Fallen Hoplite's Gauntlets (Dyed Chrome)"));
    assert(ItemFamily::Matches(family,"Fallen Hoplite\xe2\x80\x99s Boots (Dyed Chrome)"));
    assert(ItemFamily::Matches(family,"ITEM_FallenHoplite_Chestplate_DyedChrome"));
    assert(!ItemFamily::Matches(family,"Fallen Hoplite's Helm (Dyed Gold)"));
    assert(!ItemFamily::Matches(family,"Steel Helm (Dyed Chrome)"));
    assert(!ItemFamily::Matches(family,"Fallen Hoplite's Helm"));
    assert(ItemFamily::Key("Gold coin").empty()&&ItemFamily::Key("Helm").empty());
    auto familyQuery="__helpyfamily:"+family+" chrome";
    assert(ItemFamily::TakeFilter(familyQuery)==family&&familyQuery=="chrome");
    bool rejected=false;try{auto bad=std::string("__helpyfamily:../private");ItemFamily::TakeFilter(bad);}catch(...){rejected=true;}assert(rejected);
}
