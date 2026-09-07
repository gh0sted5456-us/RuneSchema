#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <stdexcept>
#include <cctype>
namespace PS::InspectionTools {
inline constexpr unsigned DefaultCaptureDepth = 7;
inline constexpr unsigned StandardCaptureDepthLimit = 10;
inline constexpr unsigned ExtendedCaptureDepthLimit = 16;
inline void ValidatePreset(const nlohmann::json& value, bool extendedDepth = false) {
    if (!value.is_object()) throw std::runtime_error("Preset must be a JSON object.");
    for (const auto& [key, ignored] : value.items())
        if (key != "Name" && key != "Objects" && key != "ControllerProperties" && key != "IncludePlayer" && key != "IncludeControllerComponents" && key != "PropertyCaptures" && key != "CaptureLimits")
            throw std::runtime_error("Unknown preset field: " + key);
    if (!value.contains("Name") || !value["Name"].is_string()) throw std::runtime_error("Preset requires Name.");
    const auto name = value["Name"].get<std::string>();
    if (name.empty() || name.size() > 64) throw std::runtime_error("Name must contain 1-64 letters, digits, underscores or hyphens.");
    for (unsigned char c : name) if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
        throw std::runtime_error("Name must contain only letters, digits, underscores or hyphens.");
    bool any = false;
    if (value.contains("CaptureLimits")) {
        if (!value.contains("PropertyCaptures") || !value["CaptureLimits"].is_object())
            throw std::runtime_error("CaptureLimits requires PropertyCaptures and an object value");
        for (const auto& [key, limit] : value["CaptureLimits"].items()) {
            if (key == "FollowObjectReferences") {
                if (!limit.is_boolean()) throw std::runtime_error("FollowObjectReferences must be boolean");
                continue;
            }
            unsigned maximum = 0;
            if (key == "MaxDepth") maximum = extendedDepth ? ExtendedCaptureDepthLimit : StandardCaptureDepthLimit;
            else if (key == "MaxEntries") maximum = 512;
            else if (key == "MaxNodes" || key == "MaxSparseSlots") maximum = 16384;
            else throw std::runtime_error("Unknown capture limit: " + key);
            if (!limit.is_number_integer() || limit < 1 || limit > maximum)
                throw std::runtime_error("Capture limit out of range: " + key + " (allowed 1-" + std::to_string(maximum) + ")");
        }
    }
    if (value.contains("PropertyCaptures")) {
        const auto& captures = value["PropertyCaptures"];
        if (!captures.is_array() || captures.size() > 32) throw std::runtime_error("PropertyCaptures must contain at most 32 targets");
        for (const auto& capture : captures) {
            if (!capture.is_object() || capture.size() != 2 || !capture.contains("Root") || !capture.contains("Path")
                || !capture["Root"].is_string() || !capture["Path"].is_array() || capture["Path"].empty() || capture["Path"].size() > 8)
                throw std::runtime_error("Property capture requires Root and a Path of 1-8 property names");
            const auto root = capture["Root"].get<std::string>();
            if (root.empty() || root.size() > 2048 || (root != "Player" && root != "Controller" && root != "Selected" && !root.starts_with("/")))
                throw std::runtime_error("Root must be Player, Controller, Selected or a full object path");
            for (unsigned char c : root) if (c < 32) throw std::runtime_error("Control character in Root");
            for (const auto& part : capture["Path"]) {
                if (!part.is_string()) throw std::runtime_error("Path entries must be property names");
                const auto name = part.get<std::string>();
                if (name == "*" && &part == &capture["Path"].back()) continue;
                if (name.empty() || name.size() > 128) throw std::runtime_error("Property name length must be 1-128");
                for (unsigned char c : name)
                    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'))
                        throw std::runtime_error("Property names may contain only letters, digits and underscores");
            }
            any = true;
        }
    }
    for (const auto* key : {"Objects", "ControllerProperties"}) {
        if (!value.contains(key)) continue;
        if (!value[key].is_array() || value[key].size() > 64) throw std::runtime_error(std::string(key) + " must be an array of at most 64 strings.");
        for (const auto& entry : value[key]) {
            if (!entry.is_string()) throw std::runtime_error("Preset targets must be strings.");
            const auto text = entry.get<std::string>();
            if (text.empty() || text.size() > 2048) throw std::runtime_error("Preset target is empty or too long.");
            for (unsigned char c : text) if (c < 32) throw std::runtime_error("Control characters are not allowed in targets.");
            if (std::string(key) == "Objects" && text.front() != '/') throw std::runtime_error("Objects require full Unreal object paths beginning with /.");
            any = true;
        }
    }
    for (const auto* key : {"IncludePlayer", "IncludeControllerComponents"}) {
        if (value.contains(key) && !value[key].is_boolean()) throw std::runtime_error(std::string(key) + " must be a boolean.");
        any = any || value.value(key, false);
    }
    if (!any) throw std::runtime_error("Preset must select at least one capture target.");
}
}
