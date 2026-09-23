#pragma once

#include "nlohmann/json.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <format>
#include <map>
#include <ranges>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace DragonWilds::RegistryPatch {
    inline constexpr std::string_view Schema = "runeschema.registry-patch/v1";

    enum class TargetKind { DataTable, Object, ClassDefaultObject };
    enum class Operation {
        AddRow, UpsertOwnedRow, CopyRow, MergeOwnedRow, PatchExistingRow,
        Set, Merge, Append, AppendUnique, UpsertOwned
    };

    struct Target {
        TargetKind Kind = TargetKind::DataTable;
        std::string ObjectPath;
        std::string ShortName;
        std::vector<std::string> SearchRoots;
        std::string ExpectedClass;
        std::string ExpectedRowStruct;
    };

    struct Patch {
        std::string Owner;
        std::string Source;
        std::string Id;
        std::string CanonicalId;
        std::string Transaction;
        std::string Profile;
        int Priority = 0;
        Target TargetSpec;
        Operation Op = Operation::AddRow;
        std::vector<std::string> DependsOn;
        nlohmann::json Row;
        std::string Property;
        nlohmann::json Identity;
        nlohmann::json Template;
        nlohmann::json Value;
        nlohmann::json Preconditions;
        std::string Digest;
    };

    struct Document {
        std::string Owner;
        std::string Source;
        std::string Profile;
        int Priority = 0;
        std::vector<Patch> Patches;
    };

    inline bool IsId(std::string_view value) {
        if (value.empty() || value.size() > 128 || !std::isalnum(static_cast<unsigned char>(value.front()))) return false;
        return std::ranges::all_of(value, [](unsigned char c) { return std::isalnum(c) || c == '.' || c == '_' || c == '-'; });
    }

    inline std::string NormalizeId(std::string value) {
        std::ranges::transform(value, value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    inline bool IsObjectPath(std::string_view value) {
        return value.size() >= 4 && value.size() <= 1024 && value.front() == '/' && value.find('.') != std::string_view::npos
            && value.find("..") == std::string_view::npos && value.find('\\') == std::string_view::npos;
    }

    inline bool IsTypePath(std::string_view value) {
        return value.starts_with("/Script/") && IsObjectPath(value);
    }

    inline std::string StableDigest(std::string_view value) {
        std::uint64_t hash = 14695981039346656037ull;
        for (const auto c : value) { hash ^= static_cast<unsigned char>(c); hash *= 1099511628211ull; }
        return std::format("{:016X}", hash);
    }

    inline std::string StableRowName(std::string_view prefix, std::string_view canonical, std::string_view suffix = {}) {
        const auto digest = StableDigest(std::format("{}\n{}\n{}", Schema, canonical, suffix));
        return std::format("{}_{}", prefix, digest.substr(0, 12));
    }

    inline void RequireObject(const nlohmann::json& value, std::string_view where) {
        if (!value.is_object()) throw std::runtime_error(std::format("{} must be an object", where));
    }

    inline void RequireKeys(const nlohmann::json& value, std::initializer_list<std::string_view> allowed, std::string_view where) {
        const std::set<std::string> names(allowed.begin(), allowed.end());
        for (const auto& [key, unused] : value.items())
            if (!names.contains(key)) throw std::runtime_error(std::format("{} contains unknown field '{}'", where, key));
    }

    inline Target ParseTarget(const nlohmann::json& value) {
        RequireObject(value, "target");
        RequireKeys(value, {"kind","objectPath","shortName","searchRoots","expectedClass","expectedRowStruct"}, "target");
        if (!value.contains("kind") || !value.at("kind").is_string()) throw std::runtime_error("target.kind is required");
        Target out;
        const auto kind = value.at("kind").get<std::string>();
        if (kind == "dataTable") out.Kind = TargetKind::DataTable;
        else if (kind == "object") out.Kind = TargetKind::Object;
        else if (kind == "classDefaultObject") out.Kind = TargetKind::ClassDefaultObject;
        else throw std::runtime_error("target.kind is unsupported");
        if (value.contains("objectPath")) out.ObjectPath = value.at("objectPath").get<std::string>();
        if (value.contains("shortName")) out.ShortName = value.at("shortName").get<std::string>();
        if (out.ObjectPath.empty() == out.ShortName.empty()) throw std::runtime_error("target requires exactly one of objectPath or shortName");
        if (!out.ObjectPath.empty() && !IsObjectPath(out.ObjectPath)) throw std::runtime_error("target.objectPath is not a cooked object path");
        if (!out.ShortName.empty() && (!IsId(out.ShortName) || out.ShortName.size() > 256)) throw std::runtime_error("target.shortName is invalid");
        if (value.contains("searchRoots")) {
            if (!value.at("searchRoots").is_array() || value.at("searchRoots").size() > 32) throw std::runtime_error("target.searchRoots is invalid");
            for (const auto& root : value.at("searchRoots")) {
                auto path = root.get<std::string>();
                if (!path.starts_with('/') || path.find("..") != std::string::npos) throw std::runtime_error("target.searchRoots contains an invalid root");
                out.SearchRoots.push_back(std::move(path));
            }
        }
        if (value.contains("expectedClass")) out.ExpectedClass = value.at("expectedClass").get<std::string>();
        if (value.contains("expectedRowStruct")) out.ExpectedRowStruct = value.at("expectedRowStruct").get<std::string>();
        if (!out.ExpectedClass.empty() && !IsTypePath(out.ExpectedClass)) throw std::runtime_error("target.expectedClass is invalid");
        if (!out.ExpectedRowStruct.empty() && !IsTypePath(out.ExpectedRowStruct)) throw std::runtime_error("target.expectedRowStruct is invalid");
        return out;
    }

    inline Operation ParseOperation(std::string_view value) {
        static const std::map<std::string_view, Operation> operations{
            {"addRow",Operation::AddRow},{"upsertOwnedRow",Operation::UpsertOwnedRow},{"copyRow",Operation::CopyRow},
            {"mergeOwnedRow",Operation::MergeOwnedRow},{"patchExistingRow",Operation::PatchExistingRow},{"set",Operation::Set},
            {"merge",Operation::Merge},{"append",Operation::Append},{"appendUnique",Operation::AppendUnique},{"upsertOwned",Operation::UpsertOwned}};
        const auto found = operations.find(value);
        if (found == operations.end()) throw std::runtime_error(std::format("unsupported registry operation '{}'", value));
        return found->second;
    }

    inline Document ParseDocument(const nlohmann::json& value, std::string actualOwner, std::string source) {
        RequireObject(value, "registry patch document");
        RequireKeys(value, {"$schema","schema","modId","profile","priority","patches"}, "registry patch document");
        if (value.value("schema", "") != Schema) throw std::runtime_error("unsupported registry patch schema");
        const auto declaredOwner = value.value("modId", "");
        if (!IsId(declaredOwner) || !IsId(actualOwner) || NormalizeId(declaredOwner) != NormalizeId(actualOwner))
            throw std::runtime_error("registry patch modId does not match the containing mod");
        Document out{NormalizeId(actualOwner), std::move(source)};
        out.Profile = value.value("profile", "dragonwilds.dataTableOwnedRows.v1");
        out.Priority = value.value("priority", 0);
        if (out.Priority < -100000 || out.Priority > 100000) throw std::runtime_error("registry patch priority is out of range");
        if (!value.contains("patches") || !value.at("patches").is_array() || value.at("patches").empty() || value.at("patches").size() > 4096)
            throw std::runtime_error("registry patch patches must contain 1..4096 entries");
        std::set<std::string> ids;
        for (const auto& item : value.at("patches")) {
            RequireObject(item, "patch");
            RequireKeys(item, {"id","transaction","dependsOn","target","operation","property","row","identity","template","value","preconditions"}, "patch");
            Patch patch; patch.Owner = out.Owner; patch.Source = out.Source; patch.Profile = out.Profile; patch.Priority = out.Priority;
            patch.Id = item.value("id", "");
            if (!IsId(patch.Id) || !ids.insert(patch.Id).second) throw std::runtime_error("patch id is invalid or duplicated");
            patch.CanonicalId = patch.Owner + ":" + NormalizeId(patch.Id);
            patch.Transaction = item.value("transaction", patch.Id);
            if (!IsId(patch.Transaction)) throw std::runtime_error("patch transaction is invalid");
            if (!item.contains("target") || !item.contains("operation")) throw std::runtime_error("patch target and operation are required");
            patch.TargetSpec = ParseTarget(item.at("target"));
            patch.Op = ParseOperation(item.at("operation").get<std::string>());
            if (item.contains("dependsOn")) {
                if (!item.at("dependsOn").is_array() || item.at("dependsOn").size() > 128) throw std::runtime_error("patch dependsOn is invalid");
                for (const auto& dependency : item.at("dependsOn")) { auto id=dependency.get<std::string>(); if(!IsId(id)) throw std::runtime_error("patch dependency is invalid"); patch.DependsOn.push_back(std::move(id)); }
            }
            if (item.contains("row")) patch.Row = item.at("row");
            if (item.contains("property")) patch.Property = item.at("property").get<std::string>();
            if (item.contains("identity")) patch.Identity = item.at("identity");
            if (item.contains("template")) patch.Template = item.at("template");
            if (item.contains("value")) patch.Value = item.at("value");
            if (item.contains("preconditions")) patch.Preconditions = item.at("preconditions");
            const bool rowOp = patch.Op <= Operation::PatchExistingRow;
            if (rowOp && patch.Row.is_null()) throw std::runtime_error("DataTable operation requires row");
            if (!rowOp && (patch.Property.empty() || patch.Value.is_null())) throw std::runtime_error("object operation requires property and value");
            if (rowOp && patch.TargetSpec.Kind != TargetKind::DataTable) throw std::runtime_error("DataTable operation requires a dataTable target");
            if (!rowOp && patch.TargetSpec.Kind == TargetKind::DataTable) throw std::runtime_error("object operation cannot target a dataTable");
            patch.Digest = StableDigest(item.dump());
            out.Patches.push_back(std::move(patch));
        }
        return out;
    }

    inline std::vector<Patch> BuildPlan(const std::vector<Document>& documents) {
        std::vector<Patch> all;
        std::unordered_map<std::string,std::string> ownership;
        for (const auto& document : documents) for (const auto& patch : document.Patches) {
            const auto [found, inserted] = ownership.emplace(patch.CanonicalId, patch.Digest);
            if (!inserted) {
                if (found->second == patch.Digest) continue;
                throw std::runtime_error(std::format("conflicting registry patch identity '{}'", patch.CanonicalId));
            }
            all.push_back(patch);
        }
        std::ranges::sort(all, [](const Patch& a, const Patch& b) {
            return std::tie(a.Priority,a.Owner,a.Source,a.Id) < std::tie(b.Priority,b.Owner,b.Source,b.Id);
        });
        std::unordered_map<std::string,std::size_t> byId;
        for (std::size_t i=0;i<all.size();++i) byId.emplace(all[i].Owner+":"+all[i].Id,i);
        std::vector<int> state(all.size()); std::vector<Patch> sorted;
        const auto visit = [&](auto&& self, std::size_t index)->void {
            if (state[index] == 2) return;
            if (state[index] == 1) throw std::runtime_error("registry patch dependency cycle");
            state[index] = 1;
            for (const auto& dependency : all[index].DependsOn) {
                const auto found = byId.find(all[index].Owner+":"+dependency);
                if (found == byId.end()) throw std::runtime_error(std::format("missing registry patch dependency '{}:{}'", all[index].Owner, dependency));
                self(self, found->second);
            }
            state[index]=2; sorted.push_back(all[index]);
        };
        for (std::size_t i=0;i<all.size();++i) visit(visit,i);
        return sorted;
    }
}
