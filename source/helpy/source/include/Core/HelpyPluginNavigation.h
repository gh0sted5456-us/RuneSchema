#pragma once
#include "Runtime/RuneSchemaPluginApi.h"
#include <algorithm>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

namespace PS::HelpyPluginNavigation {
struct Tab {std::string Owner,Route,Title,Icon;RuneSchemaNavDrawFn Draw{};void* Context{};};
inline std::shared_mutex Mutex;
inline std::map<std::string,Tab> Tabs;
inline bool Label(const char* text,size_t limit,bool empty=false) {
    if(!text)return false;
    size_t n=0;for(;n<=limit&&text[n];++n)if(static_cast<unsigned char>(text[n])<32||text[n]=='#')return false;
    return n<=limit&&(empty||n>0);
}
inline int32_t Register(const char* owner,const RuneSchemaNavTabV1* tab) {
    if(!Label(owner,128)||!tab||tab->StructSize<sizeof(*tab)||!tab->Draw||
        !Label(tab->Route,192)||!Label(tab->Title,80)||!Label(tab->Icon,32,true))return RS_PLUGIN_INVALID_ARGUMENT;
    const std::string route=tab->Route,prefix=std::string(owner)+".";
    if(!route.starts_with(prefix)||route.size()==prefix.size()||
        !std::all_of(route.begin(),route.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='_'||c=='-';}))return RS_PLUGIN_INVALID_ARGUMENT;
    std::unique_lock lock(Mutex);
    if(Tabs.contains(route))return RS_PLUGIN_DUPLICATE;
    if(Tabs.size()>=64)return RS_PLUGIN_FAILED;
    Tabs.emplace(route,Tab{owner,route,tab->Title,tab->Icon,tab->Draw,tab->Context});return RS_PLUGIN_OK;
}
inline std::vector<Tab> Read() {
    std::shared_lock lock(Mutex);std::vector<Tab> rows;
    for(const auto& [_,tab]:Tabs)rows.push_back({tab.Owner,tab.Route,tab.Title,tab.Icon});
    return rows; // Never let a copied callback outlive the unload gate.
}
inline void Remove(const std::string& owner) {
    std::unique_lock lock(Mutex);std::erase_if(Tabs,[&](const auto& row){return row.second.Owner==owner;});
}
inline int32_t Draw(const std::string& route,const RuneSchemaNavUiV1& ui) {
    std::shared_lock lock(Mutex);const auto found=Tabs.find(route);
    if(found==Tabs.end())return RS_PLUGIN_NOT_FOUND;
    try{return found->second.Draw(found->second.Context,&ui);}catch(...){return RS_PLUGIN_FAILED;}
}
}
