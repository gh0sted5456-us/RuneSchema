#pragma once
#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace PS::ManagedModStartup {
struct Request {std::wstring Owner,Name;std::filesystem::path Root;bool Lua=false,Native=false;};
enum class Result:int {Queued=0,Invalid=1,Collision=2,TooLate=3};
inline std::wstring Fold(std::wstring value){std::transform(value.begin(),value.end(),value.begin(),towlower);return value;}
inline bool SafeName(std::wstring_view value){return !value.empty()&&value.size()<=255&&value!=L"."&&value!=L".."&&value.back()!=L'.'&&value.back()!=L' '&&value.find_first_of(L"/\\:<>\"|?*")==value.npos&&std::none_of(value.begin(),value.end(),[](wchar_t c){return c<32;});}
// The host accepts only the container itself (legacy) or one complete child
// package, never an arbitrary descendant elsewhere in an owner's mod.
inline bool PackageRelative(const std::filesystem::path& relative,std::wstring_view name,std::wstring_view owner){
    if(relative.empty()||relative.is_absolute())return false;
    std::vector<std::wstring> parts;for(const auto& part:relative)parts.push_back(Fold(part.wstring()));
    if(parts.size()==1)return parts[0]==L"ue4ss"&&Fold(std::wstring(name))==Fold(std::wstring(owner));
    return parts.size()==2&&parts[0]==L"ue4ss"&&parts[1]==Fold(std::wstring(name))&&SafeName(parts[1]);
}
class Queue {
    std::mutex Gate;bool Closed=false;std::map<std::wstring,std::filesystem::path> Seen;std::vector<Request> Pending,Ready;
public:
    Result Add(Request request){std::lock_guard lock(Gate);if(Closed)return Result::TooLate;
        if(request.Name.empty()||request.Owner.empty()||request.Root.empty()||(!request.Lua&&!request.Native)||Seen.size()>=4096)return Result::Invalid;
        const auto key=Fold(request.Name);if(Seen.contains(key))return Result::Collision;
        Seen[key]=request.Root;Pending.push_back(std::move(request));return Result::Queued;}
    std::vector<Request> Take(){std::lock_guard lock(Gate);std::vector<Request> out;out.swap(Pending);return out;}
    void Confirm(Request request){std::lock_guard lock(Gate);Ready.push_back(std::move(request));}
    std::vector<Request> Snapshot(){std::lock_guard lock(Gate);return Ready;}
    bool Owns(std::wstring name){std::lock_guard lock(Gate);return Seen.contains(Fold(std::move(name)));}
    void Close(){std::lock_guard lock(Gate);Closed=true;}
    void Reset(){std::lock_guard lock(Gate);Closed=false;Seen.clear();Pending.clear();Ready.clear();}
};
}
