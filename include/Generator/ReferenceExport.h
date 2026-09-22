#pragma once
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <stdexcept>

namespace PS::ReferenceExport {
using nlohmann::json;
inline json Build(const std::string& loader,const json& entry,const json& values,const json& available) {
    if(!values.is_object() || !available.is_object())throw std::runtime_error("Reference fields must be objects.");
    const bool authored=entry.value("Authored",false);
    json source=json::object();
    for(const auto* key:{"Path","Name","InternalName","Target","Row","Table","ReflectedType"})if(entry.contains(key))source[key]=entry[key];
    json output={{"Kind","RuneSchemaEditableReference"},{"Version",1},{"Loader",loader},
        {"Source",source},{"CaptureScope",authored?"Authored definition; may be inactive":"Loaded record; not an unmodified vanilla baseline"},
        {"Origin","Unknown; paths are clues, not proof of mod ownership"},
        {"Values",json::object()},{"FieldInfo",json::object()},{"Truncated",false}};
    std::set<std::string> names;
    for(const auto& [key,value]:available.items())names.insert(key);
    for(const auto& [key,value]:values.items())names.insert(key);
    if(loader=="assets" || loader=="buildings" || loader=="spawns")
        for(const auto* key:{"Name","InternalName","PersistenceID","Type","Mesh","Skeleton","Icon","FlavourText"})names.insert(key);
    for(const auto& name:names) {
        if(output["Values"].size()>=256){output["Truncated"]=true;break;}
        const bool captured=values.contains(name) && !values[name].is_null();
        output["Values"][name]=captured?values[name]:json(nullptr);
        output["FieldInfo"][name]={{"Status",captured?(authored?"Authored value":"Captured value"):"Unknown, unset, excluded or unsupported"},
            {"Writable","Unverified; inspectable does not imply loader-editable"}};
        if(available.contains(name))output["FieldInfo"][name]["Metadata"]=available[name];
    }
    if(authored && values.contains("$Clone"))output["DeclaredCloneSource"]=values["$Clone"];
    std::set<std::string> paths;size_t visited=0;
    auto collect=[&](auto&& self,const json& value,unsigned depth)->void {
        if(depth>8 || ++visited>4096 || paths.size()>=128){output["Truncated"]=true;return;}
        if(value.is_string()) {
            const auto& text=value.get_ref<const std::string&>();
            if(text.starts_with('/') && text.size()<=1024)paths.insert(text);
        } else if(value.is_structured())for(const auto& child:value)self(self,child,depth+1);
    };
    collect(collect,source,0);collect(collect,output["Values"],0);
    output["ReferencedPaths"]=paths;
    if(output.dump().size()>512*1024)throw std::runtime_error("Reference exceeds 512 KiB; narrow the selection.");
    return output;
}
inline std::string Jsonc(const json& report) {
    return "// Editable reference only. Do not place this report in a mod loader folder.\n"
        "// null means unknown/unset/unsupported; never apply it as a clearing instruction.\n"
        "// Values retain actual field names. Common absent fields are prompts, not verified properties.\n"
        "// Copy verified fields into a loader starter; preserve IDs for overrides, create new IDs for new clones.\n"
        "// Referenced paths may reveal dependencies; mod ownership and runtime $Clone origin are not inferred.\n"
        +report.dump(2)+"\n";
}
}
