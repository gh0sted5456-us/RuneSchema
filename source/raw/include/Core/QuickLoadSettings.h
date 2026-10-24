#pragma once
#include <string>
#include <string_view>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <nlohmann/json.hpp>
namespace PS::QuickLoadSettings {
struct Config {bool Enabled=false;std::string CharacterName,WorldName;bool SoftLoading=false;std::string Mode;};
inline Config Normalize(Config config){
    if(config.Mode.empty())config.Mode=config.Enabled?"quickLaunch":"lessRestrictive";
    if(config.Mode!="vanilla"&&config.Mode!="lessRestrictive"&&config.Mode!="quickLaunch")throw std::runtime_error("Unsupported launch mode");
    config.Enabled=config.Mode=="quickLaunch";config.SoftLoading=config.Mode!="vanilla";return config;
}
inline bool Name(std::string_view value){return !value.empty()&&value.size()<=256&&value.front()!=' '&&value.back()!=' '&&value!="NAMEHERE"&&value!="."&&value!=".."&&value.find_first_of("\r\n\t\0",0,4)==value.npos&&value.find_first_of("/\\:")==value.npos;}
inline std::string Trim(std::string value){const auto a=value.find_first_not_of(" \t\r");if(a==value.npos)return {};const auto b=value.find_last_not_of(" \t\r");return value.substr(a,b-a+1);}
inline bool StandaloneEntryEnabled(std::string_view text){
    if(text.size()>1024*1024)throw std::runtime_error("UE4SS mod list exceeds Quick Launch's bounded conflict check");
    if(text.starts_with("\xEF\xBB\xBF"))text.remove_prefix(3);
    size_t at=0;while(at<text.size()){
        const auto end=text.find('\n',at);auto line=std::string(text.substr(at,end==text.npos?text.size()-at:end-at));at=end==text.npos?text.size():end+1;
        const auto comment=line.find_first_of(";#");if(comment!=line.npos)line.resize(comment);
        std::erase_if(line,[](unsigned char c){return std::isspace(c)!=0;});
        std::transform(line.begin(),line.end(),line.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        if(line=="quickload:1")return true;
    }return false;
}
inline Config Parse(std::string_view text){
    if(text.size()>32768)throw std::runtime_error("QuickLoad.ini exceeds 32 KiB");
    Config config;bool section=false;size_t at=0;
    while(at<text.size()){const auto end=text.find('\n',at);auto line=Trim(std::string(text.substr(at,end==text.npos?text.size()-at:end-at)));at=end==text.npos?text.size():end+1;
        if(line.empty()||line[0]==';'||line[0]=='#')continue;
        if(line[0]=='['){section=line=="[QuickLoad]";continue;}if(!section)continue;
        const auto equal=line.find('=');if(equal==line.npos)continue;const auto key=Trim(line.substr(0,equal)),value=Trim(line.substr(equal+1));
        if(key=="Enabled"||key=="SoftLoading"){if(value!="true"&&value!="false"&&value!="1"&&value!="0")throw std::runtime_error("Invalid QuickLoad boolean value");if(key=="Enabled")config.Enabled=value=="true"||value=="1";else config.SoftLoading=value=="true"||value=="1";}
        else if(key=="Mode")config.Mode=value;else if(key=="CharacterName")config.CharacterName=value;else if(key=="WorldName")config.WorldName=value;
    }return Normalize(config);
}
inline std::string Encode(const Config& value){
    const auto config=Normalize(value);
    if((!config.CharacterName.empty()&&!Name(config.CharacterName))||(!config.WorldName.empty()&&!Name(config.WorldName))||(config.Enabled&&(!Name(config.CharacterName)||!Name(config.WorldName))))throw std::runtime_error("Select a valid character and world before enabling Quick Load");
    return "[QuickLoad]\nMode="+config.Mode+"\nEnabled="+std::string(config.Enabled?"true":"false")+"\nWorldName="+config.WorldName+"\nCharacterName="+config.CharacterName+"\nSoftLoading="+(config.SoftLoading?"true":"false")+"\n";
}
inline nlohmann::json Document(const Config& value){const auto c=Normalize(value);return {{"Mode",c.Mode},{"CharacterName",c.CharacterName},{"WorldName",c.WorldName}};}
}
