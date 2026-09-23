#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>
#include "Generator/TraceNameFilters.h"
namespace PS::PlayerTrace {
inline void ValidateOptions(const nlohmann::json& options) {
    if(!options.is_object())throw std::runtime_error("Player Trace options must be an object.");
    for(const auto& [key,value]:options.items()) {
        if(key=="Seconds" || key=="MaxEvents" || key=="Categories") {
            const auto maximum=key=="Seconds"?60:key=="MaxEvents"?4096:31;
            if(!value.is_number_integer() || value<1 || value>maximum)throw std::runtime_error("Player Trace option out of range: "+key);
        } else if(key=="Filter") {
            if(!value.is_string() || value.get_ref<const std::string&>().size()>128)throw std::runtime_error("Player Trace filter must be 0-128 characters.");
        } else if(key=="IncludeAny" || key=="ExcludeAny") {
            ValidateFilterTerms(value);
        } else if(key=="EventCaptures") {
            if (!value.is_array() || value.empty() || value.size()>8)
                throw std::runtime_error("EventCaptures requires 1-8 field paths.");
            for (const auto& capture : value) {
                if (!capture.is_object() || capture.size()!=2 || !capture.contains("Root") || !capture.contains("Path")
                    || !capture["Root"].is_string() || !capture["Path"].is_array())
                    throw std::runtime_error("Each event capture requires Root and Path.");
                const auto root=capture["Root"].get<std::string>();
                if (root!="Context" && root!="Player" && root!="Controller" && root!="Target")
                    throw std::runtime_error("Event capture root must be Context, Player, Controller or Target.");
                const auto& path=capture["Path"];
                if (path.empty() || path.size()>4) throw std::runtime_error("Event path requires 1-4 segments.");
                for (size_t i=0;i<path.size();++i)
                    if (!path[i].is_string() || path[i].get_ref<const std::string&>().empty()
                        || path[i].get_ref<const std::string&>().size()>128
                        || (path[i]=="*" && i+1!=path.size()))
                        throw std::runtime_error("Invalid event field path segment.");
            }
        } else if(key=="SuppressTicks" || key=="TraceSelectedObject" || key=="CaptureParameters") {
            if(!value.is_boolean())throw std::runtime_error("SuppressTicks must be boolean.");
        } else throw std::runtime_error("Unknown Player Trace option: "+key);
    }
    bool specific=options.value("Filter",std::string{}).size()>=8;
    if(options.contains("IncludeAny") && !options.at("IncludeAny").empty()) {
        bool allSpecific=true;for(const auto& term:options.at("IncludeAny"))allSpecific&=term.get_ref<const std::string&>().size()>=8;
        specific|=allSpecific;
    }
    if ((options.contains("EventCaptures") || options.value("CaptureParameters",false)) && !specific)
        throw std::runtime_error("Event capture requires an 8+ character function filter, or include terms each at least 8 characters.");
}
}
