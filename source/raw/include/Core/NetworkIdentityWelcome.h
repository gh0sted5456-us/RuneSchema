#pragma once
#include <Windows.h>
#include <compressapi.h>
#include <array>
#include <cstring>
#include <memory>
#include <optional>
#include "Core/NetworkIdentitySync.h"
#include "Core/CacheBundleIO.h"
#include "Core/HelpyCacheSources.h"

namespace PS::NetworkIdentityWelcome {
    using namespace NetworkIdentitySync;
    inline constexpr char Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    inline std::string Base64(const std::vector<unsigned char>& input) {
        std::string output;
        for (std::size_t i = 0; i < input.size(); i += 3) {
            const unsigned value = (unsigned(input[i]) << 16)
                | (i + 1 < input.size() ? unsigned(input[i + 1]) << 8 : 0)
                | (i + 2 < input.size() ? unsigned(input[i + 2]) : 0);
            output += Alphabet[(value >> 18) & 63]; output += Alphabet[(value >> 12) & 63];
            output += i + 1 < input.size() ? Alphabet[(value >> 6) & 63] : '=';
            output += i + 2 < input.size() ? Alphabet[value & 63] : '=';
        }
        return output;
    }
    inline std::vector<unsigned char> Unbase64(std::string_view text) {
        Require(!text.empty() && text.size() % 4 == 0 && text.size() <= MaxWelcome, "Invalid welcome encoding length");
        std::array<int, 256> values; values.fill(-1);
        for (int i = 0; i < 64; ++i) values[static_cast<unsigned char>(Alphabet[i])] = i;
        std::vector<unsigned char> result;
        for (std::size_t i = 0; i < text.size(); i += 4) {
            const bool end = i + 4 == text.size();
            const bool two = text[i + 2] == '=', one = text[i + 3] == '=';
            Require((!two || one) && ((!two && !one) || end), "Invalid welcome padding");
            const int a = values[static_cast<unsigned char>(text[i])], b = values[static_cast<unsigned char>(text[i + 1])],
                c = two ? 0 : values[static_cast<unsigned char>(text[i + 2])], d = one ? 0 : values[static_cast<unsigned char>(text[i + 3])];
            Require(a >= 0 && b >= 0 && c >= 0 && d >= 0 && (!two || (b & 15) == 0) && (!one || two || (c & 3) == 0), "Invalid welcome encoding");
            const unsigned value = (unsigned(a) << 18) | (unsigned(b) << 12) | (unsigned(c) << 6) | unsigned(d);
            result.push_back(static_cast<unsigned char>(value >> 16));
            if (!two) result.push_back(static_cast<unsigned char>(value >> 8));
            if (!one) result.push_back(static_cast<unsigned char>(value));
        }
        return result;
    }
    struct Compressor {
        COMPRESSOR_HANDLE Value = nullptr;
        ~Compressor() { if (Value) CloseCompressor(Value); }
    };
    struct Decompressor {
        DECOMPRESSOR_HANDLE Value = nullptr;
        ~Decompressor() { if (Value) CloseDecompressor(Value); }
    };
    inline nlohmann::json CanonicalEntries(const nlohmann::json& rows) {
        auto result = rows;
        std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
            if (a.at("identity") != b.at("identity")) return a.at("identity") < b.at("identity");
            return a.at("path") < b.at("path");
        });
        return result;
    }
    inline std::string OrderProof(const nlohmann::json& document) {
        return CacheBundleIO::Hash(nlohmann::json{{"kind",document.at("kind")},{"version",document.at("version")},
            {"lanes",document.at("lanes")},{"fixedLanes",document.at("fixedLanes")}}.dump());
    }
    inline std::optional<std::string> EncodeWire(const nlohmann::json& wire) {
        const auto raw = wire.dump();
        if(raw.size()>MaxDocument)return std::nullopt;
        Compressor compressor;
        Require(CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &compressor.Value), "Welcome compressor unavailable");
        SIZE_T needed = 0; Compress(compressor.Value, raw.data(), raw.size(), nullptr, 0, &needed);
        Require(needed > 0 && needed <= MaxDocument + 65536, "Invalid compressed welcome size");
        std::vector<unsigned char> buffer(4 + needed);
        const auto size = static_cast<std::uint32_t>(raw.size()); std::memcpy(buffer.data(), &size, 4);
        Require(Compress(compressor.Value, raw.data(), raw.size(), buffer.data() + 4, needed, &needed), "Registry welcome compression failed");
        buffer.resize(4 + needed); auto token = std::string(WelcomePrefix) + Base64(buffer);
        if(token.size()>MaxWelcome)return std::nullopt;
        return token;
    }
    inline std::string Encode(const nlohmann::json& document) {
        Validate(document);auto wire=document;wire["indexEncoding"]="mixed-v2";
        for(auto& [key,lane]:wire["lanes"].items()){
            const auto canonical=CanonicalEntries(lane["entries"]);lane["identityProof"]=CacheBundleIO::Hash(canonical.dump());
            std::set<std::string> sources;std::map<std::string,std::size_t> indices;
            for(std::size_t i=0;i<canonical.size();++i){
                const auto path=canonical.at(i).at("path").get<std::string>();const auto source=HelpyCacheSources::Source(path);
                Compatible(!source.empty(),"An authority asset source cannot be identified; native handling retained");sources.insert(source);indices.emplace(path,i);
            }
            lane["sources"]=sources;std::int64_t previous=0;
            for(auto& row:lane["entries"]){const auto index=static_cast<std::int64_t>(indices.at(row.at("path").get<std::string>()));row=index-previous;previous=index;}
        }
        auto token=EncodeWire(wire);
        if(!token&&wire.contains("sourceVersions")){wire.erase("sourceVersions");wire["telemetryOmitted"]=true;token=EncodeWire(wire);}
        if(!token&&!wire.at("registryOwners").empty()){wire["registryOwners"]=nlohmann::json::object();wire["telemetryOmitted"]=true;token=EncodeWire(wire);}
        Require(token.has_value(),"Registry table exceeds native welcome budget");
        // Upgrade individual lanes without losing the compact representation needed by large mod sets.
        for(auto& [key,lane]:wire["lanes"].items()){
            auto compact=lane;lane["entries"]=document.at("lanes").at(key).at("entries");
            for(auto& row:lane["entries"])row=row.at("path").get<std::string>();
            lane.erase("sources");lane["entryEncoding"]="asset-path-v2";
            if(auto exact=EncodeWire(wire))token=std::move(exact);else lane=std::move(compact);
        }
        return std::move(*token);
    }
    inline nlohmann::json Decode(std::string_view token, const nlohmann::json& local) {
        Require(token.starts_with(WelcomePrefix) && token.size() <= MaxWelcome, "Unsupported registry welcome");
        const auto data = Unbase64(token.substr(WelcomePrefix.size()));
        Require(data.size() > 4, "Truncated registry welcome"); std::uint32_t size = 0; std::memcpy(&size, data.data(), 4);
        Require(size > 0 && size <= MaxDocument, "Expanded registry welcome exceeds its limit");
        Decompressor decompressor;
        Require(CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &decompressor.Value), "Welcome decompressor unavailable");
        std::string output(size, '\0'); SIZE_T written = 0;
        Require(Decompress(decompressor.Value, data.data() + 4, data.size() - 4, output.data(), output.size(), &written)
            && written == size, "Invalid compressed registry welcome");
        Validate(local);
        auto document = ParseJson(output);
        Require(document.is_object() && document.contains("lanes") && document.at("lanes").is_object()
            && document.at("lanes").size() <= 256, "Invalid welcome lane table");
        const auto encoding=document.value("indexEncoding",std::string{});
        Require(encoding=="identity-delta-v1"||encoding=="mixed-v2", "Unsupported welcome index encoding");
        document.erase("indexEncoding");
        for (auto& [key, lane] : document["lanes"].items()) {
            Compatible(local.at("lanes").contains(key), "Authority registry lane is unavailable locally");
            Require(lane.is_object(),"Invalid welcome registry lane");
            const auto entryEncoding=encoding=="mixed-v2"?lane.value("entryEncoding",std::string("identity-delta-v1")):encoding;
            lane.erase("entryEncoding");
            Require(entryEncoding=="asset-path-v2"||entryEncoding=="identity-delta-v1","Unsupported welcome lane encoding");
            if(entryEncoding=="asset-path-v2"){
                Require(lane.is_object()&&lane.contains("entries")&&lane.at("entries").is_array()&&lane.at("entries").size()<=NetworkIdentityPlan::Capacity
                    &&lane.contains("identityProof")&&lane.at("identityProof").is_string(),"Invalid authority path roster");
                std::map<std::string,nlohmann::json> lookup;
                for(const auto& row:local.at("lanes").at(key).at("entries"))
                    lookup.emplace(row.at("path").get<std::string>(),row);
                auto rows=nlohmann::json::array();std::set<std::string> used;
                for(const auto& entry:lane.at("entries")){
                    Require(entry.is_string(),"Invalid authority asset path");const auto path=entry.get<std::string>();Token(path,2048);
                    Require(used.insert(path).second,"Duplicate authority asset path");
                    Compatible(lookup.contains(path),"An authority asset is unavailable locally; shared IDs were not changed");rows.push_back(lookup.at(path));
                }
                Compatible(lane.at("identityProof").get<std::string>()==CacheBundleIO::Hash(CanonicalEntries(rows).dump()),"Authority asset identities or native types differ locally; shared IDs were not changed");
                lane["entries"]=std::move(rows);lane.erase("identityProof");continue;
            }
            Require(lane.is_object()
                && lane.contains("entries") && lane.at("entries").is_array()
                && lane.at("entries").size() <= NetworkIdentityPlan::Capacity
                && lane.contains("identityProof") && lane.at("identityProof").is_string(), "Incompatible welcome registry lane");
            Require(lane.contains("sources") && lane.at("sources").is_array() && lane.at("sources").size() <= MaxSources, "Invalid authority source roster");
            std::set<std::string> sources;
            for (const auto& source : lane.at("sources")) { Require(source.is_string(), "Invalid authority source"); const auto value=source.get<std::string>(); Token(value,192); Require(sources.insert(value).second, "Duplicate authority source"); }
            auto candidates = nlohmann::json::array();
            for (const auto& row : local.at("lanes").at(key).at("entries"))
                if (sources.contains(HelpyCacheSources::Source(row.at("path").get<std::string>()))) candidates.push_back(row);
            const auto canonical = CanonicalEntries(candidates);
            Compatible(canonical.size() == lane.at("entries").size() && lane.at("identityProof").get<std::string>() == CacheBundleIO::Hash(canonical.dump()), "Authority content differs locally; shared IDs were not changed");
            std::int64_t previous = 0;
            for (auto& row : lane["entries"]) {
                Require(row.is_number_integer(), "Invalid welcome asset delta");
                if (row.is_number_unsigned()) Require(row.get<std::uint64_t>() <= NetworkIdentityPlan::Capacity, "Welcome delta exceeds registry capacity");
                const auto delta = row.get<std::int64_t>();
                Require(delta >= -static_cast<std::int64_t>(NetworkIdentityPlan::Capacity) && delta <= static_cast<std::int64_t>(NetworkIdentityPlan::Capacity), "Welcome delta exceeds registry capacity");
                previous += delta;
                Require(previous >= 0 && static_cast<std::size_t>(previous) < canonical.size(), "Invalid welcome asset index");
                row = canonical.at(static_cast<std::size_t>(previous));
            }
            lane.erase("identityProof");
            lane.erase("sources");
        }
        Validate(document);
        return document;
    }
}
