#pragma once
#include "InputBindings.h"
#include <nlohmann/json.hpp>
#include <map>
#include <set>
#include <deque>
#include <cmath>
#include <limits>

namespace PS::HudInterfaces {
using Json=nlohmann::json;
inline std::string Text(const Json& row,const char* key,size_t limit,bool required=true){
    const auto value=row.value(key,std::string{});if((required&&value.empty())||value.size()>limit||value.find('\0')!=value.npos)throw std::runtime_error(std::string("Invalid HUD ")+key+".");return value;
}
inline std::string Id(const Json& row,const char* key){const auto value=Text(row,key,64);if(value.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-")!=value.npos)throw std::runtime_error("Invalid HUD identity.");return value;}
inline Json Value(const Json& control,const Json& value){
    const auto type=control.at("Type").get<std::string>();
    if(type=="toggle"){if(!value.is_boolean())throw std::runtime_error("HUD toggle needs true or false.");}
    else if(type=="slider"){
        if(!value.is_number()||!std::isfinite(value.get<double>()))throw std::runtime_error("HUD slider needs a finite number.");
        const double number=value.get<double>(),lo=control.at("Min"),hi=control.at("Max");if(number<lo||number>hi)throw std::runtime_error("HUD slider is outside its range.");
        const double step=control.value("Step",0.);if(step>0)return std::clamp(lo+std::round((number-lo)/step)*step,lo,hi);
    }else if(type=="text"||type=="label"||type=="choice"){
        if(!value.is_string()||value.get_ref<const std::string&>().size()>2048||value.get_ref<const std::string&>().find('\0')!=std::string::npos)throw std::runtime_error("HUD text is too long.");
        if(type=="choice"&&std::find(control.at("Options").begin(),control.at("Options").end(),value)==control.at("Options").end())throw std::runtime_error("Unknown HUD option.");
    }else if(type!="button"||!value.is_null())throw std::runtime_error("Unknown HUD control value.");
    return value;
}
inline Json Parse(const std::string& owner,const Json& document){
    if(owner.empty()||owner.size()>128||!document.is_object()||document.value("SchemaVersion",0)!=1||!document.contains("Interfaces")||!document["Interfaces"].is_array()||document["Interfaces"].size()>32)throw std::runtime_error("Invalid HUD interface document.");
    Json result=Json::array();std::set<std::string> ids;
    for(const auto& input:document.at("Interfaces")){
        if(!input.is_object())throw std::runtime_error("Invalid HUD interface.");
        const auto local=Id(input,"Id"),kind=Text(input,"Kind",16),id=owner+"/"+local;
        if(!ids.insert(id).second)throw std::runtime_error("Duplicate HUD interface.");
        Json row={{"Id",id},{"LocalId",local},{"Owner",owner},{"Name",Text(input,"Name",128)},{"Kind",kind},
            {"Keyboard",InputBindings::Text(InputBindings::Parse(input.value("Keyboard",std::string("None"))))},
            {"Controller",InputBindings::PadText(InputBindings::ParsePad(input.value("Controller",std::string("None"))))}};
        if(kind=="client"){row["Client"]=Text(input,"Client",128);const auto target=row["Client"].get<std::string>();if(target.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-")!=target.npos)throw std::runtime_error("Invalid HUD client name.");}
        else if(kind=="panel"){
            if(!input.contains("Controls")||!input["Controls"].is_array()||input["Controls"].size()>64)throw std::runtime_error("Invalid HUD controls.");
            Json controls=Json::array();std::set<std::string> fields;
            for(const auto& control:input["Controls"]){
                const auto field=Id(control,"Id"),type=Text(control,"Type",16);if(!fields.insert(field).second)throw std::runtime_error("Duplicate HUD control.");
                Json item={{"Id",field},{"Type",type},{"Label",Text(control,"Label",128)}};
                if(type=="slider"){
                    const double lo=control.at("Min"),hi=control.at("Max"),step=control.value("Step",0.);
                    if(!std::isfinite(lo)||!std::isfinite(hi)||!std::isfinite(step)||lo>=hi||lo<-(std::numeric_limits<double>::max()/2)||hi>std::numeric_limits<double>::max()/2||!std::isfinite(hi-lo)||step<0||step>hi-lo||(step>0&&!std::isfinite((hi-lo)/step)))throw std::runtime_error("Invalid HUD slider bounds.");
                    item["Min"]=lo;item["Max"]=hi;item["Step"]=step;
                }else if(type=="choice"){
                    const auto& options=control.at("Options");if(!options.is_array()||options.empty()||options.size()>32)throw std::runtime_error("Invalid HUD options.");
                    std::set<std::string> unique;for(const auto& option:options){if(!option.is_string()||option.get_ref<const std::string&>().empty()||option.get_ref<const std::string&>().size()>128||!unique.insert(option.get<std::string>()).second)throw std::runtime_error("Invalid HUD option.");}
                    item["Options"]=options;
                }
                item["Value"]=Value(item,control.value("Value",type=="button"?Json{}:type=="toggle"?Json(false):type=="slider"?control.value("Min",Json(0)):type=="choice"?item["Options"][0]:Json("")));
                controls.push_back(std::move(item));
            }row["Controls"]=std::move(controls);row["Ready"]=false;
        }else throw std::runtime_error("Unknown HUD interface kind.");
        result.push_back(std::move(row));
    }return result;
}
struct Store{
    Json Rows=Json::array();uint64_t Revision=0;size_t Bytes=0;std::map<std::string,std::set<std::string>> Callbacks;std::deque<Json> Commands;
    Json& Find(const std::string& id){for(auto& row:Rows)if(row.at("Id")==id)return row;throw std::runtime_error("HUD interface is not enabled.");}
    Json& Control(Json& row,const std::string& id){for(auto& control:row.at("Controls"))if(control.at("Id")==id)return control;throw std::runtime_error("Unknown HUD control.");}
    void Add(const Json& rows){if(Rows.size()+rows.size()>256)throw std::runtime_error("Too many HUD interfaces.");for(const auto& row:rows){if(std::any_of(Rows.begin(),Rows.end(),[&](const auto& old){return old.at("Id")==row.at("Id");}))throw std::runtime_error("Duplicate HUD interface across packages.");}auto next=Rows;for(const auto& row:rows)next.push_back(row);const auto bytes=next.dump().size();if(bytes>2*1024*1024)throw std::runtime_error("HUD declarations exceed the local transport limit.");Rows=std::move(next);Bytes=bytes;++Revision;}
    void Register(const std::string& id,const std::set<std::string>& controls){
        auto& row=Find(id);if(row.at("Kind")!="panel")throw std::runtime_error("Only declared panels accept Lua callbacks.");
        for(const auto& field:controls)Control(row,field);
        Callbacks[id]=controls;row["Ready"]=true;row["CallbackControls"]=controls;row.erase("Error");++Revision;
    }
    void Unregister(const std::string& id){auto& row=Find(id);Callbacks.erase(id);row["Ready"]=false;row.erase("CallbackControls");std::erase_if(Commands,[&](const auto& command){return command.at("Id")==id;});++Revision;}
    void Set(const std::string& id,const std::string& field,const Json& value){auto& row=Find(id);auto& control=Control(row,field);const auto checked=Value(control,value);const auto bytes=Bytes-control.at("Value").dump().size()+checked.dump().size();if(bytes>2*1024*1024)throw std::runtime_error("HUD values exceed the local transport limit.");control["Value"]=checked;Bytes=bytes;row.erase("Error");++Revision;}
    void Queue(const Json& request){
        const auto id=request.at("Id").get<std::string>(),field=request.at("Control").get<std::string>();auto& row=Find(id);
        if(!row.value("Ready",false)||!Callbacks.contains(id)||!Callbacks.at(id).contains(field))throw std::runtime_error("The mod has not registered this control.");
        const auto& control=Control(row,field);const auto value=Value(control,request.at("Value"));
        if(request.at("Expected")!=control.at("Value"))throw std::runtime_error("This control changed in the mod. Refresh and try again.");
        if(Commands.size()>=32)throw std::runtime_error("Wait for pending HUD changes.");
        if(std::any_of(Commands.begin(),Commands.end(),[&](const auto& entry){return entry.at("Id")==id&&entry.at("Control")==field;}))throw std::runtime_error("This control has a pending change.");
        Commands.push_back({{"Id",id},{"Owner",row.at("Owner")},{"LocalId",row.at("LocalId")},{"Control",field},{"Type",control.at("Type")},{"Value",value}});
    }
    Json Next(){if(Commands.empty())return {};auto command=std::move(Commands.front());Commands.pop_front();return command;}
    void Error(const std::string& id,const std::string& message){Find(id)["Error"]=message.substr(0,512);++Revision;}
};
}
