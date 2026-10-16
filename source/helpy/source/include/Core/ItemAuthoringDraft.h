#pragma once
#include "Generator/ItemCloneRequest.h"
#include "Core/HelpyKitFiles.h"
#include <array>
#include <cstdio>
#include <chrono>

namespace PS::ItemAuthoring {
using Json=nlohmann::json;
enum class Mode { Transmog, Creation };
struct Draft {
    Json Source=Json::object(),Appearance=Json::object(),Inspection=Json::object(),Prepared=Json::object(),Result=Json::object();
    std::array<char,Authoring::MaxCloneDocumentBytes+1> Editor{};
    char Name[257]{},Icon[2049]{},Mod[65]="MyClones",Filter[257]{};
    std::string Id,Error,Status,ExportPath;
    bool Give=true,Permanent=true,Acknowledged=false,InspectWanted=false,ExportWanted=false,Ready=false,Created=false;
    int Count=1;
    uint64_t Pending=0,Revision=0,SentRevision=0;
    std::chrono::steady_clock::time_point Requested{};
    Draft(){Editor[0]='{';Editor[1]='}';}
    void Changed(){if(Created)Id.clear();++Revision;Prepared=Result=Json::object();ExportWanted=false;Acknowledged=false;Created=false;Error.clear();Status.clear();}
    void SetSource(const Json& row){
        Authoring::ValidateObjectPath(row.at("Path").get<std::string>());
        Source=row;Appearance=Inspection=Json::object();Ready=false;InspectWanted=true;Id.clear();
        Editor.fill(0);Editor[0]='{';Editor[1]='}';Icon[0]=0;
        std::snprintf(Name,sizeof(Name),"%s",(row.value("Name",std::string("Item"))+" Clone").c_str());Changed();
    }
};
inline Json Preview(const Draft& draft,Mode mode){
    auto row=draft.Source;if(row.empty())return row;
    row["_HelpyIconPreview"]=!draft.Created;
    if(draft.Name[0])row["Name"]=draft.Name;
    if(draft.Icon[0])row["Icon"]=draft.Icon;
    else if(mode==Mode::Transmog&&draft.Appearance.contains("Icon")&&draft.Appearance["Icon"].is_string()&&!draft.Appearance["Icon"].get_ref<const std::string&>().empty())row["Icon"]=draft.Appearance["Icon"];
    // Once prepared/created, show the native receipt rather than stale source
    // metadata. This also preserves an explicit icon override in the editor.
    if(draft.Result.contains("Icon")&&draft.Result["Icon"].is_string())row["Icon"]=draft.Result["Icon"];
    if(draft.Created&&draft.Result.value("CompanionsReady",true)){
        row["Path"]=draft.Result.value("Path",row.value("Path",std::string{}));row["RuntimeClone"]=true;
    }
    return row;
}
inline Json ParseValues(std::string_view text){
    if(text.size()>Authoring::MaxCloneDocumentBytes)throw std::runtime_error("Item values exceed 256 KiB.");
    auto values=Json::parse(text,[](int depth,Json::parse_event_t,Json&){if(depth>16)throw std::runtime_error("Item values exceed 16 nesting levels.");return true;},true,true);
    if(!values.is_object()||values.size()>Authoring::MaxCloneFields)throw std::runtime_error("Item values must be a JSONC object with at most 512 fields.");
    return values;
}
inline void SplitValues(const Json& values,const Json& inspection,Json& direct,Json& stats){
    direct=stats=Json::object();
    if(!inspection.contains("Fields")||!inspection["Fields"].is_array())throw std::runtime_error("Inspect the source before editing values.");
    for(const auto& [key,value]:values.items()){
        const auto& fields=inspection["Fields"];
        const auto found=std::find_if(fields.begin(),fields.end(),[&](const Json& field){return field.value("Name",std::string{})==key;});
        if(found==fields.end()||!found->value("Editable",false))throw std::runtime_error("Not an editable native field: "+key);
        if(value.dump().size()>Authoring::MaxCloneValueBytes)throw std::runtime_error("Native field exceeds 16 KiB: "+key);
        if(key.starts_with("@row/")){
            const auto at=key.find('/',5);
            if(at==key.npos||!Authoring::SafeFieldName(key.substr(5,at-5))||!Authoring::SafeFieldName(key.substr(at+1)))throw std::runtime_error("Invalid linked stat field.");
            stats[key.substr(5,at-5)][key.substr(at+1)]=value;
        }else{
            if(!Authoring::SafeFieldName(key))throw std::runtime_error("Item identity is managed automatically: "+key);
            direct[key]=value;
        }
    }
}
inline Json Request(Draft& draft,Mode mode,const std::string& player,bool prepare){
    if(!draft.Ready)throw std::runtime_error("Wait for the source inspection.");
    if(mode==Mode::Transmog&&draft.Appearance.empty())throw std::runtime_error("Choose an appearance item.");
    Json direct,stats;SplitValues(mode==Mode::Creation?ParseValues(draft.Editor.data()):Json::object(),draft.Inspection,direct,stats);
    const bool permanent=prepare||draft.Permanent;
    const std::string tag=permanent?draft.Mod:"RuneSchema";
    if(!ClonePresentation::ValidId(draft.Id,tag))draft.Id=ClonePresentation::NewId(tag);
    auto request=Authoring::CloneRequest(draft.Source.at("Path"),draft.Name,draft.Icon,std::move(direct),permanent,prepare?false:draft.Give,draft.Count,
        prepare||draft.Acknowledged,prepare?"export":player,tag,draft.Id,mode==Mode::Transmog?draft.Appearance.value("Path",std::string{}):std::string{},"","",permanent?draft.Mod:"");
    request["StatOverrides"]=std::move(stats);request["IconMode"]=draft.Icon[0]?"Override":"Auto";
    if(prepare){request["Action"]="PrepareClone";request.erase("Player");request.erase("AcknowledgeExperimental");}
    return request;
}
inline std::string Jsonc(const Json& document){
    return "// RuneSchema item definition. Keep matching assets/ and raw/ files together.\n// $Clone inherits the source; listed fields override it.\n"+document.dump(2)+"\n";
}
inline std::filesystem::path Export(const std::filesystem::path& root,const Json& prepared){
    if(prepared.value("State",std::string{})!="Prepared"||!prepared.contains("Documents"))throw std::runtime_error("Prepare the current item before exporting.");
    const auto name=prepared.at("InternalName").get<std::string>();
    if(!Authoring::SafeFileStem(name))throw std::runtime_error("Invalid prepared item filename.");
    const auto& documents=prepared.at("Documents");
    if(!documents.is_object()||!documents.contains("assets")||!documents["assets"].is_object()||documents["assets"].size()!=1)throw std::runtime_error("Prepared item is missing its /assets definition.");
    for(const auto& [kind,document]:documents.items()){
        if(kind!="assets"&&kind!="raw"&&kind!="recipes"&&kind!="journal")throw std::runtime_error("Unsupported export folder.");
        if(!document.is_object()||document.dump().size()>4*1024*1024)throw std::runtime_error("Invalid export document.");
    }
    static std::atomic<uint64_t> sequence=0;
    HelpyKitFiles::NoRedirect(root);std::filesystem::create_directories(root);
    const auto stamp=std::chrono::system_clock::now().time_since_epoch().count();
    const auto directory=root/(name+"_"+std::to_string(stamp)+"_"+std::to_string(++sequence));
    if(!std::filesystem::create_directory(directory))throw std::runtime_error("Cannot reserve an item export folder.");
    for(const auto& [kind,document]:documents.items()){
        if(document.empty())continue;
        const auto file=directory/kind/(name+".jsonc");ConfigFiles::Write(file,Jsonc(document));
        if(Json::parse(ConfigFiles::Read(file,4*1024*1024),nullptr,true,true)!=document)throw std::runtime_error("Item export verification failed.");
    }
    return directory;
}
}
