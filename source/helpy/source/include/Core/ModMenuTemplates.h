#pragma once
#include <nlohmann/json.hpp>
#include "ModMenuColor.h"
#include <cmath>
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

namespace PS::ModMenuTemplates {
using Json=nlohmann::json;
inline constexpr size_t MaximumBytes=256*1024;
inline void Require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
inline const Json& Contract(const Json& page){
    const auto& definition=page.at("Helpy").at("templates");
    Require(definition.is_object()&&definition.at("id").is_string()&&definition.at("version").is_number_integer(),"This mod has not declared a supported template contract.");
    const auto& keys=definition.at("keys");Require(keys.is_array()&&!keys.empty()&&keys.size()<=200,"Invalid template key list.");return definition;
}
inline std::map<std::string,const Json*> Settings(const Json& page){
    std::map<std::string,const Json*> result;const auto& settings=page.at("Settings");Require(settings.is_array()&&settings.size()<=200,"Invalid settings definition.");
    for(const auto& setting:settings)if(setting.contains("key")){const auto key=setting.at("key").get<std::string>();Require(result.emplace(key,&setting).second,"Duplicate setting key.");}
    return result;
}
inline void Validate(const Json& setting,const Json& value){
    const auto type=setting.at("type").get<std::string>();bool valid=false;
    if(type=="toggle")valid=value.is_boolean();
    else if(type=="slider"&&value.is_number()){
        const auto number=value.get<double>(),lo=setting.at("min").get<double>(),hi=setting.at("max").get<double>();
        valid=std::isfinite(number)&&std::isfinite(lo)&&std::isfinite(hi)&&lo<hi&&number>=lo&&number<=hi;
    }else if(type=="choice"&&value.is_string()){
        const auto& options=setting.at("options");valid=options.is_array()&&std::find(options.begin(),options.end(),value)!=options.end();
    }else if(type=="text"&&setting.value("presentation",std::string{})=="color"){
        valid=value.is_string()&&ModMenuColor::Valid(value.get_ref<const std::string&>(),setting.value("colorAlpha",false));
    }else if(type=="text"&&value.is_string()){
        const auto& text=value.get_ref<const std::string&>();valid=text.size()<=setting.value("maxLength",60u)&&text.find_first_of("\r\n")==text.npos&&text.find('\0')==text.npos;
    }
    Require(valid,"A template value has an invalid type or is outside its mod-defined range.");
}
inline std::set<std::string> Keys(const Json& page){
    const auto settings=Settings(page);std::set<std::string> keys;
    for(const auto& field:Contract(page).at("keys")){
        Require(field.is_string(),"Invalid template key.");const auto key=field.get<std::string>();const auto found=settings.find(key);
        Require(found!=settings.end()&&keys.insert(key).second,"Missing or duplicate template setting.");
        const auto& setting=*found->second;Require(!setting.value("locked",false)&&setting.at("type")!="key"&&setting.at("type")!="button","Template includes a locked or unsupported setting.");
    }return keys;
}
inline std::string Context(const Json& page){
    const auto context=page.value("TemplateContext",std::string{});
    if(Contract(page).contains("contextVariable"))Require(!context.empty()&&context.size()<=256,"Wait until the mod confirms its selected character or target.");
    return context;
}
inline Json Export(const Json& page,std::string name={}){
    const auto& definition=Contract(page);(void)Context(page);const auto settings=Settings(page);Json values=Json::object();
    for(const auto& key:Keys(page)){const auto& value=page.at("Values").at(key);Validate(*settings.at(key),value);values[key]=value;}
    Require(name.size()<=80,"Template name is too long.");
    Json result={{"format","RuneSchema.ModMenu2.Template"},{"version",1},{"mod",page.at("Id")},{"template",definition.at("id")},{"templateVersion",definition.at("version")},{"values",std::move(values)}};
    if(!name.empty())result["name"]=std::move(name);if(page.contains("Version"))result["modVersion"]=page.at("Version");Require(result.dump().size()<=MaximumBytes,"Template is too large.");return result;
}
inline Json Parse(const std::string& text){
    Require(text.size()<=MaximumBytes,"Template exceeds 256 KiB.");
    return Json::parse(text,[](int depth,Json::parse_event_t,Json&){Require(depth<=16,"Template nesting is too deep.");return true;});
}
inline Json Prepare(const Json& page,const Json& document){
    Require(document.is_object()&&document.dump().size()<=MaximumBytes,"Invalid template document.");
    for(const auto& [key,value]:document.items())Require(key=="format"||key=="version"||key=="mod"||key=="template"||key=="templateVersion"||key=="values"||key=="name"||key=="modVersion","Unknown template field.");
    Require(document.at("format")=="RuneSchema.ModMenu2.Template"&&document.at("version").is_number_integer()&&document.at("version")==1,"Unsupported template format.");
    const auto& definition=Contract(page);
    Require(document.at("mod")==page.at("Id")&&document.at("template")==definition.at("id")&&document.at("templateVersion").is_number_integer()&&document.at("templateVersion")==definition.at("version"),"Template belongs to another mod or an incompatible template version.");
    if(document.contains("name"))Require(document.at("name").is_string()&&document.at("name").get_ref<const std::string&>().size()<=80,"Invalid template name.");
    const auto& values=document.at("values");const auto keys=Keys(page);Require(values.is_object()&&values.size()==keys.size(),"A full template must contain exactly its declared settings.");
    const auto settings=Settings(page);Json expected=Json::object();
    for(const auto& [key,value]:values.items()){Require(keys.contains(key),"Template contains an undeclared setting.");Validate(*settings.at(key),value);expected[key]=page.at("Values").at(key);}
    return {{"Op","template_apply"},{"Id",page.at("Id")},{"TemplateId",definition.at("id")},{"TemplateVersion",definition.at("version")},{"Revision",page.at("Revision")},{"Context",Context(page)},{"Values",values},{"Expected",std::move(expected)}};
}
inline bool Current(const Json& page,const Json& plan){
    try{if(page.at("Id")!=plan.at("Id")||page.at("Revision")!=plan.at("Revision")||Contract(page).at("id")!=plan.at("TemplateId")||Contract(page).at("version")!=plan.at("TemplateVersion")||Context(page)!=plan.at("Context").get<std::string>())return false;
        for(const auto& [key,value]:plan.at("Expected").items())if(page.at("Values").at(key)!=value)return false;return true;
    }catch(...){return false;}
}
}
