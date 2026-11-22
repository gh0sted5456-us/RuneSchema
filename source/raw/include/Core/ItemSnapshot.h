#pragma once
#include "Core/HelpyFlagSignature.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
// runeschema.item.snapshot: a read-only, lossless record of one live item instance.
// This header is the engine-free half (request rules, the typed codecs, layout and digests),
// shared with tests. Runtime/ItemSnapshotService.cpp does the reflected census on the game thread.
// Nothing here creates, moves or changes an item; restore is a separate, later contract.
namespace PS::ItemSnapshot {
using Json=nlohmann::json;
inline constexpr std::string_view Service="runeschema.item.snapshot";
inline constexpr std::string_view Schema="runeschema.item-snapshot/1";
inline constexpr size_t MaxFields=128,MaxValueBytes=128*1024,MaxSnapshotBytes=1024*1024,MaxItemPath=1024,MaxShapeBytes=16*1024;
// Codecs. Text: Unreal's own property text, accepted only when it re-imports Identical to the source.
// DateTime: Unreal's text export truncates ticks, so FDateTime is stored as its 8 bytes, little-endian.
inline constexpr std::string_view TextCodec="unreal-property-text/1";
inline constexpr std::string_view DateTimeCodec="native-datetime-le64/1";
inline constexpr std::string_view DateTimeStruct="/Script/CoreUObject.DateTime";
inline constexpr uint64_t MaxDateTimeTicks=3652059ULL*864000000000ULL-1; // 9999-12-31 23:59:59.9999999

// Owned: one exact inventory component the character's controller owns (a mod's own storage),
// named by Container {Index, Serial} from its ObjectIdentity.
enum class Scope{Inventory,Personal,Loadout,Owned};
inline std::string_view Name(Scope scope){return scope==Scope::Inventory?"Inventory":scope==Scope::Personal?"Personal":scope==Scope::Loadout?"Loadout":"Owned";}

// 32 upper-case hex digits; braces and dashes accepted on input. Empty when invalid or all zero.
inline std::string NormalizeGuid(std::string_view value){
    std::string out;
    for(const unsigned char c:value){
        if(c=='-'||c=='{'||c=='}')continue;
        if(!std::isxdigit(c))return {};
        out.push_back(static_cast<char>(std::toupper(c)));
    }
    return out.size()==32&&out!=std::string(32,'0')?out:std::string{};
}

// The exact item the caller means: a character, one of its containers, a slot, and the instance
// GUID and definition it expects there. Every part must match the live slot or nothing is read.
struct Request{std::string Player,Instance,Item;Scope Where=Scope::Inventory;int Slot=0;int32_t Index=-1,Serial=0;};
inline Json ContainerJson(const Request& r){return {{"Index",r.Index},{"Serial",r.Serial}};}
inline Request Parse(const Json& input){
    if(!input.is_object())throw std::invalid_argument("The request must be a JSON object.");
    static const std::set<std::string> known{"Player","Scope","Slot","Instance","Item","Container"};
    for(const auto& [key,value]:input.items())if(!known.contains(key))throw std::invalid_argument("Unknown request field: "+key);
    Request request;
    const auto text=[&](const char* key)->std::string{
        const auto found=input.find(key);
        if(found==input.end()||!found->is_string())throw std::invalid_argument(std::string(key)+" must be a string.");
        return found->get<std::string>();
    };
    request.Player=NormalizeGuid(text("Player"));if(request.Player.empty())throw std::invalid_argument("Player must be a character GUID.");
    request.Instance=NormalizeGuid(text("Instance"));if(request.Instance.empty())throw std::invalid_argument("Instance must be the item's instance GUID.");
    request.Item=text("Item");
    if(request.Item.empty()||request.Item.size()>MaxItemPath||request.Item.front()!='/'||request.Item.find('\0')!=std::string::npos)
        throw std::invalid_argument("Item must be the item definition's object path.");
    const auto scope=text("Scope");
    if(scope=="Inventory")request.Where=Scope::Inventory;
    else if(scope=="Personal")request.Where=Scope::Personal;
    else if(scope=="Loadout")request.Where=Scope::Loadout;
    else if(scope=="Owned")request.Where=Scope::Owned;
    else throw std::invalid_argument("Scope must be Inventory, Personal, Loadout or Owned.");
    const auto container=input.find("Container");
    if((container!=input.end())!=(request.Where==Scope::Owned))throw std::invalid_argument("Container goes with Scope Owned, and only with it.");
    if(container!=input.end()){
        if(!container->is_object()||container->size()!=2||!container->contains("Index")||!container->contains("Serial")
            ||!(*container)["Index"].is_number_integer()||!(*container)["Serial"].is_number_integer())throw std::invalid_argument("Container must be {Index, Serial} from ObjectIdentity.");
        const auto index=(*container)["Index"].get<int64_t>(),serial=(*container)["Serial"].get<int64_t>();
        if(index<0||index>INT32_MAX||serial<1||serial>INT32_MAX)throw std::invalid_argument("Container identity is out of range.");
        request.Index=static_cast<int32_t>(index);request.Serial=static_cast<int32_t>(serial);
    }
    const auto slot=input.find("Slot");
    if(slot==input.end()||!slot->is_number_integer())throw std::invalid_argument("Slot must be an integer.");
    const auto value=slot->get<int64_t>();if(value<0||value>=1024)throw std::invalid_argument("Slot is outside 0..1023.");
    request.Slot=static_cast<int>(value);
    return request;
}

// FDateTime ticks as 16 lower-case hex digits of the 8 little-endian bytes.
inline std::string EncodeTicks(uint64_t ticks){
    if(ticks>MaxDateTimeTicks)throw std::runtime_error("DateTime exceeds Unreal's supported range");
    constexpr char hex[]="0123456789abcdef";std::string out;
    for(int i=0;i<8;++i){const auto byte=static_cast<unsigned>((ticks>>(8*i))&0xFF);out+=hex[byte>>4];out+=hex[byte&15];}
    return out;
}
inline uint64_t DecodeTicks(std::string_view text){
    if(text.size()!=16||text.find_first_not_of("0123456789abcdef")!=text.npos)throw std::runtime_error("Invalid exact DateTime bytes");
    constexpr std::string_view hex="0123456789abcdef";uint64_t ticks=0;
    for(int i=0;i<8;++i)ticks|=static_cast<uint64_t>((hex.find(text[2*i])<<4)|hex.find(text[2*i+1]))<<(8*i);
    if(ticks>MaxDateTimeTicks)throw std::runtime_error("DateTime exceeds Unreal's supported range");
    return ticks;
}

// One captured save field. Shape describes the value type all the way down (struct members,
// enums, container elements), so drift in a nested type changes the layout hash.
struct Field{
    std::string Name,Owner,Type,CppType,Shape,Codec,Value;
    bool Transient=false; // SaveGame and Transient together: kept as evidence, never dropped.
};
inline bool IdentityField(const Field& field){return field.Name=="Guid"&&field.Shape.find("/Script/CoreUObject.Guid")!=std::string::npos;}
inline std::string Sha256Hex(std::string_view text){
    const auto digest=HelpyDeliveryFlags::Signature::Sha256(text.data(),text.size());
    return HelpyDeliveryFlags::Signature::Hex(digest.data(),digest.size());
}
// Layout: every captured field's name, owner, reflected type, C++ type, nested shape, codec and
// transient flag, sorted by name. Values are not part of it.
inline std::string Layout(const std::vector<Field>& fields){
    std::string text="runeschema.item-layout/1\n";
    for(const auto& field:fields)text+=field.Name+"|"+field.Owner+"|"+field.Type+"|"+field.CppType+"|"+field.Shape+"|"+field.Codec+"|"+(field.Transient?"T":"-")+"\n";
    return "sha256:"+Sha256Hex(text);
}
// Validates the census and builds the snapshot. Digest covers everything; StateDigest leaves out
// the identity field, so two instances with the same state compare equal while staying distinct.
inline Json Build(const std::string& item,const std::string& itemClass,const std::string& instance,std::vector<Field> fields){
    if(fields.empty()||fields.size()>MaxFields)throw std::runtime_error("Native item save field count is outside 1..128");
    std::sort(fields.begin(),fields.end(),[](const Field& a,const Field& b){return a.Name<b.Name;});
    for(size_t i=1;i<fields.size();++i)if(fields[i].Name==fields[i-1].Name)throw std::runtime_error("Ambiguous native item property: "+fields[i].Name);
    for(const char* required:{"Count","Guid","Durability"})
        if(std::none_of(fields.begin(),fields.end(),[&](const Field& field){return field.Name==required;}))throw std::runtime_error("Native item save fields changed");
    Json rows=Json::array(),identity=Json::array(),cache=Json::array(),all=Json::array(),state=Json::array();
    for(const auto& field:fields){
        if(field.Name.empty()||field.Codec.empty()||field.Value.size()>MaxValueBytes||field.Shape.size()>MaxShapeBytes)throw std::runtime_error("Native item field exceeds its bound: "+field.Name);
        if(field.Codec!=TextCodec&&field.Codec!=DateTimeCodec)throw std::runtime_error("Unknown item field codec: "+field.Codec);
        if(field.Codec==DateTimeCodec)(void)DecodeTicks(field.Value);
        Json row{{"Name",field.Name},{"Owner",field.Owner},{"Type",field.Type},{"CppType",field.CppType},{"Codec",field.Codec},{"Value",field.Value}};
        if(field.Transient)row["Transient"]=true;
        const bool isIdentity=IdentityField(field);if(isIdentity)row["Identity"]=true;
        rows.push_back(row);all.push_back({field.Name,field.Codec,field.Value});
        if(!isIdentity)state.push_back({field.Name,field.Codec,field.Value});
        if(isIdentity)identity.push_back(field.Name);
        if(field.Transient)cache.push_back(field.Name);
    }
    const auto layout=Layout(fields);
    const auto digest=[&](const Json& values){return "sha256:"+Sha256Hex(Json{{"Schema",Schema},{"Item",item},{"Class",itemClass},{"Layout",layout},{"Fields",values}}.dump());};
    Json snapshot{{"Schema",Schema},{"Item",item},{"Class",itemClass},{"SourceInstance",instance},{"Layout",layout},
        {"Fields",rows},{"IdentityFields",identity},{"TransientFields",cache},{"Digest",digest(all)},{"StateDigest",digest(state)}};
    if(snapshot.dump().size()>MaxSnapshotBytes)throw std::runtime_error("Native item snapshot exceeds its bound");
    return snapshot;
}
// The service reply. A refusal is a normal answer with Ok false; nothing was read or changed.
inline Json Refusal(std::string_view phase,std::string_view message){return {{"Schema",Schema},{"Ok",false},{"Phase",phase},{"Error",message}};}
}
