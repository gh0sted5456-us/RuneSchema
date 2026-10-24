#pragma once
#include <nlohmann/json.hpp>
#include <cstring>
#include <cmath>
#include <stdexcept>
#include <string>
namespace PS::ItemTransfer {
using Json=nlohmann::json;
inline Json Compact(const Json& row) {
    if(!row.is_object())throw std::runtime_error("Drop an item from the item bay.");
    const auto path=row.value("Path",std::string{});
    if(path.empty()||path.size()>1024||path.front()!='/'||path.find('.')==path.npos
        ||path.find_first_of("\r\n\t")!=path.npos||path.find('\0')!=path.npos)
        throw std::runtime_error("The dragged item has no canonical asset path.");
    Json result={{"Path",path},{"Count",row.value("Count",Json(1))}};
    if(!result["Count"].is_number_integer()||result["Count"]<1||result["Count"]>10000)
        throw std::runtime_error("Dragged quantity must be 1..10000.");
    for(const auto* key:{"Name","Icon","Category","SourceKind","SourceName","SourceRoot"})if(row.contains(key)&&row[key].is_string()) {
        const auto& text=row[key].get_ref<const std::string&>();
        if(text.size()<=1024&&text.find('\0')==text.npos)result[key]=text;
    }
    if(row.contains("Masterwork")&&row["Masterwork"].is_boolean())result["Masterwork"]=row["Masterwork"];
    if(row.contains("PowerLevel")&&row["PowerLevel"].is_number()&&std::isfinite(row["PowerLevel"].get<double>())&&row["PowerLevel"].get<double>()>=0)result["PowerLevel"]=row["PowerLevel"];
    if(row.contains("MaxStackSize")&&row["MaxStackSize"].is_number_integer()&&row["MaxStackSize"]>0&&row["MaxStackSize"]<=10000)result["MaxStackSize"]=row["MaxStackSize"];
    if(row.contains("SlotStrategy")&&row["SlotStrategy"].is_string()&&row["SlotStrategy"].get_ref<const std::string&>().size()<=64)result["SlotStrategy"]=row["SlotStrategy"];
    if(row.contains("NativeSlotVerified")&&row["NativeSlotVerified"].is_boolean())result["NativeSlotVerified"]=row["NativeSlotVerified"];
    if(row.contains("NativeMagicAmmo")&&row["NativeMagicAmmo"].is_boolean())result["NativeMagicAmmo"]=row["NativeMagicAmmo"];
    return result;
}
inline Json Decode(const void* data,std::size_t bytes) {
    if(!data||bytes<2||bytes>8193)throw std::runtime_error("Invalid item drag payload.");
    const auto* text=static_cast<const char*>(data);
    if(text[bytes-1]!='\0'||std::memchr(text,'\0',bytes-1))throw std::runtime_error("Invalid item drag terminator.");
    return Compact(Json::parse(text,text+bytes-1,[](int depth,Json::parse_event_t,Json&){
        if(depth>4)throw std::runtime_error("Item drag payload is too deeply nested.");return true;
    }));
}
}
