#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
namespace PS::JournalWorkspace {
using Json=nlohmann::json;
inline bool Identifier(const std::string& value){return !value.empty()&&value.size()<=128&&std::all_of(value.begin(),value.end(),[](unsigned char c){return std::isalnum(c)||c=='_'||c=='-';});}
inline Json Draft(const std::string& id,const std::string& type,const std::string& title,const Json& pages,const std::string& image,const std::string& category,const Json& fields=Json::object()) {
    if(!Identifier(id))throw std::runtime_error("Use a unique mod-prefixed ID: letters, numbers, underscore or dash.");
    if(type!="Lore"&&type!="People"&&type!="Place"&&type!="Treasure"&&type!="World"&&type!="Recipe")throw std::runtime_error("Unsupported journal entry type.");
    if(title.empty()||title.size()>1024||!pages.is_array()||pages.empty()||pages.size()>128)throw std::runtime_error("A title and 1-128 pages are required.");
    size_t total=0;Json descriptions=Json::array();
    for(const auto& page:pages){if(!page.is_string()||page.get_ref<const std::string&>().size()>65536)throw std::runtime_error("Each page must be text, at most 64 KiB.");total+=page.get_ref<const std::string&>().size();descriptions.push_back({{"Description",page}});}
    if(total>262144)throw std::runtime_error("Entry text exceeds 256 KiB.");
    const auto asset=[](const std::string& p){return p.size()<=2048&&p.starts_with("/")&&p.find('.')!=p.npos&&p.find("..")==p.npos&&p.find_first_of("\\\r\n\t:")==p.npos;};
    if(!category.empty()&&!asset(category))throw std::runtime_error("Choose a full native journal subcategory asset path.");
    if(!image.empty()&&!asset(image))throw std::runtime_error("Image must be a cooked Unreal asset path, not a wiki URL or local image.");
    Json body={{"Type",type},{"DisplayName",title},{"PageDescriptions",descriptions},{"Unlock",false}};
    if(!image.empty())body["Image"]=image;
    if(!category.empty())body["AddTo"]={{"SubCategory",category}};
    if(!fields.is_object()||fields.size()>64||fields.dump().size()>65536)throw std::runtime_error("Additional journal fields must be a bounded object.");
    for(const auto& [key,value]:fields.items()){if(key=="Type"||key=="DisplayName"||key=="PageDescriptions"||key=="Image")throw std::runtime_error("Use the normal journal controls for title, type, artwork and pages.");body[key]=value;}
    for(const auto* key:{"Unlock","UnlockOnAcquire"})if(body.contains(key)&&!body[key].is_boolean())throw std::runtime_error("Journal unlock flags must be boolean.");
    if(body.contains("AddTo")){const auto& placement=body["AddTo"];if(!placement.is_object()||placement.contains("SubCategory")==bool(placement.contains("Section")||placement.contains("Category")))throw std::runtime_error("Choose a native subcategory OR a section/category pair.");if(!placement.contains("SubCategory")&&(!placement.contains("Section")||!placement.contains("Category")||placement["Section"].get<std::string>().empty()||placement["Category"].get<std::string>().empty()))throw std::runtime_error("Both journal Section and Category are required.");if(placement.contains("Group")&&(!placement["Group"].is_object()||!Identifier(placement["Group"].value("Id",std::string{}))))throw std::runtime_error("A valid journal group ID is required.");}
    if(type=="Recipe"&&(!body.contains("RecipeData")||!body["RecipeData"].is_string()||body["RecipeData"].get_ref<const std::string&>().empty()))throw std::runtime_error("Recipe entries require RecipeData.");
    if(body.contains("UnlockCondition")){const auto& condition=body["UnlockCondition"];if(!condition.is_object())throw std::runtime_error("UnlockCondition must be an object.");auto trigger=condition.value("UnlockType",std::string{});const auto sep=trigger.rfind("::");if(sep!=trigger.npos)trigger=trigger.substr(sep+2);if(trigger!="PlayerStart"&&trigger!="ItemFirstPickup"&&trigger!="RecipeUnlocked"&&trigger!="Interaction"&&trigger!="AIKill"&&trigger!="RegionUnlocked"&&trigger!="Manual"&&trigger!="NeverUnlock")throw std::runtime_error("Choose a native journal unlock type.");
        const auto required=trigger=="ItemFirstPickup"?"UnlockingItemData":trigger=="RecipeUnlocked"?"UnlockingRecipe":trigger=="Interaction"?"UnlockingInteractionActor":trigger=="AIKill"?"AIData":"";if(*required&&(!condition.contains(required)||!condition[required].is_string()||!condition[required].get_ref<const std::string&>().starts_with('/')))throw std::runtime_error("This unlock condition requires its cooked trigger asset.");
        if(condition.contains("MapRegion")&&(!condition["MapRegion"].is_string()||!condition["MapRegion"].get_ref<const std::string&>().starts_with('/')))throw std::runtime_error("MapRegion requires a cooked asset path.");if(condition.contains("RegionID")&&!condition["RegionID"].is_number_integer())throw std::runtime_error("RegionID must be an integer.");
    }
    return {{id,body}};
}
inline Json Cache(const Json& entries,const std::string& key,bool complete) {
    if(!entries.is_array()||entries.size()>8192||key.size()>1024)throw std::runtime_error("Invalid journal cache.");
    return {{"Version",1},{"SourceKey",key},{"ScanComplete",complete},{"Entries",entries},{"Coverage","Loaded native assets; completeness of the full game journal is not certified."}};
}
inline Json ReadCache(const Json& doc) {
    if(!doc.is_object()||doc.value("Version",0)!=1||!doc.contains("Entries")||!doc["Entries"].is_array()||doc["Entries"].size()>8192)throw std::runtime_error("Journal cache format is unsupported.");
    if(!doc.contains("SourceKey")||!doc["SourceKey"].is_string()||doc["SourceKey"].get_ref<const std::string&>().size()>1024)throw std::runtime_error("Invalid journal cache source key.");
    size_t budget=0;for(const auto& row:doc["Entries"]){if(!row.is_object()||!row.value("Native",false)||!row.contains("Path")||!row["Path"].is_string()||row["Path"].get_ref<const std::string&>().size()>2048||!row["Path"].get_ref<const std::string&>().starts_with("/"))throw std::runtime_error("Invalid native journal cache row.");budget+=row.dump().size();if(budget>16*1024*1024)throw std::runtime_error("Journal cache exceeds its bound.");}
    for(const auto& row:doc["Entries"]){
        for(const auto* key:{"Name","Type","PersistenceID","InternalName","Icon","Description","SourceKind","SourceName","ModifiedBy"})
            if(row.contains(key)&&(!row[key].is_string()||row[key].get_ref<const std::string&>().size()>65536))throw std::runtime_error("Invalid journal text field.");
        for(const auto* key:{"ModifiedByRuneSchema","ObservedCurrent"})if(row.contains(key)&&!row[key].is_boolean())throw std::runtime_error("Invalid journal source flag.");
        if(row.contains("Pages")){if(!row["Pages"].is_array()||row["Pages"].size()>128)throw std::runtime_error("Invalid journal pages.");for(const auto& page:row["Pages"])if(!page.is_string()||page.get_ref<const std::string&>().size()>65536)throw std::runtime_error("Invalid journal page text.");}
    }
    return doc;
}
}
