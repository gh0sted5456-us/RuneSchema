#pragma once
#include "Runtime/RuneSchemaPluginApi.h"
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace PS::HelpyPluginNavigation {
// Description, IconPath and IconFlags come from the optional Home tile fields; Legal from the notice field.
struct Tab {std::string Owner,Route,Title,Icon;RuneSchemaNavDrawFn Draw{};void* Context{};std::string Description;std::filesystem::path IconPath;uint32_t IconFlags=0;std::string Legal;};
// The original V1 tab ends at Context; older plugins pass exactly that size.
inline constexpr size_t BaseTabSize=offsetof(RuneSchemaNavTabV1,Context)+sizeof(void*);
inline constexpr size_t TileTabSize=offsetof(RuneSchemaNavTabV1,IconFlags)+sizeof(uint32_t);
inline constexpr size_t LegalTabSize=offsetof(RuneSchemaNavTabV1,Legal)+sizeof(const char*);
inline constexpr size_t MaxLegalBytes=1200;
inline std::shared_mutex Mutex;
inline std::map<std::string,Tab> Tabs;
// One pending route requested by an owned plugin event; the window takes it on its next frame.
inline std::string PendingRoute;
inline bool Label(const char* text,size_t limit,bool empty=false) {
    if(!text)return false;
    size_t n=0;for(;n<=limit&&text[n];++n)if(static_cast<unsigned char>(text[n])<32||text[n]=='#')return false;
    return n<=limit&&(empty||n>0);
}
// A legal notice: printable text and line breaks, at most MaxLegalBytes.
inline bool Notice(const char* text){
    if(!text)return false;size_t n=0;
    for(;n<=MaxLegalBytes&&text[n];++n){const auto c=static_cast<unsigned char>(text[n]);if(c<32&&c!='\n')return false;}
    return n<=MaxLegalBytes;
}
// A tile icon is a plain .png file name directly in the plugin's folder.
inline bool IconFile(const char* name){
    if(!Label(name,96))return false;const std::string_view text(name);
    if(text.size()<5||text.find_first_of("/\\:")!=text.npos||text.starts_with("."))return false;
    auto extension=std::string(text.substr(text.size()-4));for(auto& c:extension)c=static_cast<char>(c>='A'&&c<='Z'?c+32:c);
    return extension==".png";
}
inline int32_t Register(const char* owner,const RuneSchemaNavTabV1* tab,const std::filesystem::path& pluginRoot={}) {
    if(!Label(owner,128)||!tab||tab->StructSize<BaseTabSize||!tab->Draw||
        !Label(tab->Route,192)||!Label(tab->Title,80)||!Label(tab->Icon,32,true))return RS_PLUGIN_INVALID_ARGUMENT;
    std::string description;std::filesystem::path icon;uint32_t flags=0;
    if(tab->StructSize>=TileTabSize){
        if(tab->Description){if(!Label(tab->Description,240,true))return RS_PLUGIN_INVALID_ARGUMENT;description=tab->Description;}
        if(tab->IconImage&&*tab->IconImage){
            if(!IconFile(tab->IconImage)||pluginRoot.empty())return RS_PLUGIN_INVALID_ARGUMENT;
            icon=pluginRoot/std::filesystem::u8path(tab->IconImage);
        }
        if(tab->IconFlags&~RS_NAV_ICON_FULL_COLOR)return RS_PLUGIN_INVALID_ARGUMENT;
        flags=tab->IconFlags;
    }
    std::string legal;
    if(tab->StructSize>=LegalTabSize&&tab->Legal&&*tab->Legal){if(!Notice(tab->Legal))return RS_PLUGIN_INVALID_ARGUMENT;legal=tab->Legal;}
    const std::string route=tab->Route,prefix=std::string(owner)+".";
    if(!route.starts_with(prefix)||route.size()==prefix.size()||
        !std::all_of(route.begin(),route.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='_'||c=='-';}))return RS_PLUGIN_INVALID_ARGUMENT;
    std::unique_lock lock(Mutex);
    if(Tabs.contains(route))return RS_PLUGIN_DUPLICATE;
    if(Tabs.size()>=64)return RS_PLUGIN_FAILED;
    Tabs.emplace(route,Tab{owner,route,tab->Title,tab->Icon,tab->Draw,tab->Context,std::move(description),std::move(icon),flags,std::move(legal)});return RS_PLUGIN_OK;
}
inline std::vector<Tab> Read() {
    std::shared_lock lock(Mutex);std::vector<Tab> rows;
    for(const auto& [_,tab]:Tabs)rows.push_back({tab.Owner,tab.Route,tab.Title,tab.Icon,{},{},tab.Description,tab.IconPath,tab.IconFlags,tab.Legal});
    return rows; // Never let a copied callback outlive the unload gate.
}
inline void Remove(const std::string& owner) {
    std::unique_lock lock(Mutex);std::erase_if(Tabs,[&](const auto& row){return row.second.Owner==owner;});
    if(PendingRoute.starts_with(owner+"."))PendingRoute.clear();
}
// Selects a route only if that owner registered it. Opening never authorizes anything.
inline int32_t RequestOpen(const std::string& owner,const std::string& route) {
    std::unique_lock lock(Mutex);const auto found=Tabs.find(route);
    if(found==Tabs.end()||found->second.Owner!=owner)return RS_PLUGIN_NOT_FOUND;
    PendingRoute=route;return RS_PLUGIN_OK;
}
inline std::string TakePendingRoute() {std::unique_lock lock(Mutex);return std::exchange(PendingRoute,{});}
inline int32_t Draw(const std::string& route,const RuneSchemaNavUiV1& ui) {
    std::shared_lock lock(Mutex);const auto found=Tabs.find(route);
    if(found==Tabs.end())return RS_PLUGIN_NOT_FOUND;
    try{return found->second.Draw(found->second.Context,&ui);}catch(...){return RS_PLUGIN_FAILED;}
}
}
