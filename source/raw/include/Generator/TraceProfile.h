#pragma once
#include "Generator/PlayerTraceOptions.h"
#include "Generator/DiagnosticExport.h"

namespace PS::InspectionTools {
    inline std::string TraceTargetAfterProfileLoad(const nlohmann::json& profile,
            const std::string& current, const std::string& resolved) {
        return profile.contains("Target") ? resolved : current;
    }
    inline nlohmann::json ParseTraceProfile(const std::string& text) {
        if(text.size()>32768)throw std::runtime_error("Trace profile exceeds 32 KiB.");
        size_t nodes=0;
        return nlohmann::json::parse(text,[&](int depth,nlohmann::json::parse_event_t,nlohmann::json&){
            if(depth>16 || ++nodes>2048)throw std::runtime_error("Trace profile structure exceeds limits.");return true;
        },true,true);
    }
    inline nlohmann::json ValidateTraceProfile(const nlohmann::json& profile) {
        using nlohmann::json;
        if (!profile.is_object()) throw std::runtime_error("Trace profile must be an object.");
        for (const auto& [key,value] : profile.items())
            if (key!="Version" && key!="Name" && key!="Description" && key!="Trace" && key!="Target" && key!="Export")
                throw std::runtime_error("Unknown trace-profile field: "+key);
        if (!profile.contains("Version") || !profile["Version"].is_number_integer() || profile["Version"]!=1
            || !profile.contains("Name") || !profile["Name"].is_string() || profile["Name"].get_ref<const std::string&>().empty()
            || !profile.contains("Trace")) throw std::runtime_error("Profile requires Version 1, Name and Trace.");
        DiagnosticExportName(profile["Name"].get<std::string>(), "", "", false);
        if (profile.contains("Description") && (!profile["Description"].is_string() || profile["Description"].get_ref<const std::string&>().size()>1024))
            throw std::runtime_error("Profile description limit: 1024 characters.");
        if (profile.contains("Target")) {
            const auto& target=profile["Target"];
            if(!target.is_object())throw std::runtime_error("Profile Target must be an object.");
            for(const auto& [key,value]:target.items())
                if(key!="Mode" && key!="Path" && key!="ClassContains" && key!="NameContains")
                    throw std::runtime_error("Unknown profile target field: "+key);
            const auto mode=target.value("Mode",std::string("ExactPath"));
            if(mode!="ExactPath" && mode!="ClassAndName")throw std::runtime_error("Profile Target Mode must be ExactPath or ClassAndName.");
            if(mode=="ExactPath" && (!target.contains("Path") || !target["Path"].is_string() || target["Path"].get_ref<const std::string&>().empty()))
                throw std::runtime_error("ExactPath targets require Path.");
            if(mode=="ClassAndName" && (!target.contains("ClassContains") || !target["ClassContains"].is_string()
                || !target.contains("NameContains") || !target["NameContains"].is_string()))
                throw std::runtime_error("ClassAndName targets require ClassContains and NameContains.");
        }
        PlayerTrace::ValidateOptions(profile["Trace"]);
        const auto checkStrings=[&](auto&& self,const json& value)->void {
            if(value.is_string() && value.get_ref<const std::string&>().find('\0')!=std::string::npos)
                throw std::runtime_error("Trace profile strings cannot contain NUL characters.");
            if(value.is_object() || value.is_array())for(const auto& child:value)self(self,child);
        };
        checkStrings(checkStrings,profile);
        json result=profile;
        auto& trace=result["Trace"];
        const json defaults={{"Seconds",30},{"MaxEvents",1024},{"Categories",15},{"Filter",""},
                {"SuppressTicks",true},{"TraceSelectedObject",false},{"CaptureParameters",false}};
        for (const auto& [key,value] : defaults.items())
            if (!trace.contains(key)) trace[key]=value;
        if (trace.contains("EventCaptures") && trace["EventCaptures"].dump().size()>=2048)
            throw std::runtime_error("Event field paths exceed the editor capacity.");
        if (!result.contains("Export")) result["Export"]={{"Name",""},{"Jsonc",false}};
        const auto& output=result["Export"];
        if (!output.is_object()) throw std::runtime_error("Export must be an object.");
        for(const auto& [key,value]:output.items()) if(key!="Name" && key!="Jsonc") throw std::runtime_error("Unknown export preference.");
        if ((output.contains("Name") && !output["Name"].is_string()) || (output.contains("Jsonc") && !output["Jsonc"].is_boolean()))
            throw std::runtime_error("Invalid export preferences.");
        DiagnosticExportName(output.value("Name",std::string{}),"Trace","",false);
        return result;
    }
}
