#pragma once
#include "Core/CacheBundleIO.h"
#include "Core/ConfigFiles.h"
#include <nlohmann/json.hpp>
#include <algorithm>

namespace PS::HelpyWindowPlacement {
using Json=nlohmann::json;
inline Json Windows=Json::object();inline std::filesystem::path File;inline std::string Baseline,Status;inline bool Ready=false,Existed=false;
inline RECT Fit(RECT value,RECT work,int minimumWidth,int minimumHeight){
    const LONG width=std::clamp<LONG>(value.right-value.left,std::min<LONG>(minimumWidth,work.right-work.left),work.right-work.left);
    const LONG height=std::clamp<LONG>(value.bottom-value.top,std::min<LONG>(minimumHeight,work.bottom-work.top),work.bottom-work.top);
    const LONG left=std::clamp(value.left,work.left,work.right-width),top=std::clamp(value.top,work.top,work.bottom-height);return {left,top,left+width,top+height};
}
inline void Load(const std::filesystem::path& path){File=path;Ready=false;try{CacheBundleIO::Safe(path);Existed=std::filesystem::exists(path);Baseline=Existed?ConfigFiles::Read(path,64*1024):std::string{};Windows=Existed?Json::parse(Baseline):Json::object();
    if(!Windows.is_object()||Windows.size()>16)throw std::runtime_error("Invalid window placement preferences");
    for(const auto& [key,row]:Windows.items()){if(key.empty()||key.size()>32||!row.is_array()||row.size()!=5)throw std::runtime_error("Invalid window placement entry");for(const auto& value:row)if(!value.is_number_integer()||value<-1048576||value>1048576)throw std::runtime_error("Window placement bounds invalid");}
    Ready=true;Status.clear();}catch(const std::exception& error){Windows=Json::object();Status=error.what();}}
inline void Remember(HWND window,const std::string& key){
    if(!Ready||!window||IsIconic(window)||IsZoomed(window))return;RECT r{};if(!GetWindowRect(window,&r))return;
    if(r.right<=r.left||r.bottom<=r.top)return;const auto dpi=GetDpiForWindow(window);const Json bounds={r.left,r.top,r.right,r.bottom,dpi?int(dpi):96};if(Windows.value(key,Json())==bounds)return;
    Windows[key]=bounds;try{CacheBundleIO::Safe(File);const bool exists=std::filesystem::exists(File);if(exists!=Existed||(exists&&ConfigFiles::Read(File,64*1024)!=Baseline))throw std::runtime_error("Window preferences changed elsewhere; existing file retained");
        const auto text=Windows.dump();ConfigFiles::Write(File,text);Baseline=text;Existed=true;Status.clear();}catch(const std::exception& error){Status=error.what();}
}
inline void Restore(HWND window,const std::string& key,int minimumWidth,int minimumHeight){
    if(!window||!Windows.contains(key))return;const auto& row=Windows.at(key);RECT rect{row[0].get<LONG>(),row[1].get<LONG>(),row[2].get<LONG>(),row[3].get<LONG>()};
    if(rect.right<=rect.left||rect.bottom<=rect.top)return;MONITORINFO monitor{sizeof(monitor)};if(!GetMonitorInfoW(MonitorFromRect(&rect,MONITOR_DEFAULTTONEAREST),&monitor))return;
    const auto saved=row[4].get<int>();const auto dpi=GetDpiForWindow(window);if(saved>=72&&saved<=384&&dpi){rect.right=rect.left+MulDiv(rect.right-rect.left,int(dpi),saved);rect.bottom=rect.top+MulDiv(rect.bottom-rect.top,int(dpi),saved);}
    rect=Fit(rect,monitor.rcWork,minimumWidth,minimumHeight);SetWindowPos(window,nullptr,rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,SWP_NOACTIVATE|SWP_NOZORDER);
}
}
