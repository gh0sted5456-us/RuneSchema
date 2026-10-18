#include "Core/ContentSource.h"
#include "Core/IconCacheBuild.h"
#include "Core/RecipeWorkspace.h"
#include <cassert>
#include <iostream>
int main(){
    using namespace PS;using Json=nlohmann::json;
    assert(ContentSource::Root("/Game/Mods/ColorsOfMoney/Stations/Library.Library")=="/Game/Mods/ColorsOfMoney");
    assert(ContentSource::Root("/Game/RuneSchema/MyClones/Items/Helm.Helm")=="/Game/RuneSchema/MyClones");
    assert(ContentSource::Root("/GameFeatures/Plugins/Brynmoor/UI/I.I")=="/GameFeatures/Plugins/Brynmoor");
    assert(ContentSource::Root("/Plugins/GameFeatures/Fellhollow/UI/I.I")=="/Plugins/GameFeatures/Fellhollow");
    assert(ContentSource::Root("/Plugins/Feature/UI/I.I")=="/Plugins/Feature");
    assert(ContentSource::Root("/UmbralSands/UI/Icons/I.I")=="/UmbralSands");
    assert(ContentSource::Root("/Game/../Private/F.F").empty());
    assert(RecipeWorkspace::StationSource({{"Path","/Game/RuneSchema/MyClones/Stations/B.B"}})=="/Game/RuneSchema/MyClones");
    HelpyCacheSources::Plan plan;plan.Authoritative=true;plan.Registered={"game","engine","umbralSands","colorsofmoney"};
    plan.Owners["colorsofmoney"]={"ColorsOfMoney"};plan.Owners["game"]={"Dye"};
    assert(ContentSource::Origins("/Game/Art/UI/I.I",plan)==std::set<std::string>{"core"});
    assert(ContentSource::Origins("/UmbralSands/UI/I.I",plan)==std::set<std::string>{"plugins/umbralsands"});
    assert(ContentSource::Origins("/ColorsOfMoney/Icons/I.I",plan)==std::set<std::string>{"mods/colorsofmoney"});
    assert(ContentSource::Origins("/Game/RuneSchema/C/C.C",plan,"ColorsOfMoney")==std::set<std::string>{"mods/colorsofmoney"});
    assert(ContentSource::UiTexture("/Foo/Icons/Custom.Custom","Texture2D"));assert(!ContentSource::UiTexture("/Foo/Art/T_Normal.T_Normal","Texture2D"));
    assert(ContentSource::NativeContentFile("Plugins/GameFeatures/Foo/Content/Paks/Foo.utoc"));
    assert(ContentSource::NativeContentFile("Content/Paks/Core.pak"));assert(!ContentSource::NativeContentFile("Plugins/Foo/Settings.ini"));
    assert(!ContentSource::Valid("mods/../escape"));assert(!ContentSource::Valid("mods/C:\\escape"));
    assert(!ContentSource::Valid(std::string("mods/a\0b",8)));
    assert(!IconCacheBuild::Start("core"));IconCacheBuild::PumpAvailable=true;
    assert(!IconCacheBuild::Start("../../save"));assert(IconCacheBuild::Start("mods/colorsofmoney"));
    assert(IconCacheBuild::HasWork()&&!IconCacheBuild::Start("core"));
    assert(IconCacheBuild::Take().value()=="mods/colorsofmoney");assert(!IconCacheBuild::Take());
    IconCacheBuild::Cancel();assert(IconCacheBuild::Cancelled());
    IconCacheBuild::Publish({{"Active",false},{"Stage","Cancelled"}});assert(!IconCacheBuild::HasWork());
    assert(IconCacheBuild::Start(""));assert(!IconCacheBuild::Cancelled());assert(IconCacheBuild::Take().value().empty());
    IconCacheBuild::Publish({{"Active",false},{"Stage","Complete"}});
    IconCacheBuild::SetManifest({{"Expected",{"/Game/UI/I.I"}}});
    assert(!IconCacheBuild::Read().contains("Expected"));assert(IconCacheBuild::Read(true)["Expected"].size()==1);
    assert(IconCacheBuild::Start("core"));assert(!IconCacheBuild::Read(true).contains("Expected"));
    (void)IconCacheBuild::Take();IconCacheBuild::Publish({{"Active",false}});
    std::cout<<"Menu-independent cache control, source boundaries and recipe grouping tests passed\n";
}
