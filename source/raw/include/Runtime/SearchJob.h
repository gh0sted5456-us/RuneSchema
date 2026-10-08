#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace PS::RuntimeJobs {
    inline void Validate(const nlohmann::json& job) {
        if (!job.is_object()) throw std::runtime_error("Job must be an object");
        for (const auto& [key, value] : job.items())
            if (key != "Name" && key != "Enabled" && key != "Trigger" && key != "Query"
                && key != "ClassContains" && key != "MaxResults" && key != "DelaySeconds")
                throw std::runtime_error("Unknown job field: " + key);
        for (const auto* key : {"Name", "Trigger", "Query"})
            if (!job.contains(key) || !job.at(key).is_string()) throw std::runtime_error(std::string("Missing string: ") + key);
        const auto name = job.at("Name").get<std::string>();
        if (name.empty() || name.size() > 64) throw std::runtime_error("Name must be 1-64 characters");
        for (unsigned char c : name)
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_'))
                throw std::runtime_error("Name allows only letters, digits, hyphens and underscores");
        const auto trigger = job.at("Trigger").get<std::string>();
        if (trigger != "game-load" && trigger != "world-load" && trigger != "game-state-ready")
            throw std::runtime_error("Trigger must be game-load, world-load or game-state-ready");
        for (const auto* key : {"Query", "ClassContains"}) {
            if (!job.contains(key)) continue;
            if (!job.at(key).is_string()) throw std::runtime_error(std::string(key) + " must be a string");
            const auto value = job.at(key).get<std::string>();
            if (value.size() > 512 || (std::string(key) == "Query" && value.empty())) throw std::runtime_error("Query must be nonempty and filters at most 512 characters");
            for (unsigned char c : value) if (c < 32) throw std::runtime_error("Control character in search filter");
        }
        if (job.contains("Enabled") && !job.at("Enabled").is_boolean()) throw std::runtime_error("Enabled must be boolean");
        for (const auto* key : {"MaxResults", "DelaySeconds"}) {
            if (!job.contains(key)) continue;
            if (!job.at(key).is_number_integer() || job.at(key) < (std::string(key) == "MaxResults" ? 1 : 0)
                || job.at(key) > (std::string(key) == "MaxResults" ? 250 : 30))
                throw std::runtime_error(std::string(key) + " is outside its allowed range");
        }
    }
}
