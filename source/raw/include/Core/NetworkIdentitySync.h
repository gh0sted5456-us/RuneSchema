#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>
#include <tuple>
#include <numeric>
#include <stdexcept>
#include "Core/NetworkIdentityPlan.h"
#include "nlohmann/json.hpp"

namespace PS::NetworkIdentitySync {
    inline constexpr std::size_t MaxDocument = 2 * 1024 * 1024;
    inline constexpr std::size_t MaxWelcome = 48 * 1024;
    inline constexpr std::string_view WelcomePrefix = "runeschema-ids-v1:";
    using Permutations = std::map<std::string, std::vector<std::size_t>>;
    struct Incompatible : std::runtime_error { using std::runtime_error::runtime_error; };
    inline void Compatible(bool value, const char* detail) { if (!value) throw Incompatible(detail); }

    inline void Require(bool value, const char* detail) {
        if (!value) throw std::runtime_error(detail);
    }
    template<class Apply, class Verify, class Rollback>
    void Commit(Apply applyChanges, Verify verifyChanges, Rollback rollbackChanges) {
        try { applyChanges(); Require(verifyChanges(), "Registry ID verification failed; original IDs restored"); }
        catch (...) { rollbackChanges(); throw; }
    }
    inline void Token(const std::string& value, std::size_t limit) {
        Require(!value.empty() && value.size() <= limit
            && value.find_first_of("\r\n\t") == std::string::npos
            && value.find('\0') == std::string::npos, "Invalid registry identity token");
    }
    inline void Validate(const nlohmann::json& document) {
        Require(document.is_object() && document.value("kind", std::string{}) == "RuneSchemaNetworkIdentityPlan"
            && document.value("version", 0) == 1, "Unsupported registry ID plan");
        Require(document.contains("lanes") && document.at("lanes").is_object()
            && document.at("lanes").size() <= 256, "Invalid registry lane table");
        Require(document.contains("fixedLanes") && document.at("fixedLanes").is_object()
            && document.at("fixedLanes").size() <= 256, "Invalid fixed registry lanes");
        Require(document.contains("registryOwners") && document.at("registryOwners").is_object()
            && document.at("registryOwners").size() <= 256, "Invalid registry owner table");
        for (const auto* field : {"fixedLanes", "registryOwners"})
            for (const auto& [key, value] : document.at(field).items()) {
                Token(key, 512); Require(value.is_string(), "Invalid registry fingerprint"); Token(value.get<std::string>(), 96);
            }
        if(document.contains("sourceVersions")) {
            Require(document.at("sourceVersions").is_object()&&document.at("sourceVersions").size()<=512,"Invalid source version telemetry");
            for(const auto& [key,value]:document.at("sourceVersions").items()){Token(key,128);Require(value.is_string(),"Invalid source version");Token(value.get<std::string>(),128);}
        }
        for (const auto& [key, lane] : document.at("lanes").items()) {
            Token(key, 512);
            Require(lane.is_object() && lane.contains("prefixCount") && lane.at("prefixCount").is_number_unsigned()
                && lane.contains("prefixFingerprint") && lane.at("prefixFingerprint").is_string()
                && lane.contains("entries") && lane.at("entries").is_array(), "Incomplete registry lane");
            const auto prefix = lane.at("prefixCount").get<std::size_t>();
            Require(prefix <= NetworkIdentityPlan::Capacity
                && lane.at("entries").size() <= NetworkIdentityPlan::Capacity - prefix, "Registry ID capacity exceeded");
            Token(lane.at("prefixFingerprint").get<std::string>(), 96);
            std::set<std::string> paths, identities;
            for (const auto& entry : lane.at("entries")) {
                Require(entry.is_object() && entry.contains("path") && entry.at("path").is_string()
                    && entry.contains("identity") && entry.at("identity").is_string()
                    && entry.contains("type") && entry.at("type").is_string(), "Incomplete registry entry");
                const auto path = entry.at("path").get<std::string>(), identity = entry.at("identity").get<std::string>();
                const auto type = entry.at("type").get<std::string>();
                Token(path, 2048); Token(identity, 1024); Token(type, 2048);
                Require(path.front() == '/' && path.find("..") == std::string::npos
                    && type.front() == '/' && type.find("..") == std::string::npos
                    && paths.insert(path).second && identities.insert(identity).second, "Duplicate or invalid registry entry");
            }
        }
    }
    inline nlohmann::json Parse(const std::string& bytes) {
        Require(!bytes.empty() && bytes.size() <= MaxDocument, "Registry ID document exceeds its limit");
        auto value = nlohmann::json::parse(bytes, [](int depth, auto, auto&) {
            Require(depth <= 16, "Registry ID document nesting exceeds its limit"); return true;
        }); Validate(value); return value;
    }
    // All lanes are planned before the caller touches either native lookup table.
    inline Permutations Plan(const nlohmann::json& local, const nlohmann::json& authority) {
        Validate(local); Validate(authority);
        for (const auto& [key, value] : authority.at("fixedLanes").items())
            Compatible(local.at("fixedLanes").contains(key) && local.at("fixedLanes").at(key) == value, "An unsupported authority registry lane differs; IDs were not changed");
        Permutations result;
        for (const auto& [key, lane] : local.at("lanes").items()) {
            auto& rows = result[key]; rows.resize(lane.at("entries").size()); std::iota(rows.begin(), rows.end(), std::size_t{0});
        }
        for (const auto& [key, server] : authority.at("lanes").items()) {
            Compatible(local.at("lanes").contains(key), "Authority registry lane is unavailable locally");
            const auto& client = local.at("lanes").at(key);
            Compatible(client.at("prefixCount") == server.at("prefixCount")
                && client.at("prefixFingerprint") == server.at("prefixFingerprint"), "Protected native registry baseline differs");
            Compatible(client.at("entries").size() >= server.at("entries").size(), "Some authority assets are unavailable locally");
            std::map<std::string, std::tuple<std::string, std::string, std::size_t>> lookup;
            for (std::size_t i = 0; i < client.at("entries").size(); ++i) {
                const auto& row = client.at("entries").at(i);
                lookup.emplace(row.at("path").get<std::string>(), std::tuple{row.at("identity").get<std::string>(), row.at("type").get<std::string>(), i});
            }
            auto& order = result[key];
            order.clear(); std::set<std::size_t> used;
            for (const auto& row : server.at("entries")) {
                const auto found = lookup.find(row.at("path").get<std::string>());
                Compatible(found != lookup.end() && std::get<0>(found->second) == row.at("identity").get<std::string>()
                    && std::get<1>(found->second) == row.at("type").get<std::string>(), "Authority asset identity or native class differs locally");
                order.push_back(std::get<2>(found->second));
                used.insert(std::get<2>(found->second));
            }
            for (std::size_t i = 0; i < client.at("entries").size(); ++i) if (!used.contains(i)) order.push_back(i);
        }
        return result;
    }
}
