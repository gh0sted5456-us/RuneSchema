#pragma once
#include "Utility/ConfigCodec.h"
#include <sstream>
#include <unordered_set>

namespace PS {
    inline std::string IniTrim(std::string_view value) {
        const auto first=value.find_first_not_of(" \t\r\n");
        if(first==value.npos)return {};
        return std::string(value.substr(first,value.find_last_not_of(" \t\r\n")-first+1));
    }
    inline bool IniName(std::string_view value) {
        return !value.empty()&&value.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==value.npos;
    }
    inline nlohmann::json SettingsDocument(const PSConfigSettings& settings,nlohmann::json original=nlohmann::json::object()) {
        if(!original.is_object())throw std::runtime_error("Settings must be an object");
        original.merge_patch(nlohmann::json::parse(StripJsonComments(EncodeSettings(settings))));
        return original;
    }
    inline nlohmann::json DecodeIniDocument(std::string text) {
        if(text.starts_with("\xEF\xBB\xBF"))text.erase(0,3);
        nlohmann::json document=nlohmann::json::object();
        std::vector<std::string> section;
        std::unordered_set<std::string> assigned;
        std::istringstream input(text);std::string line;size_t number=0;
        while(std::getline(input,line)) {
            ++number;bool quoted=false,escaped=false;
            for(size_t i=0;i<line.size();++i){
                const auto c=line[i];
                if(!quoted&&(c==';'||c=='#')){line.resize(i);break;}
                if(quoted){if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;}
                else if(c=='"')quoted=true;
            }
            line=IniTrim(line);if(line.empty())continue;
            const auto invalid=[&]{return std::runtime_error("Invalid settings.ini line "+std::to_string(number));};
            if(line.front()=='['){
                if(line.back()!=']')throw invalid();
                auto name=IniTrim(std::string_view(line).substr(1,line.size()-2));
                section.clear();
                if(name=="RuneSchema")continue;
                std::istringstream parts(name);std::string part;
                if(name.empty()||name.back()=='.')throw invalid();
                while(std::getline(parts,part,'.')){if(!IniName(part))throw invalid();section.push_back(part);}
                auto* object=&document;
                for(const auto& component:section){
                    if(object->contains(component)&&!(*object)[component].is_object())throw invalid();
                    if(!object->contains(component))(*object)[component]=nlohmann::json::object();
                    object=&(*object)[component];
                }
                continue;
            }
            const auto equals=line.find('=');if(equals==line.npos)throw invalid();
            const auto key=IniTrim(std::string_view(line).substr(0,equals));
            const auto value=IniTrim(std::string_view(line).substr(equals+1));
            if(!IniName(key)||value.empty())throw invalid();
            auto* object=&document;std::string identity;
            for(const auto& part:section){
                identity+=part+'.';
                if(object->contains(part)&&!(*object)[part].is_object())throw invalid();
                if(!object->contains(part))(*object)[part]=nlohmann::json::object();
                object=&(*object)[part];
            }
            if(!assigned.insert(identity+key).second||object->contains(key))throw invalid();
            // JSON literals keep arrays, numeric types and escaped strings
            // lossless. Plain words are accepted for string-valued options.
            auto parsed=nlohmann::json::parse(value,nullptr,false);
            if(parsed.is_discarded()){
                if(value.find_first_of("\"[]{}")!=value.npos)throw invalid();
                parsed=value;
            }
            (*object)[key]=std::move(parsed);
        }
        if(assigned.empty())throw std::runtime_error("settings.ini has no settings");
        (void)DecodeSettings(document.dump());return document;
    }
    inline std::string EncodeIniDocument(const nlohmann::json& document) {
        (void)DecodeSettings(document.dump());
        std::string text="; RuneSchema settings. Edit with the game closed, then restart.\n"
            "; Booleans: true/false. Lists: JSON arrays. Quote strings containing ; or #.\n"
            "; Plugin-owned settings remain below plugins/<PluginId>/settings/.\n\n";
        const auto emit=[&](auto&& self,const nlohmann::json& object,const std::string& section)->void {
            text+='['+section+"]\n";
            for(const auto& [key,value]:object.items()){
                if(!IniName(key))throw std::runtime_error("Setting name cannot be represented in INI: "+key);
                if(!value.is_object())text+=key+" = "+value.dump()+"\n";
            }
            text+='\n';
            for(const auto& [key,value]:object.items())if(value.is_object()){
                if(section=="RuneSchema"&&key=="RuneSchema")throw std::runtime_error("Reserved INI section name");
                self(self,value,section=="RuneSchema"?key:section+'.'+key);
            }
        };
        emit(emit,document,"RuneSchema");
        // Refuse lossy conversion of unknown/forward-compatible settings.
        if(DecodeIniDocument(text)!=document)throw std::runtime_error("INI conversion would lose settings");
        return text;
    }
}
