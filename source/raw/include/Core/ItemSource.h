#pragma once
#include "Core/ContentSource.h"
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <stdexcept>
namespace PS::ItemSource {
enum class Kind { All, Vanilla, Expansion, Modded, Unknown };
inline constexpr const char* Labels[]{"All sources","Base Game","Plugins","Mods","Unknown"};
inline const char* Label(Kind kind){return Labels[static_cast<unsigned>(kind)];}
inline const char* Token(Kind kind){switch(kind){case Kind::Vanilla:return "vanilla";case Kind::Expansion:return "expansion";case Kind::Modded:return "modded";case Kind::Unknown:return "unknown";default:return "all";}}
inline Kind Decode(std::string_view value){
    for(const auto kind:{Kind::All,Kind::Vanilla,Kind::Expansion,Kind::Modded,Kind::Unknown})if(value==Token(kind)||value==Label(kind))return kind;
    if(ContentSource::Valid(std::string(value)))return value=="core"?Kind::Vanilla:value.starts_with("mods/")?Kind::Modded:Kind::Expansion;
    if(value=="Vanilla")return Kind::Vanilla;if(value=="Expansion")return Kind::Expansion;if(value=="Modded")return Kind::Modded;
    return Kind::Unknown;
}
struct Info {Kind Group=Kind::Unknown;std::string Name,Root,Evidence;bool Modified=false;std::string ModifiedBy;std::set<std::string> Containers;};
inline std::string DisplayName(const Info& info){
    if(info.Group==Kind::Vanilla)return "Base Game";
    if(info.Group==Kind::Unknown||info.Name.empty())return "Unknown";
    std::string key;for(unsigned char c:HelpyCacheSources::Lower(info.Name))if(c>='a'&&c<='z'||c>='0'&&c<='9')key+=char(c);
    if(info.Group==Kind::Modded){
        if(key.starts_with("dye")||key=="jobboarddyequests"||key.starts_with("colorfuldye")||key.starts_with("runeschemadye"))return "Dyes";
        if(key=="rsdw"||key=="rsdwspears"||key=="spearsscythes"||key=="spearsandscythes"||key=="runeschemaspearsscythes")return "Spears & Scythes";
        if(key=="flintlocks"||key=="flintlockpistols"||key=="runeschemaflintlockpistols"||key=="gunslinger")return "Flintlock Pistols";
        if(key=="shadowmancer"||key=="shadowmancerarmor"||key=="runeschemashadowmancer")return "Shadowmancer";
    }
    if(info.Group==Kind::Expansion){
        if(key=="umbralsands")return "Umbral Sands";
        if(key=="scornedwilderness"||key=="scorchedwilderness")return "Scorched Wilderness";
        if(key=="dowdunreach")return "Dowdun Reach";
    }
    std::string out;const auto& name=info.Name;
    const auto upper=[](unsigned char c){return c>='A'&&c<='Z';};
    const auto lower=[](unsigned char c){return c>='a'&&c<='z';};
    for(size_t i=0;i<name.size();++i){const unsigned char c=name[i];
        if(c=='_'||c=='-'||c=='/'||c=='.'||c==' '){if(!out.empty()&&out.back()!=' ')out+=' ';continue;}
        if(i&&!out.empty()&&upper(c)&&out.back()!=' '&&(lower(name[i-1])||(upper(name[i-1])&&i+1<name.size()&&lower(name[i+1]))))out+=' ';
        out+=char((out.empty()||out.back()==' ')&&lower(c)?c-32:c);
    }
    while(!out.empty()&&out.back()==' ')out.pop_back();return out.empty()?"Unknown":out;
}
inline Info Describe(const std::string& path,const HelpyCacheSources::Plan& plan={},const std::set<std::string>& nativePlugins={},const std::string& exactOwner={},bool declaredModded=false,bool patched=false,const std::string& patchOwner={}){
    const auto root=ContentSource::Root(path),lower=HelpyCacheSources::Lower(root);
    if(root.empty())return {};
    const auto leaf=root.substr(root.find_last_of('/')+1),mount=HelpyCacheSources::Source(path);
    if(!exactOwner.empty()&&!ContentSource::Mod(exactOwner).empty())return {Kind::Modded,exactOwner,root,"Native clone/definition provenance"};
    const auto full=HelpyCacheSources::Lower(path);
    if(full.starts_with("/engine/transient")||full.starts_with("/temp/"))return {Kind::Unknown,leaf,root};
    const auto package=HelpyCacheSources::AssetKey(path);
    if(plan.ContainerAssets){const auto found=plan.ContainerAssets->find(package);
        if(found!=plan.ContainerAssets->end()&&!found->second.empty()){
            Info info{Kind::Unknown,{},root,"Container suppliers conflict; precedence unverified"};
            if(plan.ContainerFiles){const auto files=plan.ContainerFiles->find(package);if(files!=plan.ContainerFiles->end())info.Containers=files->second;}
            if(found->second.size()==1){const auto& owner=*found->second.begin();info.Evidence="Container directory index";
                if(owner=="core"){info.Group=Kind::Vanilla;info.Name="Dragonwilds";}
                else if(owner.starts_with("mods/")){info.Group=Kind::Modded;info.Name=owner.substr(5);}
                else if(owner.starts_with("plugins/")){info.Group=Kind::Expansion;info.Name=owner.substr(8);}
                info.Modified=patched;info.ModifiedBy=patchOwner;
            }
            return info;
        }
    }
    // Creation provenance can introduce a clone anywhere. Patching an existing
    // core/official-plugin asset cannot change its original content provider.
    if(lower=="/game"||lower=="/engine")return {Kind::Vanilla,"Dragonwilds",root,"Base-game package namespace",patched,patchOwner};
    if(nativePlugins.contains(HelpyCacheSources::Lower(leaf)))return {Kind::Expansion,leaf,root,"Official game content plugin",patched,patchOwner};
    if(plan.AssetOwners){const auto found=plan.AssetOwners->find(HelpyCacheSources::AssetKey(path));
        if(found!=plan.AssetOwners->end()&&!found->second.empty())return {Kind::Modded,found->second.size()==1?*found->second.begin():std::string{},root,found->second.size()==1?"Exact registry declaration":"Conflicting registry declarations"};
    }
    if(plan.AuthoredAssets&&lower!="/game"&&lower!="/engine"){const auto found=plan.AuthoredAssets->find(HelpyCacheSources::AssetKey(path));
        if(found!=plan.AuthoredAssets->end()&&!found->second.empty())return {Kind::Modded,found->second.size()==1?*found->second.begin():std::string{},root,found->second.size()==1?"Exact RuneSchema asset definition":"Conflicting asset definitions"};
    }
    if(lower.starts_with("/game/mods/")||lower.starts_with("/game/runeschema/")||lower.starts_with("/game/gameplay/mods/"))return {Kind::Modded,leaf,root};
    if(declaredModded)return {Kind::Modded,leaf,root};
    // Installed native plugin descriptors are collected by the existing source
    // worker. Never inspect the filesystem or guess from item/icon names here.
    if(const auto found=plan.DeclaredOwners.find(mount);found!=plan.DeclaredOwners.end()&&!found->second.empty()){
        // A mount may be borrowed official content. Dependency membership
        // alone proves neither creation nor ownership of this particular item.
        return {Kind::Unknown,{},root,"Mount dependency only; item provider unverified"};
    }
    if(lower.starts_with("/gamefeatures/plugins/")||lower.starts_with("/plugins/gamefeatures/"))return {Kind::Unknown,leaf,root,"Plugin provider unverified"};
    return {Kind::Unknown,leaf,root};
}
inline nlohmann::json Fields(const Info& info){return {{"SourceKind",Token(info.Group)},{"SourceName",info.Name},{"SourceRoot",info.Root},{"SourceEvidence",info.Evidence},{"ModifiedByRuneSchema",info.Modified},{"ModifiedBy",info.ModifiedBy},{"SourceContainers",info.Containers}};}
inline Info Row(const nlohmann::json& row){
    if(!row.is_object())return {};
    const auto text=[&](const char* key){const auto found=row.find(key);return found!=row.end()&&found->is_string()?found->get<std::string>():std::string{};};
    const auto flag=[&](const char* key){const auto found=row.find(key);return found!=row.end()&&found->is_boolean()&&found->get<bool>();};
    auto info=Describe(text("Path"),{},{},{},flag("RuntimeClone")||flag("DeclaredModded"));
    if(flag("RuntimeClone")){info.Group=Kind::Modded;info.Evidence="Native clone/definition provenance";}
    const bool containerEvidence=text("SourceEvidence")=="Container directory index"||text("SourceEvidence")=="Container suppliers conflict; precedence unverified";
    const bool nativeCore=info.Group==Kind::Vanilla&&!flag("RuntimeClone")&&!containerEvidence;
    const auto kind=text("SourceKind");if(!kind.empty()&&!nativeCore)info.Group=Decode(kind);
    if(row.contains("SourceName")&&row["SourceName"].is_string())info.Name=text("SourceName"); // an explicit unknown owner must stay unknown
    const auto root=text("SourceRoot");if(!root.empty())info.Root=root;
    info.Evidence=text("SourceEvidence");
    if(nativeCore){info.Name="Dragonwilds";info.Evidence="Base-game package namespace";}
    info.Modified=(info.Group==Kind::Vanilla||info.Group==Kind::Expansion)&&(flag("ModifiedByRuneSchema")||(flag("RuneSchemaManaged")&&!flag("RuntimeClone")));
    info.ModifiedBy=text("ModifiedBy");
    if(const auto containers=row.find("SourceContainers");containers!=row.end()&&containers->is_array()&&containers->size()<=32)
        for(const auto& value:*containers)if(value.is_string()&&value.get_ref<const std::string&>().size()<=2048)info.Containers.insert(value.get<std::string>());
    return info;
}
inline Info Reattribute(const nlohmann::json& row,const HelpyCacheSources::Plan& plan,const std::set<std::string>& nativePlugins){
    if(row.value("RuntimeClone",false))return Row(row);
    // Helpy receives native presentation rows, not the full package index.
    // Cache import/export obtains separate bounded, live ownership proofs.
    if(!plan.ContainerAssets&&(row.value("SourceEvidence",std::string{})=="Container directory index"
        ||row.value("SourceEvidence",std::string{})=="Container suppliers conflict; precedence unverified"))return Row(row);
    auto info=Describe(row.value("Path",std::string{}),plan,nativePlugins,{},false,
        row.value("ModifiedByRuneSchema",false)||row.value("RuneSchemaManaged",false),row.value("ModifiedBy",std::string{}));
    if(info.Group==Kind::Unknown){
        const auto previous=Row(row);
        if(previous.Group==Kind::Modded&&(previous.Evidence=="Exact registry declaration"||previous.Evidence=="Exact RuneSchema asset definition"))return previous;
    }
    return info;
}
inline std::string Provider(const Info& info){
    const auto name=HelpyCacheSources::Lower(info.Name);
    const auto value=info.Group==Kind::Vanilla?"core":info.Group==Kind::Modded?ContentSource::Mod(name):info.Group==Kind::Expansion?"plugins/"+name:std::string{};
    return ContentSource::Valid(value)?value:std::string{};
}
inline bool Matches(Kind filter,const Info& info,const std::string& provider={}){return (filter==Kind::All||filter==info.Group)&&(provider.empty()||Provider(info)==provider);}
inline std::string ProviderToken(const std::string& provider){
    if(provider.empty())return {};if(!ContentSource::Valid(provider)||provider!=HelpyCacheSources::Lower(provider))throw std::runtime_error("Invalid item provider filter");
    std::string out="__helpyowner:";for(unsigned char c:provider){out+="0123456789abcdef"[c>>4];out+="0123456789abcdef"[c&15];}return out+" ";
}
inline std::string TakeProvider(std::string& query){
    constexpr std::string_view prefix="__helpyowner:";if(!query.starts_with(prefix))return {};
    const auto end=query.find(' '),count=(end==query.npos?query.size():end)-prefix.size();
    if(!count||count>400||count%2)throw std::runtime_error("Invalid item provider encoding");
    std::string provider;for(size_t i=prefix.size();i<prefix.size()+count;i+=2){const auto a=std::string_view("0123456789abcdef").find(query[i]),b=std::string_view("0123456789abcdef").find(query[i+1]);
        if(a==std::string_view::npos||b==std::string_view::npos)throw std::runtime_error("Invalid item provider encoding");provider+=char(a*16+b);}
    (void)ProviderToken(provider);query=end==query.npos?std::string{}:query.substr(end+1);return provider;
}
inline std::string Query(Kind source,const std::string& category,const std::string& text,const std::string& provider={}){
    std::string query;if(source!=Kind::All)query="__helpysource:"+std::string(Token(source))+" ";
    query+=ProviderToken(provider);
    if(category!="All")query+="__helpycat:"+HelpyCacheSources::Lower(category)+" ";
    query+=text;while(!query.empty()&&query.back()==' ')query.pop_back();return query;
}
inline Kind TakeFilter(std::string& query){
    constexpr std::string_view prefix="__helpysource:";if(!query.starts_with(prefix))return Kind::All;
    const auto end=query.find(' ');const auto token=query.substr(prefix.size(),end==query.npos?std::string::npos:end-prefix.size());
    const auto kind=Decode(token);if(token!=Token(kind))throw std::runtime_error("Invalid item source filter");
    query=end==query.npos?std::string{}:query.substr(end+1);return kind;
}
}
