#pragma once
#include "Utility/Config.h"
#include "glaze/glaze.hpp"
#include <stdexcept>
#include <string_view>
#include <nlohmann/json.hpp>

namespace PS {
    inline std::string StripJsonComments(std::string_view input) {
        std::string result; result.reserve(input.size());
        bool quoted=false,escaped=false,line=false,block=false;
        for(std::size_t i=0;i<input.size();++i) {
            const char c=input[i],next=i+1<input.size()?input[i+1]:'\0';
            if(line){if(c=='\n'){line=false;result.push_back(c);}continue;}
            if(block){if(c=='*'&&next=='/'){block=false;++i;}else if(c=='\n')result.push_back(c);continue;}
            if(!quoted&&c=='/'&&next=='/'){line=true;++i;continue;}
            if(!quoted&&c=='/'&&next=='*'){block=true;++i;continue;}
            result.push_back(c);
            if(quoted){if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;}
            else if(c=='"')quoted=true;
        }
        if(block)throw std::runtime_error("Unterminated block comment in settings.jsonc");
        return result;
    }
    inline PSConfigSettings DecodeSettings(const std::string& text) {
        PSConfigSettings candidate{};
        const auto json=StripJsonComments(text);
        const auto error = glz::read<glz::opts{.error_on_unknown_keys = false, .error_on_missing_keys = false}>(candidate, json);
        if (error) throw std::runtime_error(glz::format_error(error, json));
        const auto source=nlohmann::json::parse(json);
        auto& launch=candidate.quickLaunch;
        if(!source.contains(nlohmann::json::json_pointer("/quickLaunch/mode")))
            launch.mode=launch.enabled?"quickLaunch":"lessRestrictive";
        if(launch.mode!="vanilla"&&launch.mode!="lessRestrictive"&&launch.mode!="quickLaunch")
            throw std::runtime_error("Invalid quickLaunch.mode; use vanilla, lessRestrictive, or quickLaunch");
        launch.enabled=launch.mode=="quickLaunch";
        launch.softLoading=launch.mode!="vanilla";
        if(candidate.plugins.compatibilityNotices!="normal"&&candidate.plugins.compatibilityNotices!="quiet"&&candidate.plugins.compatibilityNotices!="off")
            throw std::runtime_error("Invalid plugins.compatibilityNotices; use normal, quiet, or off");
        return candidate;
    }
    inline std::string EncodeSettings(const PSConfigSettings& settings) {
        std::string text;
        const auto error = glz::write<glz::opts{.prettify = true}>(settings, text);
        if (error) throw std::runtime_error("Cannot serialize configuration");
        return std::string{
            "// RuneSchema settings. Edit with the game closed, then restart.\n"
            "// Advanced logging controls detail; advanced runtime unlocks authoring and diagnostics.\n"
            "// Plugin-owned settings live below plugins/<PluginId>/settings/.\n"}+text+"\n";
    }
}
