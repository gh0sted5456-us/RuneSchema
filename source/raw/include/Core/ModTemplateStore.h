#pragma once
#include "Core/CacheBundleIO.h"
#include <cmath>
#include <nlohmann/json.hpp>
#include <cwctype>
#include <set>

namespace PS::ModTemplates {
using Json=nlohmann::json;namespace fs=std::filesystem;
inline constexpr size_t MaximumBytes=64*1024;
inline std::wstring Wide(const std::string& text){
    if(text.size()>INT_MAX)throw std::runtime_error("UTF-8 value is too large");
    if(text.empty())return {};
    const auto count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);
    if(!count)throw std::runtime_error("Invalid UTF-8");
    std::wstring value(count,0);if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),value.data(),count))throw std::runtime_error("Invalid UTF-8");return value;
}
inline std::string Utf8(const fs::path& path){const auto value=path.u8string();return {reinterpret_cast<const char*>(value.data()),value.size()};}
inline Json Parse(const std::string& bytes){
    if(bytes.empty()||bytes.size()>MaximumBytes)throw std::runtime_error("Template JSON exceeds 64 KiB or is empty");
    std::vector<std::set<std::string>> keys;
    auto result=Json::parse(bytes,[&](int depth,Json::parse_event_t event,Json& value){
        if(depth>32)throw std::runtime_error("Template JSON nesting exceeds 32 levels");
        if(event==Json::parse_event_t::object_start)keys.emplace_back();
        else if(event==Json::parse_event_t::object_end)keys.pop_back();
        else if(event==Json::parse_event_t::key&&!keys.back().insert(value.get<std::string>()).second)throw std::runtime_error("Duplicate JSON key");
        return true;
    });
    if(!result.is_object())throw std::runtime_error("Template root must be a JSON object");return result;
}
inline void ValidateValue(const Json& value,int depth=0){
    if(depth>32)throw std::runtime_error("Template JSON nesting exceeds 32 levels");
    if(value.is_number_float()&&!std::isfinite(value.get<double>()))throw std::runtime_error("JSON numbers must be finite");
    if(value.is_structured())for(const auto& child:value)ValidateValue(child,depth+1);
}
inline std::string Serialize(const Json& value){
    if(!value.is_object())throw std::runtime_error("Template root must remain an object");
    ValidateValue(value);const auto bytes=value.dump();if(bytes.size()>MaximumBytes)throw std::runtime_error("Template JSON exceeds 64 KiB");return bytes;
}
inline std::vector<std::string> Pointer(const std::string& text){
    if(text.size()>512||text.find('\0')!=text.npos)throw std::runtime_error("JSON Pointer is too long or contains NUL");
    (void)Wide(text);std::vector<std::string> result;if(text.empty())return result;
    if(text.front()!='/')throw std::runtime_error("JSON Pointer must begin with slash");
    size_t start=1;
    do{
        const auto slash=text.find('/',start);const auto token=text.substr(start,slash==text.npos?slash:slash-start);std::string decoded;
        for(size_t i=0;i<token.size();++i){if(token[i]!='~')decoded+=token[i];else{if(++i==token.size()||(token[i]!='0'&&token[i]!='1'))throw std::runtime_error("Invalid JSON Pointer escape");decoded+=token[i]=='0'?'~':'/';}}
        result.push_back(std::move(decoded));if(result.size()>32)throw std::runtime_error("JSON Pointer has too many segments");
        if(slash==text.npos)break;start=slash+1;
    }while(true);return result;
}
inline size_t Index(const std::string& token,size_t count,bool append){
    if(append&&token=="-")return count;
    if(token.empty()||(token.size()>1&&token[0]=='0'))throw std::runtime_error("Invalid JSON array index");
    size_t value=0;for(auto c:token){if(c<'0'||c>'9'||value>(count/10)+1)throw std::runtime_error("Invalid JSON array index");value=value*10+(c-'0');if(value>count)throw std::runtime_error("JSON array index is out of range");}
    if(!append&&value==count)throw std::runtime_error("JSON array index is out of range");return value;
}
inline const Json& Read(const Json& document,const std::string& text){
    const Json* value=&document;for(const auto& token:Pointer(text)){
        if(value->is_object()){if(!value->contains(token))throw std::runtime_error("JSON object key not found");value=&value->at(token);}
        else if(value->is_array())value=&value->at(Index(token,value->size(),false));
        else throw std::runtime_error("JSON Pointer crosses a scalar");
    }return *value;
}
inline void Set(Json& document,const std::string& text,const Json& replacement){
    const auto tokens=Pointer(text);Json candidate=document;
    if(tokens.empty())candidate=replacement;
    else{
        Json* value=&candidate;
        for(size_t i=0;i+1<tokens.size();++i){const auto& token=tokens[i];
            if(value->is_object()){if(!value->contains(token))throw std::runtime_error("Missing intermediate JSON container");value=&value->at(token);}
            else if(value->is_array())value=&value->at(Index(token,value->size(),false));
            else throw std::runtime_error("JSON Pointer crosses a scalar");
        }
        const auto& final=tokens.back();
        if(value->is_object())(*value)[final]=replacement;
        else if(value->is_array()){const auto index=Index(final,value->size(),true);if(index==value->size())value->push_back(replacement);else value->at(index)=replacement;}
        else throw std::runtime_error("JSON Pointer parent is not a container");
    }
    (void)Serialize(candidate);document.swap(candidate);
}
inline Json Scalar(unsigned kind,double number,const std::string& text,bool boolean){
    switch(kind){case 0:return nullptr;case 1:return boolean;case 2:if(!std::isfinite(number))throw std::runtime_error("Number must be finite");return number;
        case 3:(void)Wide(text);return text;case 4:return Json::array();case 5:return Json::object();default:throw std::runtime_error("Unknown JSON kind");}
}
inline unsigned Kind(const Json& value){return value.is_null()?0:value.is_boolean()?1:value.is_number()?2:value.is_string()?3:value.is_array()?4:5;}
inline void Array(const Json& value,bool numbers){
    if(!value.is_array()||value.size()>1024)throw std::runtime_error("Bulk JSON arrays require at most 1024 values");
    for(const auto& entry:value)if(numbers?(!entry.is_number()||!std::isfinite(entry.get<double>())):!entry.is_string())throw std::runtime_error("Bulk JSON array type changed");
}
inline bool Owner(const std::string& owner){
    return !owner.empty()&&owner.size()<=96&&owner!="."&&owner!=".."&&std::all_of(owner.begin(),owner.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.';});
}
inline fs::path FileName(const std::string& id){
    if(id.empty()||id.size()>240||id.back()==' '||id.back()=='.'||id.find_first_of("<>:\"/\\|?*")!=id.npos||id.find('\0')!=id.npos
        ||std::any_of(id.begin(),id.end(),[](unsigned char c){return c<32||c==127;}))throw std::runtime_error("Template ID must be a safe JSON basename");
    const fs::path file(Wide(id));auto ext=file.extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),::towlower);
    if(ext!=L".json"||file.stem().empty())throw std::runtime_error("Template ID must end in .json");
    auto base=file.filename().wstring();base.resize(base.find('.'));std::transform(base.begin(),base.end(),base.begin(),::towupper);
    if(base==L"CON"||base==L"PRN"||base==L"AUX"||base==L"NUL"||(base.size()==4&&(base.starts_with(L"COM")||base.starts_with(L"LPT"))&&base[3]>=L'0'&&base[3]<=L'9'))throw std::runtime_error("Reserved template filename");
    return file;
}
struct Listing {std::vector<std::string> Ids,Labels;size_t Files=0;};
class Store {
    fs::path Root;
public:
    Store(const fs::path& documents,const std::string& owner){if(!Owner(owner))throw std::runtime_error("Invalid template owner");Root=documents/"SavedGames/RSDragonwilds"/owner;CacheBundleIO::Safe(Root);}
    Listing List()const{
        Listing result;CacheBundleIO::Safe(Root);if(!fs::exists(Root))return result;
        if(!fs::is_directory(Root))throw std::runtime_error("Template root is not a directory");
        for(const auto& entry:fs::directory_iterator(Root)){
            CacheBundleIO::Safe(entry.path());if(entry.is_directory())continue;
            if(!entry.is_regular_file()||++result.Files>1024)throw std::runtime_error("Template directory is unsafe or exceeds 1024 files");
            auto ext=entry.path().extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),::towlower);if(ext!=L".json")continue;
            const auto id=Utf8(entry.path().filename());(void)FileName(id);result.Ids.push_back(id);
        }
        std::sort(result.Ids.begin(),result.Ids.end());for(const auto& id:result.Ids)result.Labels.push_back(Utf8(FileName(id).stem()));return result;
    }
    Json Read(const std::string& id)const{return Parse(CacheBundleIO::Read(Root/FileName(id),MaximumBytes));}
    std::string Create(const Json& document)const{
        const auto bytes=Serialize(document);if(List().Files>=1024)throw std::runtime_error("Template directory has reached 1024 files");
        CacheBundleIO::Safe(Root);fs::create_directories(Root);CacheBundleIO::Safe(Root);
        unsigned char random[16];if(BCryptGenRandom(nullptr,random,sizeof(random),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)throw std::runtime_error("Cannot generate a template ID");
        constexpr char hex[]="0123456789abcdef";std::string id="template-";for(auto byte:random){id+=hex[byte>>4];id+=hex[byte&15];}id+=".json";
        const auto target=CacheBundleIO::NativePath(Root/FileName(id)),temporary=fs::path(target.wstring()+L".tmp");CacheBundleIO::Safe(target);CacheBundleIO::Safe(temporary);
        HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot stage a new template");
        DWORD written=0;const bool ok=WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)&&written==bytes.size()&&FlushFileBuffers(file);CloseHandle(file);
        if(!ok){DeleteFileW(temporary.c_str());throw std::runtime_error("Cannot flush the new template");}
        // No replacement flag: a copied template is never overwritten.
        try{CacheBundleIO::Safe(Root);CacheBundleIO::Safe(temporary);CacheBundleIO::Safe(target);}catch(...){DeleteFileW(temporary.c_str());throw;}
        if(!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_WRITE_THROUGH)){DeleteFileW(temporary.c_str());throw std::runtime_error("Cannot commit a new template without replacement");}
        return id;
    }
};
}
