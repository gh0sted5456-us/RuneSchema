#pragma once
#include "Core/ConfigFiles.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <shared_mutex>
#include <mutex>
#include <atomic>
#include <set>
#include <sstream>
#include <optional>
#include <windows.h>

namespace PS::HelpyPersonalization {
using Json=nlohmann::json;
inline std::shared_mutex Gate;
inline Json Entries=Json::object();
inline std::filesystem::path File;
inline std::string Baseline,Status;
inline bool Existed=false,Initialized=false,Ready=false;
inline std::atomic<uint64_t> Revision=0;
inline std::string Fold(std::string value){for(auto& c:value)if(c>='A'&&c<='Z')c+=32;return value;}
inline std::string Trim(std::string value){const auto first=value.find_first_not_of(" \t\r\n");return first==value.npos?std::string{}:value.substr(first,value.find_last_not_of(" \t\r\n")-first+1);}
inline bool Text(const std::string& value,size_t max=192){return value.size()<=max&&std::none_of(value.begin(),value.end(),[](unsigned char c){return c<32||c==127;})&&(value.empty()||MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0)>0);}
inline std::string ItemKey(const Json& row){
    if(!row.is_object())return {};for(const char* field:{"Path","AssetPath"})if(row.contains(field)&&row[field].is_string()){
        const auto path=row[field].get<std::string>();if(path.starts_with('/')&&!path.starts_with("//")&&path.size()<=4096&&Text(path,4096))return "item:"+Fold(path);
    }return {};
}
inline std::string ModKey(const std::string& owner){return "mod:"+Fold(owner);}
inline std::string Guid(std::string value){if(value.size()>80)return {};std::string out;for(const unsigned char c:value){if(c=='-'||c=='{'||c=='}')continue;if(!std::isxdigit(c))return {};out+=static_cast<char>(std::tolower(c));}return out.size()==32&&out!=std::string(32,'0')?out:std::string{};}
inline bool StackKeyValid(const std::string& key){return key.size()>73&&key.size()<=4168&&key.starts_with("stack:")&&key[38]==':'&&key[71]==':'&&Guid(key.substr(6,32))==key.substr(6,32)&&Guid(key.substr(39,32))==key.substr(39,32)&&key[72]=='/'&&key.find("..",72)==key.npos&&key[73]!='/'&&Text(key,4168);}
inline std::string StackKey(const Json& row){
    if(!row.is_object()||!row.value("PlayerGuid",Json{}).is_string()||!row.contains("Instance")||!row["Instance"].is_array()||row["Instance"].size()!=4)return {};
    const auto player=Guid(row["PlayerGuid"].get<std::string>()),item=ItemKey(row);if(player.empty()||item.empty())return {};
    std::string instance;for(const auto& word:row["Instance"]){if(!word.is_number_integer()||word.get<int64_t>()<0||word.get<uint64_t>()>UINT32_MAX)return {};char text[9]{};std::snprintf(text,sizeof(text),"%08x",word.get<uint32_t>());instance+=text;}
    if(instance==std::string(32,'0'))return {};return "stack:"+player+":"+instance+":"+item.substr(5);
}
inline std::string TypeKey(const Json& row){const auto item=ItemKey(row);if(!row.is_object()||!row.contains("PlayerGuid"))return item;const auto guid=row["PlayerGuid"].is_string()?Guid(row["PlayerGuid"].get<std::string>()):std::string{};return guid.empty()||item.empty()?std::string{}:"type:"+guid+":"+item.substr(5);}
inline bool TypeKeyValid(const std::string& key){return key.size()>39&&key.starts_with("type:")&&key[37]==':'&&Guid(key.substr(5,32))==key.substr(5,32)&&ItemKey(Json{{"Path",key.substr(38)}})=="item:"+key.substr(38)&&key.find("..",38)==key.npos;}
inline Json Get(const std::string& key){std::shared_lock lock(Gate);const auto found=Entries.find(key);return found==Entries.end()?Json::object():*found;}
inline Json Effective(const Json& row){
    auto result=Get(ItemKey(row));const auto type=TypeKey(row);
    if(type!=ItemKey(row)){result["Favorite"]=false;auto tags=result.value("Tags",Json::array());Json clean=Json::array();for(const auto& tag:tags)if(Fold(tag.get<std::string>())!="junk")clean.push_back(tag);result["Tags"]=clean;}
    const std::string prefix=row.value("MarksReadOnly",false)?"remote:":"";
    for(const auto& key:{type,StackKey(row)})if(!key.empty()){const auto mark=Get(prefix+key);for(const auto& [field,value]:mark.items())result[field]=value;}
    return result;
}
inline std::string Nickname(const std::string& key){const auto row=Get(key);return row.value("Nickname",std::string{});}
inline bool Favorite(const Json& row){return Effective(row).value("Favorite",false);}
inline bool Locked(const Json& row){return Effective(row).value("Locked",false);}
inline bool Tagged(const Json& row,const std::string& tag){const auto data=Effective(row);if(!data.contains("Tags"))return false;const auto folded=Fold(tag);for(const auto& value:data["Tags"])if(Fold(value.get<std::string>())==folded)return true;return false;}
inline bool BulkEligible(const Json& row,bool junk){return !row.value("Equipped",false)&&!ItemKey(row).empty()&&!Locked(row)&&!Favorite(row)&&(!junk||Tagged(row,"junk"));}
inline std::string SearchText(const Json& row){const auto data=Get(ItemKey(row));std::string text=data.value("Nickname",std::string{});if(data.contains("Tags"))for(const auto& value:data["Tags"])text+=" "+value.get<std::string>();return text;}
inline bool QueryUsesLabels(const std::string& text){std::istringstream input(Fold(text));std::vector<std::string> words;std::string word;while(input>>word)words.push_back(word);if(words.empty())return false;
    std::shared_lock lock(Gate);for(const auto& [key,row]:Entries.items())if(key.starts_with("item:")){std::string labels=row.value("Nickname",std::string{});if(row.contains("Tags"))for(const auto& tag:row["Tags"])labels+=" "+tag.get<std::string>();labels=Fold(labels);for(const auto& value:words)if(labels.find(value)!=labels.npos)return true;}return false;}
