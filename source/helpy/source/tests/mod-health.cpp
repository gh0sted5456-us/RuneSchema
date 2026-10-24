#include "Core/ModHealth.h"
#include <cassert>
int main(){using namespace PS;ModHealth::State health;
    ModHealth::Observe(health,"Dye",LiveLog::Parse("LogLinker: Error: /Dye/Item failed"),"Game");assert(health.Errors==1);
    ModHealth::Observe(health,"Dye",LiveLog::Parse("[Warning][MOD:Dye] invalid reference"),"RuneSchema");assert(health.Warnings==1);
    ModHealth::Observe(health,"Dye",LiveLog::Parse("[Error] Dye_Quests had an issue"),"RuneSchema");assert(health.Errors==1);
    ModHealth::Observe(health,"Dye",LiveLog::Parse("[Error] mods/Dye_Quests/test.json"),"RuneSchema");assert(health.Errors==1);
    const auto dependency=LiveLog::Parse("[Error] owner='Flintlocks' dependency /Game/Mods/Dye/Data/Item failed");
    assert(ModHealth::Attributed("Flintlocks",dependency)&&!ModHealth::Attributed("Dye",dependency));
    assert(!ModHealth::Attributed("Dye",LiveLog::Parse("[Error][MOD:Dye_Quests] dependency /Dye/Item failed")));
    ModHealth::State probes;
    const auto missing=LiveLog::Parse("LogStreaming: Warning: LoadPackage: SkipPackage: /Game/Mods/VendorBanners/Registry/DA_RuneSchemaRegistry_VendorBanners - The package to load does not exist on disk or in the loader");
    ModHealth::Observe(probes,"VendorBanners",missing,"Game");assert(probes.OptionalProbes==1&&probes.Warnings==0);
    ModHealth::Observe(probes,"VendorBanners",LiveLog::Parse("LogUObjectGlobals: Warning: Failed to find object 'Object /Game/Mods/VendorBanners/Registry/DA_RuneSchemaRegistry_VendorBanners.DA_RuneSchemaRegistry_VendorBanners'"),"Game");assert(probes.OptionalProbes==2&&probes.Warnings==0);
    ModHealth::Observe(probes,"VendorBanners",LiveLog::Parse("LogLinker: Error: /Game/Mods/VendorBanners/Registry/DA_RuneSchemaRegistry_VendorBanners corrupt package"),"Game");assert(probes.Errors==1);
    ModHealth::Observe(probes,"VendorBanners",LiveLog::Parse("LogAnimation: Warning: /Game/Mods/VendorBanners/Animation Skeleton GUID is out-of-date"),"Game");assert(probes.Warnings==1);
    ModHealth::Observe(probes,"VendorBanners",missing,"RuneSchema");assert(probes.Warnings==2);
    assert(!ModHealth::OptionalProbe("Dye",missing,"Game"));
    ModHealth::State rejected;rejected.RejectedDeclarations=1;assert(std::string(ModHealth::Label(rejected,true))=="Errors observed");
    ModHealth::State clean;assert(std::string(ModHealth::Label(clean,false)).starts_with("Not checked"));assert(std::string(ModHealth::Label(clean,true))=="No errors observed");
}
