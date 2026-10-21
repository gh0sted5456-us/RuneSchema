#pragma once
#include "Core/ConfigFiles.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <vector>
#include <set>
#include <algorithm>
#include <Windows.h>
namespace PS::HelpyNavigationOrder {
using Json=nlohmann::json;
inline Json Orders=Json::object();inline std::filesystem::path File;inline std::string Baseline,Status;inline bool Initialized=false,Ready=false,Existed=false;
inline Json Validate(const Json& document){
    if(!document.is_object()||document.value("Format",0)!=1||!document.contains("Orders")||!document["Orders"].is_object()||document["Orders"].size()>128)throw std::runtime_error("Invalid navigation layout.");
    const auto& orders=document["Orders"];for(const auto& [name,order]:orders.items()){
        if(name.empty()||name.size()>64||name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-_")!=name.npos||!order.is_array()||order.size()>128)throw std::runtime_error("Invalid navigation surface.");
        std::set<int> seen;for(const auto& id:order)if(!id.is_number_integer()||id<0||id>65535||!seen.insert(id.get<int>()).second)throw std::runtime_error("Invalid navigation identity.");
    }return orders;
}
inline std::vector<int> Ordered(const std::string& surface,const std::vector<int>& defaults){
    std::vector<int> result;std::set<int> seen;const auto found=Orders.find(surface);if(found!=Orders.end())for(const auto& id:*found){const auto value=id.get<int>();if(std::find(defaults.begin(),defaults.end(),value)!=defaults.end()&&seen.insert(value).second)result.push_back(value);}
    for(int value:defaults)if(seen.insert(value).second)result.push_back(value);return result;
}
inline void Safe(const std::filesystem::path& file){for(auto current=std::filesystem::absolute(file).lexically_normal();!current.empty();){const auto attr=GetFileAttributesW(current.c_str());if(attr!=INVALID_FILE_ATTRIBUTES&&(attr&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Navigation preferences cannot contain links.");const auto parent=current.parent_path();if(parent==current)break;current=parent;}}
inline bool Load(const std::filesystem::path& file){File=file;Initialized=true;Ready=false;try{Safe(file);Existed=std::filesystem::exists(file);Baseline=Existed?ConfigFiles::Read(file,128*1024):std::string{};Orders=Existed?Validate(Json::parse(Baseline)):Json::object();Ready=true;Status.clear();return true;}catch(const std::exception& error){Status=error.what();return false;}}
inline bool Save(Json next){try{
    if(!Ready||File.empty())throw std::runtime_error("Navigation storage unavailable; layout was not saved.");next=Validate({{"Format",1},{"Orders",next}});
    uint64_t hash=1469598103934665603ull;for(wchar_t c:std::filesystem::absolute(File).wstring()){hash^=towlower(c);hash*=1099511628211ull;}
    const auto name=L"Local\\RuneSchema-Navigation-"+std::to_wstring(hash);HANDLE mutex=CreateMutexW(nullptr,FALSE,name.c_str());if(!mutex)throw std::runtime_error("Cannot lock navigation preferences.");
    const auto wait=WaitForSingleObject(mutex,1000);if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED){CloseHandle(mutex);throw std::runtime_error("Navigation preferences busy.");}
    struct Close{HANDLE Value;~Close(){ReleaseMutex(Value);CloseHandle(Value);}}close{mutex};Safe(File);
    const bool exists=std::filesystem::exists(File);if(exists!=Existed||(exists&&ConfigFiles::Read(File,128*1024)!=Baseline))throw std::runtime_error("Navigation changed in another window. Restart Helpy before saving.");
    const auto text=Json{{"Format",1},{"Orders",next}}.dump(2);if(text.size()>128*1024)throw std::runtime_error("Navigation layout exceeds its size limit.");ConfigFiles::Write(File,text);Orders=std::move(next);Baseline=text;Existed=true;Status.clear();return true;
}catch(const std::exception& error){Status=error.what();return false;}}
inline bool Move(const std::string& surface,int from,int to,const std::vector<int>& defaults){auto order=Ordered(surface,defaults);auto source=std::find(order.begin(),order.end(),from),target=std::find(order.begin(),order.end(),to);if(source==order.end()||target==order.end()||source==target)return false;const auto destination=std::distance(order.begin(),target);order.erase(source);order.insert(order.begin()+destination,from);auto next=Orders;next[surface]=order;return Save(std::move(next));}
inline bool Reset(const std::string& surface){auto next=Orders;next.erase(surface);return Save(std::move(next));}
}
