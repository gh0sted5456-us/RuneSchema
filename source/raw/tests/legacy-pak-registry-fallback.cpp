#include "Core/CookedPakRegistryManifest.h"
#include "Core/JsonDocument.h"
#include <filesystem>
#include <stdexcept>

namespace M=PS::CookedPakRegistryManifest;
using Json=nlohmann::json;
static void Check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main(int argc,char** argv){
    Check(argc==2,"Expected the legacy PAK example root");
    const auto root=std::filesystem::path(argv[1]);
    Json document;std::string source;size_t files=0;
    M::Reset();
    PS::JsonHelpers::ParseJsonFilesInPathWithSource(root/"LegacyWeapons"/"registry",
        [&](const Json& value,const std::filesystem::path& relative){
            ++files;document=value;source=relative.generic_string();
            Check(value.at("SchemaVersion")==1&&value.at("Entries").empty(),"Sidecar must be a complete registry document");
            Check(M::Publish(value,"LegacyWeapons",source),"Loose JSONC must publish without a cooked declaration");
        });
    Check(files==1&&source=="10-native.jsonc","The real JSONC folder parser must read the sidecar");
    Check(!document.contains("RegistryOwner")&&!document.contains("Enrollment"),"Loose ownership comes from the folder");
    const auto manifests=M::Snapshot();
    Check(manifests.size()==1&&manifests[0].Owner=="LegacyWeapons","Loose ownership was not retained");
    Check(manifests[0].AssetLanes.size()==1&&manifests[0].AssetLanes[0].Paths[0]=="/OriginalWeapons/Items/ITEM_Rifle.ITEM_Rifle",
        "A sidecar must retain the original content namespace");
    for(const auto* lane:{"Recipes","Buildings"}){
        const auto values=M::Definitions(lane);
        Check(values.size()==1&&values[0].Owner=="LegacyWeapons"&&values[0].Body.at("Unlock")==false,
            "Loose metadata must reach its native loader without granting progress");
        Check(!values[0].Body.contains("$Clone")&&!values[0].Body.contains("Properties"),"Enrollment must not construct or rewrite an asset");
    }
    Check(M::Definitions("Buildings")[0].Body.at("AddTo").at("Collection")=="Modded Buildings",
        "Legacy building menu metadata must not require recooking");
    Check(M::Publish(document,"LegacyWeapons",source)&&M::Snapshot().size()==1,"Loose publication must be idempotent");
    const auto cooked=M::Parse(document,"LegacyWeapons","pak:/OriginalWeapons.RSREG_Native");
    Check(cooked&&cooked->Fingerprint==manifests[0].Fingerprint,"Loose and cooked documents must share the same contract");
    Check(M::Publish(document,"LegacyWeapons","pak:/OriginalWeapons.RSREG_Native"),"Matching cooked metadata should be accepted");
    Check(M::Definitions("Buildings").size()==1&&M::Definitions("Recipes").size()==1,"Matching declarations must coalesce");
    auto conflict=document;
    const auto building="/OriginalWeapons/Buildings/BUILDING_Bench.BUILDING_Bench";
    conflict["LoaderDefinitions"]["Buildings"][building]["AddTo"]["PageIndex"]=1;
    bool rejected=false;try{M::Publish(conflict,"LegacyWeapons","conflict.jsonc");}catch(const std::exception&){rejected=true;}
    Check(rejected&&M::Snapshot().size()==2&&M::Definitions("Buildings")[0].Body.at("AddTo").at("PageIndex")==0,
        "Conflicting policy must reject atomically");
    M::Reset();
    Check(M::Publish(document,"LegacyWeapons",source),"Loose enrollment must not depend on a cooked registry surviving");
    Check(M::Definitions("Buildings").size()==1,"The fallback must remain functional alone");
}
