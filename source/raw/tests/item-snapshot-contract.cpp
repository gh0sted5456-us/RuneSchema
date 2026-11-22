#include "Core/ItemSnapshot.h"
#include <cstdlib>
#include <iostream>
#include <string>
// runeschema.item.snapshot, engine-free half: exact request matching, the exact DateTime codec,
// census validation, layout (value types, not values) and the two digests.
namespace {
namespace S=PS::ItemSnapshot;using Json=nlohmann::json;
void Check(bool value,const char* message){if(!value){std::cerr<<"FAIL: "<<message<<"\n";std::exit(1);}}
template<class F>bool Throws(F&& f){try{f();}catch(...){return true;}return false;}
const std::string P="0A0B0C0D0E0F10111213141516171819",I="1A1B1C1D1E1F20212223242526272829";
Json Good(){return {{"Player","{0a0b0c0d-0e0f-1011-1213-141516171819}"},{"Scope","Inventory"},{"Slot",3},{"Instance",I},{"Item","/Game/Items/Copper/DA_CopperOre.DA_CopperOre"}};}
std::vector<S::Field> Fields(const std::string& guid="0000000100000002000000030000000A"){
    return {
        {"Count","/Script/Dominion.Item","IntProperty","int32","IntProperty:4x1",std::string(S::TextCodec),"12",false},
        {"Guid","/Script/Dominion.Item","StructProperty","FGuid","StructProperty:16x1{/Script/CoreUObject.Guid#16;A@0=UInt32Property:4x1}",std::string(S::TextCodec),guid,false},
        {"Durability","/Script/Dominion.Item","IntProperty","int32","IntProperty:4x1",std::string(S::TextCodec),"740",false},
        {"AddedToInventoryTimestamp","/Script/Dominion.Item","StructProperty","FDateTime","StructProperty:8x1{/Script/CoreUObject.DateTime#8}",std::string(S::DateTimeCodec),S::EncodeTicks(638000000000000123ULL),false},
        {"ItemDataPersistenceId","/Script/Dominion.Item","StrProperty","FString","StrProperty:16x1",std::string(S::TextCodec),"\"DA_CopperOre\"",true},
    };
}
}
int main(){
    // Requests: every part is required and exact; unknown fields and bad values are refused.
    {const auto r=S::Parse(Good());Check(r.Player==P&&r.Instance==I&&r.Slot==3&&r.Where==S::Scope::Inventory,"a valid request parses and normalizes GUIDs");
        for(const char* scope:{"Personal","Loadout"}){auto j=Good();j["Scope"]=scope;Check(S::Name(S::Parse(j).Where)==scope,"read-only scopes parse");}
        const auto refused=[&](auto change){auto j=Good();change(j);return Throws([&]{(void)S::Parse(j);});};
        Check(refused([](Json& j){j.erase("Instance");}),"Instance is required");
        Check(refused([](Json& j){j["Player"]=std::string(32,'0');}),"a zero GUID is refused");
        Check(refused([](Json& j){j["Scope"]="World";}),"arbitrary containers are refused");
        Check(refused([](Json& j){j["Slot"]=1024;})&&refused([](Json& j){j["Slot"]=-1;})&&refused([](Json& j){j["Slot"]="3";}),"slot bounds and type");
        Check(refused([](Json& j){j["Item"]="DA_CopperOre";})&&refused([](Json& j){j["Item"]=std::string(1025,'/');}),"item path form and bound");
        Check(refused([](Json& j){j["Restore"]=true;}),"unknown fields are refused, so no option can turn a read into a write");
        Check(Throws([]{(void)S::Parse(Json::array());}),"non-object requests are refused");}
    // Exact DateTime: 8 little-endian bytes, lower-case hex, inside Unreal's range.
    {Check(S::EncodeTicks(1)=="0100000000000000"&&S::DecodeTicks("0100000000000000")==1,"little-endian byte order");
        for(const uint64_t ticks:std::initializer_list<uint64_t>{0ULL,638000000000000123ULL,S::MaxDateTimeTicks})Check(S::DecodeTicks(S::EncodeTicks(ticks))==ticks,"ticks round-trip exactly");
        Check(Throws([]{(void)S::EncodeTicks(S::MaxDateTimeTicks+1);}),"out-of-range ticks are not encoded");
        Check(Throws([]{(void)S::DecodeTicks("ffffffffffffffff");}),"values beyond 9999-12-31 are refused");
        Check(Throws([]{(void)S::DecodeTicks("01000000000000");})&&Throws([]{(void)S::DecodeTicks("010000000000000000");}),"truncated or extended bytes are refused");
        Check(Throws([]{(void)S::DecodeTicks("0A00000000000000");})&&Throws([]{(void)S::DecodeTicks("0x00000000000000");}),"only lower-case hex is accepted");}
    // Build: identity and transient fields are marked, never dropped; both digests are stable.
    {const std::string item="/Game/Items/Copper/DA_CopperOre.DA_CopperOre",cls="/Script/Dominion.Item";
        const auto a=S::Build(item,cls,I,Fields());
        Check(a["Schema"]==std::string(S::Schema)&&a["Fields"].size()==5&&a["SourceInstance"]==I,"every save field is kept");
        Check(a["IdentityFields"]==Json::array({"Guid"})&&a["TransientFields"]==Json::array({"ItemDataPersistenceId"}),"identity and SaveGame+Transient fields are listed separately");
        Check(a["Fields"][0]["Name"]=="AddedToInventoryTimestamp","fields are sorted by name");
        auto reversed=Fields();std::reverse(reversed.begin(),reversed.end());
        Check(S::Build(item,cls,I,reversed)["Digest"]==a["Digest"],"digests do not depend on reflection order");
        const auto other=S::Build(item,cls,"2A2B2C2D2E2F30313233343536373839",Fields("0000000100000002000000030000000B"));
        Check(other["Digest"]!=a["Digest"]&&other["StateDigest"]==a["StateDigest"],"Digest covers identity; StateDigest compares state only");
        auto stack=Fields();stack[0].Value="13";
        const auto changed=S::Build(item,cls,I,stack);
        Check(changed["Layout"]==a["Layout"]&&changed["StateDigest"]!=a["StateDigest"],"values change digests, not the layout");
        auto nested=Fields();nested[1].Shape+=";B@4=UInt32Property:4x1";
        Check(S::Build(item,cls,I,nested)["Layout"]!=a["Layout"],"a nested member change changes the layout");
        auto cache=Fields();cache[4].Transient=false;
        Check(S::Build(item,cls,I,cache)["Layout"]!=a["Layout"],"the transient flag is part of the layout");
        auto codec=Fields();codec[3].Codec=std::string(S::TextCodec);
        Check(S::Build(item,cls,I,codec)["Layout"]!=a["Layout"],"the codec is part of the layout");
        Check(S::Build(item,"/Script/Dominion.Equipment",I,Fields())["Digest"]!=a["Digest"],"the item class is part of the digest");}
    // Census refusals: missing required fields, duplicates, unknown codecs, bad DateTime bytes, bounds.
    {const auto refused=[](auto change){auto f=Fields();change(f);return Throws([&]{(void)S::Build("/Game/X.X","/Script/Dominion.Item",I,f);});};
        Check(refused([](auto& f){f.erase(f.begin());}),"Count is required");
        Check(refused([](auto& f){f.erase(f.begin()+1);}),"Guid is required");
        Check(refused([](auto& f){f.push_back(f[0]);}),"duplicate field names are refused");
        Check(refused([](auto& f){f[0].Codec="raw-bytes";}),"there is no raw-memory fallback");
        Check(refused([](auto& f){f[3].Value="0100";}),"a malformed DateTime is refused at build");
        Check(refused([](auto& f){f[0].Value=std::string(S::MaxValueBytes+1,'1');}),"oversized values are refused");
        Check(refused([](auto& f){f.clear();}),"an empty census is refused");}
    // Refusals are ordinary answers with Ok false.
    {const auto r=S::Refusal("Slot","That slot is empty or out of range.");Check(r["Ok"]==false&&r["Phase"]=="Slot"&&r["Schema"]==std::string(S::Schema),"refusal shape");}
    std::cout<<"Item snapshot contract passed.\n";
}