inline Json Validate(const Json& document){
    if(!document.is_object()||document.value("Format",0)!=1||!document.contains("Entries")||!document["Entries"].is_object()||document["Entries"].size()>16384)throw std::runtime_error("Invalid Helpy personalization file.");
    auto output=Json::object();for(const auto& [key,row]:document["Entries"].items()){
        const auto identity=key.starts_with("remote:")?key.substr(7):key;const bool stack=StackKeyValid(identity),type=TypeKeyValid(identity);
        if((!key.starts_with("item:/")&&!key.starts_with("mod:")&&!stack&&!type)||!Text(key,4175)||!row.is_object()||row.size()>(row.contains("LegacyFavoriteImported")?5u:4u))throw std::runtime_error("Invalid personalization key.");
        const auto nickname=row.value("Nickname",std::string{});if(!Text(nickname))throw std::runtime_error("Nickname is too long or contains invalid characters.");
        Json entry={{"Nickname",Trim(nickname)},{"Favorite",row.value("Favorite",false)},{"Locked",row.value("Locked",false)},{"Tags",Json::array()}};
        if(row.contains("Tags")){if(!row["Tags"].is_array()||row["Tags"].size()>12)throw std::runtime_error("Use at most 12 tags per item.");std::set<std::string> seen;
            for(const auto& value:row["Tags"]){const auto tag=Trim(value.get<std::string>());if(tag.empty()||!Text(tag,48))throw std::runtime_error("Tags must contain 1-48 bytes of visible text.");if(seen.insert(Fold(tag)).second)entry["Tags"].push_back(tag);}}
        if(stack||type)for(const char* field:{"Nickname","Favorite","Locked","Tags"})if(!row.contains(field))entry.erase(field);
        if(row.contains("LegacyFavoriteImported")){
            if(!stack||key.starts_with("remote:")||row.at("LegacyFavoriteImported")!=true)throw std::runtime_error("Invalid legacy favorite import receipt");
            entry["LegacyFavoriteImported"]=true;
        }
        output[Fold(key)]=std::move(entry);
    }return output;
}
inline void Safe(const std::filesystem::path& path){for(auto current=std::filesystem::absolute(path).lexically_normal();!current.empty();){const auto flags=GetFileAttributesW(current.c_str());if(flags!=INVALID_FILE_ATTRIBUTES&&(flags&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Personalization storage must not contain links.");const auto parent=current.parent_path();if(parent==current)break;current=parent;}}
inline bool Load(const std::filesystem::path& file){std::unique_lock lock(Gate);File=file;Initialized=true;Ready=false;try{Safe(file);Existed=std::filesystem::exists(file);Baseline=Existed?ConfigFiles::Read(file,4*1024*1024):std::string{};Entries=Existed?Validate(Json::parse(Baseline)):Json::object();Status.clear();Ready=true;++Revision;return true;}catch(const std::exception& error){Status=error.what();return false;}}
inline bool SetMany(const Json& patches,std::optional<uint64_t> expectedRevision=std::nullopt){
    std::unique_lock lock(Gate);try{
        if(!Initialized||!Ready||File.empty()||!patches.is_object()||patches.empty())throw std::runtime_error("Personalization storage or a stable item identity is unavailable.");
        if(expectedRevision&&*expectedRevision!=Revision.load())throw std::runtime_error("Item marks changed; refresh before marking again.");
        uint64_t hash=1469598103934665603ull;for(unsigned char c:Fold(std::filesystem::absolute(File).generic_string())){hash^=c;hash*=1099511628211ull;}
        const auto name=L"Local\\RuneSchema-Personalization-"+std::to_wstring(hash);HANDLE mutex=CreateMutexW(nullptr,FALSE,name.c_str());
        if(!mutex)throw std::runtime_error("Cannot lock personalization settings.");const auto wait=WaitForSingleObject(mutex,1000);if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED){CloseHandle(mutex);throw std::runtime_error("Personalization settings are busy.");}
        struct Release{HANDLE Value;~Release(){ReleaseMutex(Value);CloseHandle(Value);}}release{mutex};
        Safe(File);const bool exists=std::filesystem::exists(File);if(exists!=Existed||(exists&&ConfigFiles::Read(File,4*1024*1024)!=Baseline))throw std::runtime_error("Personalization changed in another window. Reload before saving.");
        auto next=Entries;for(const auto& [key,patch]:patches.items()){
            if(patch.is_null()){next.erase(Fold(key));continue;}
            if(!patch.is_object())throw std::runtime_error("Invalid mark patch.");
            auto value=next.value(Fold(key),Json::object());const auto identity=key.starts_with("remote:")?key.substr(7):key;
            for(const auto& [field,data]:patch.items()){if((StackKeyValid(identity)||TypeKeyValid(identity))&&data.is_null())value.erase(field);else value[field]=data;}next[Fold(key)]=std::move(value);
        }
        next=Validate({{"Format",1},{"Entries",next}});const auto text=Json{{"Format",1},{"Entries",next}}.dump(2);if(text.size()>4*1024*1024)throw std::runtime_error("Personalization settings exceed 4 MiB.");
        if(next==Entries)return true;ConfigFiles::Write(File,text);Entries=std::move(next);Baseline=text;Existed=true;Status="Saved.";++Revision;return true;
    }catch(const std::exception& error){Status=error.what();return false;}
}
inline bool Set(const std::string& key,const Json& patch,std::optional<uint64_t> expectedRevision=std::nullopt){return SetMany(Json{{key,patch}},expectedRevision);}
}
