#pragma once
#include "Core/RuneSchemaUpdateInfo.h"
#include <nlohmann/json.hpp>

namespace PS::HelpyHotfixInfo {
using Json=nlohmann::json;
inline bool Tag(const std::string& tag,const std::string& installed) {
    if(tag.find('+')!=std::string::npos||std::count(tag.begin(),tag.end(),'.')!=3)return false;
    const auto remote=RuneSchemaUpdateInfo::Parse(tag),local=RuneSchemaUpdateInfo::Parse(installed);
    if(!remote||!local||remote->Parts!=local->Parts)return false;
    auto suffix=remote->Suffix;for(auto& c:suffix)if(c>='A'&&c<='Z')c+=32;
    return suffix=="hf"||suffix=="-hf";
}
inline bool Asset(const Json& asset,const std::string& tag,const std::string& platform){
    if(!asset.is_object()||!asset.value("id",Json{}).is_number_integer()||asset["id"]<=0
        ||asset.value("state",std::string{})!="uploaded"||!asset.value("size",Json{}).is_number_integer()
        ||asset["size"]<=0||asset["size"]>536870912)return false;
    const auto digest=asset.value("digest",std::string{});
    const auto name="RuneSchema-"+tag+"-"+platform+"-hotfix.zip";
    return asset.value("name",std::string{})==name&&digest.size()==71&&digest.starts_with("sha256:")
        &&digest.find_first_not_of("0123456789abcdef",7)==std::string::npos
        &&asset.value("browser_download_url",std::string{})=="https://github.com/gh0sted5456-us/RuneSchema/releases/download/"+tag+"/"+name;
}
inline Json Select(const Json& releases,const std::string& installed){
    Json best;std::string date;
    if(!releases.is_array()||releases.size()>100)return best;
    for(const auto& release:releases)try{
        if(!release.is_object()||release.value("draft",true)||!release.value("id",Json{}).is_number_integer()||release["id"]<=0)continue;
        const auto tag=release.value("tag_name",std::string{}),published=release.value("published_at",std::string{});
        if(!Tag(tag,installed)||published.size()!=20||published.back()!='Z'||!release.value("assets",Json{}).is_array())continue;
        Json assets=Json::array();for(const auto& asset:release["assets"])for(const auto* platform:{"Win64","WinGDK"})if(Asset(asset,tag,platform)){
            Json bounded;for(const auto* key:{"id","state","size","digest","name","browser_download_url"})bounded[key]=asset.at(key);assets.push_back(std::move(bounded));
        }
        if(assets.empty()||assets.size()>2||published<date)continue;
        best={{"id",release["id"]},{"tag_name",tag},{"published_at",published},{"assets",std::move(assets)}};date=published;
    }catch(...){continue;}
    return best;
}
inline Json ForPlatform(const Json& release,const std::string& platform){
    if(!release.is_object())return {};
    Json found;for(const auto& asset:release.value("assets",Json::array()))if(Asset(asset,release.value("tag_name",std::string{}),platform)){if(!found.is_null())return {};found=asset;}
    return found;
}
inline std::string Identity(const Json& release,const Json& asset){return "hotfix:"+std::to_string(release.at("id").get<std::uint64_t>())+":"+std::to_string(asset.at("id").get<std::uint64_t>())+":"+asset.at("digest").get<std::string>();}
}
