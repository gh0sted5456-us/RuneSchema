#pragma once
#include "Core/ConfigFiles.h"
#include <nlohmann/json.hpp>
#include <mutex>
#include <optional>
#include <algorithm>
#include <vector>
#include <string>
#include <string_view>
#include <stdexcept>

namespace PS::CrashDumpConfiguration {
using Json=nlohmann::json;
inline std::mutex Gate;
inline constexpr size_t MaxBytes=256*1024;
inline std::string_view Trim(std::string_view value){
    const auto first=value.find_first_not_of(" \t\r");if(first==value.npos)return {};
    return value.substr(first,value.find_last_not_of(" \t\r")-first+1);
}
struct Option {bool Value;std::optional<std::pair<size_t,size_t>> Span;};
struct Settings {Option Dumping{true,{}},Full{false,{}};bool Section=false;size_t Insert=0;};
inline Settings Parse(const std::string& text){
    if(text.size()>MaxBytes||text.find('\0')!=text.npos)throw std::runtime_error("Invalid UE4SS settings size or text");
    Settings result;bool inside=false;result.Insert=text.size();
    for(size_t at=0;at<text.size();){
        auto end=text.find('\n',at);if(end==text.npos)end=text.size();
        auto line=Trim(std::string_view(text).substr(at,end-at));
        if(at==0&&line.starts_with("\xef\xbb\xbf"))line=Trim(line.substr(3));
        line=Trim(line.substr(0,line.find_first_of(";#")));
        if(line.starts_with("[")&&line.ends_with("]")){
            if(inside)result.Insert=at;
            inside=Trim(line.substr(1,line.size()-2))=="CrashDump";
            if(inside){if(result.Section)throw std::runtime_error("Duplicate CrashDump section; edit UE4SS settings manually");result.Section=true;}
        }else if(inside){
            const auto equals=line.find('=');if(equals!=line.npos){
                const auto name=Trim(line.substr(0,equals)),value=Trim(line.substr(equals+1));
                auto* option=name=="EnableDumping"?&result.Dumping:name=="FullMemoryDump"?&result.Full:nullptr;
                if(option){
                    if(option->Span)throw std::runtime_error("Duplicate crash-dump setting; reload after resolving it");
                    if(value!="0"&&value!="1"&&value!="true"&&value!="false")throw std::runtime_error("Crash-dump settings must use 0, 1, true or false");
                    option->Value=value=="1"||value=="true";option->Span={{size_t(value.data()-text.data()),value.size()}};
                }
            }
        }
        at=end==text.size()?end:end+1;
    }return result;
}
inline std::string Encode(std::string text,bool full){
    const auto parsed=Parse(text);const std::string newline=text.find("\r\n")!=text.npos?"\r\n":"\n";
    std::vector<std::pair<size_t,std::pair<size_t,std::string>>> edits;
    std::string missing;
    const auto set=[&](const char* name,const Option& option,bool value){
        if(option.Span)edits.push_back({option.Span->first,{option.Span->second,value?"1":"0"}});
        else missing+=std::string(name)+" = "+(value?"1":"0")+newline;
    };
    set("FullMemoryDump",parsed.Full,full);if(full)set("EnableDumping",parsed.Dumping,true);
    if(!missing.empty()){
        const auto at=parsed.Section?parsed.Insert:text.size();
        if(!parsed.Section)missing="[CrashDump]"+newline+missing;
        if(at&&text[at-1]!='\n')missing=newline+missing;
        edits.push_back({at,{0,std::move(missing)}});
    }
    std::sort(edits.begin(),edits.end(),[](const auto& a,const auto& b){return a.first>b.first;});
    for(const auto& [at,edit]:edits)text.replace(at,edit.first,edit.second);
    if(text.size()>MaxBytes)throw std::runtime_error("UE4SS settings exceed their size limit");
    const auto after=Parse(text);if(after.Full.Value!=full||(full&&!after.Dumping.Value))throw std::runtime_error("Crash-dump setting validation failed");
    return text;
}
inline Json ReadUnlocked(const std::filesystem::path& directory){
    const auto file=directory/"UE4SS-settings.ini";auto text=ConfigFiles::Read(file,MaxBytes);const auto options=Parse(text);
    return {{"Ok",true},{"FullMemoryDump",options.Full.Value},{"EnableDumping",options.Dumping.Value},
        {"ExpectedContents",std::move(text)},{"Path",file.string()},{"RestartRequired",true}};
}
inline Json Read(const std::filesystem::path& directory){
    std::lock_guard lock(Gate);try{return ReadUnlocked(directory);}catch(const std::exception& error){return {{"Ok",false},{"Status",error.what()}};}
}
inline Json Save(const std::filesystem::path& directory,const Json& request){
    std::lock_guard lock(Gate);try{
        if(!request.at("FullMemoryDump").is_boolean())throw std::runtime_error("FullMemoryDump must be a boolean");
        const auto before=ReadUnlocked(directory);
        if(!request.at("ExpectedContents").is_string()||request.at("ExpectedContents")!=before.at("ExpectedContents"))throw std::runtime_error("UE4SS settings changed. Reload before saving; nothing was written.");
        const auto full=request.at("FullMemoryDump").get<bool>();const auto text=Encode(before.at("ExpectedContents").get<std::string>(),full);
        const auto file=directory/"UE4SS-settings.ini";const auto backup=ConfigFiles::Backup(file);
        if(ConfigFiles::Read(file,MaxBytes)!=before.at("ExpectedContents").get<std::string>())throw std::runtime_error("UE4SS settings changed while backing up. Reload before saving.");
        ConfigFiles::Write(file,text);auto result=ReadUnlocked(directory);result["Backup"]=backup.string();
        result["Status"]="Saved for the next Dragonwilds launch. Crash dumps stay local; existing dump files are retained.";return result;
    }catch(const std::exception& error){return {{"Ok",false},{"Status",error.what()}};}
}
}
