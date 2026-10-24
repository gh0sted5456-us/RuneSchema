#pragma once

// SPUD building-record preflight. This is deliberately independent of UE4SS so
// the exact byte transformation can be tested without loading the game.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace PS::WorldBuildingSavePreflight {

using Bytes = std::vector<std::uint8_t>;

struct Chunk {
    std::size_t offset{};
    std::size_t start{};
    std::size_t end{};
    std::uint32_t length{};
};

struct Piece {
    std::uint32_t id{};
    std::string persistenceId;
    std::size_t offset{};
    std::size_t end{};
    std::size_t linksOffset{};
    std::vector<std::uint32_t> links;
};

struct ParsedSave {
    Chunk root;
    Chunk piecesChunk;
    std::vector<Chunk> ancestors;
    std::vector<Piece> pieces;
};

inline void Require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

inline std::uint32_t U32(std::span<const std::uint8_t> data, std::size_t at) {
    Require(at <= data.size() && data.size() - at >= 4, "truncated uint32");
    return std::uint32_t(data[at]) | (std::uint32_t(data[at + 1]) << 8)
        | (std::uint32_t(data[at + 2]) << 16) | (std::uint32_t(data[at + 3]) << 24);
}

inline void PutU32(Bytes& data, std::size_t at, std::uint32_t value) {
    Require(at <= data.size() && data.size() - at >= 4, "uint32 write out of bounds");
    for (int i = 0; i != 4; ++i) data[at + i] = static_cast<std::uint8_t>(value >> (i * 8));
}

inline bool TagAt(std::span<const std::uint8_t> data, std::size_t at, const char* tag) {
    return at <= data.size() && data.size() - at >= 4
        && std::memcmp(data.data() + at, tag, 4) == 0;
}

inline Chunk ReadChunk(std::span<const std::uint8_t> data, std::size_t at, const char* tag) {
    Require(TagAt(data, at, tag), "unexpected chunk tag");
    const auto length = U32(data, at + 4);
    Require(at <= data.size() && data.size() - at >= 8
        && length <= data.size() - at - 8, "chunk exceeds save bounds");
    return { at, at + 8, at + 8 + length, length };
}

inline std::string ReadString(std::span<const std::uint8_t> data,
    std::size_t& cursor, std::size_t end) {
    Require(cursor <= end && end - cursor >= 4, "truncated FString length");
    const auto signedLength = static_cast<std::int32_t>(U32(data, cursor));
    cursor += 4;
    Require(signedLength != 0 && signedLength >= -256 && signedLength <= 256,
        "unsupported FString length");
    const bool wide = signedLength < 0;
    const auto characters = static_cast<std::size_t>(wide ? -signedLength : signedLength);
    const auto bytes = characters * (wide ? 2u : 1u);
    Require(cursor <= end && bytes <= end - cursor, "truncated FString");
    std::string result;
    result.reserve(characters - 1);
    for (std::size_t i = 0; i + 1 < characters; ++i) {
        const auto value = data[cursor + i * (wide ? 2u : 1u)];
        Require(value >= 32 && value < 127, "non-ASCII building identifier");
        if (wide) Require(data[cursor + i * 2 + 1] == 0,
            "non-ASCII wide building identifier");
        result.push_back(static_cast<char>(value));
    }
    Require(data[cursor + bytes - 1] == 0, "unterminated FString");
    if (wide) Require(data[cursor + bytes - 2] == 0, "unterminated wide FString");
    cursor += bytes;
    Require(!result.empty(), "empty building identifier");
    return result;
}

inline std::vector<Piece> ReadPieces(std::span<const std::uint8_t> data, const Chunk& pces) {
    std::vector<Piece> result;
    std::unordered_set<std::uint32_t> ids;
    auto cursor = pces.start;
    while (cursor < pces.end) {
        const auto start = cursor;
        Require(pces.end - cursor >= 4, "truncated building instance ID");
        const auto id = U32(data, cursor);
        Require(id > 0 && id <= 100000000 && ids.insert(id).second,
            "invalid or duplicate building instance ID");
        cursor += 4;
        auto persistenceId = ReadString(data, cursor, pces.end);
        Require(pces.end - cursor >= 40, "truncated building transform");
        for (int i = 0; i < 3; ++i) {
            double position{};
            std::memcpy(&position, data.data() + cursor + i * 8, sizeof(position));
            Require(std::isfinite(position) && std::abs(position) < 100000000,
                "invalid building position");
            float orientation{};
            std::memcpy(&orientation, data.data() + cursor + 24 + i * 4, sizeof(orientation));
            Require(std::isfinite(orientation), "invalid building orientation");
        }
        cursor += 36;
        const auto anchors = U32(data, cursor);
        Require(anchors <= 256, "excessive building anchors");
        cursor += 4;
        for (std::uint32_t i = 0; i < anchors; ++i)
            (void)ReadString(data, cursor, pces.end);
        Require(pces.end - cursor >= std::size_t(anchors) * 8 + 5,
            "truncated building anchor metadata");
        cursor += std::size_t(anchors) * 8;
        Require(data[cursor] <= 1, "unknown building record flag");
        ++cursor;
        const auto linksOffset = cursor;
        const auto linkCount = U32(data, cursor);
        Require(linkCount <= 4096, "excessive building connections");
        cursor += 4;
        Require(pces.end - cursor >= std::size_t(linkCount) * 4,
            "truncated building connections");
        std::vector<std::uint32_t> links;
        links.reserve(linkCount);
        for (std::uint32_t i = 0; i < linkCount; ++i) {
            links.push_back(U32(data, cursor));
            cursor += 4;
        }
        result.push_back({ id, std::move(persistenceId), start, cursor,
            linksOffset, std::move(links) });
        Require(result.size() <= 100000, "excessive building records");
    }
    Require(cursor == pces.end, "building records do not consume Pces");
    return result;
}

inline ParsedSave Parse(std::span<const std::uint8_t> data) {
    Require(data.size() >= 8 && data.size() <= 512u * 1024u * 1024u,
        "unsupported save size");
    const auto root = ReadChunk(data, 0, "SAVE");
    Require(root.end == data.size(), "SAVE length mismatch");
    std::vector<Chunk> candidates;
    for (std::size_t at = 0; at + 8 <= data.size(); ++at)
        if (TagAt(data, at, "Pces")) {
            candidates.push_back(ReadChunk(data, at, "Pces"));
            Require(candidates.size() == 1, "ambiguous Pces chunk");
        }
    Require(candidates.size() == 1, "Pces chunk unavailable");
    const auto pces = candidates.front();
    std::vector<Chunk> ancestors;
    for (const char* tag : { "SAVE", "LVLS", "LEVL", "LATS", "NOBJ", "CUST" }) {
        std::vector<Chunk> matches;
        for (std::size_t at = 0; at < pces.offset && at + 8 <= data.size(); ++at) {
            if (!TagAt(data, at, tag)) continue;
            const auto length = U32(data, at + 4);
            if (length <= data.size() - at - 8
                && at + 8 + length >= pces.end)
                matches.push_back(ReadChunk(data, at, tag));
        }
        Require(matches.size() == 1, "ambiguous building chunk ancestry");
        ancestors.push_back(matches.front());
    }
    for (std::size_t i = 1; i < ancestors.size(); ++i)
        Require(ancestors[i].offset > ancestors[i - 1].offset
            && ancestors[i].end <= ancestors[i - 1].end,
            "building chunk ancestry does not nest");
    const auto& cust = ancestors.back();
    Require(cust.length >= 4 && U32(data, cust.start) == cust.length - 4,
        "unknown CUST array contract");
    return { root, pces, std::move(ancestors), ReadPieces(data, pces) };
}

struct Repair {
    Bytes bytes;
    std::vector<std::uint32_t> removedIds;
    std::vector<std::string> removedPersistenceIds;
};

inline Repair RemoveUnresolved(std::span<const std::uint8_t> original,
    const std::unordered_set<std::string>& livePersistenceIds) {
    Require(!livePersistenceIds.empty(), "live building registry is empty");
    const auto parsed = Parse(original);
    std::unordered_set<std::uint32_t> removed;
    Repair result;
    for (const auto& piece : parsed.pieces)
        if (!livePersistenceIds.contains(piece.persistenceId)) {
            removed.insert(piece.id);
            result.removedIds.push_back(piece.id);
            result.removedPersistenceIds.push_back(piece.persistenceId);
        }
    if (removed.empty()) return result; // No-op: caller must not rewrite the save.

    Bytes kept;
    for (const auto& piece : parsed.pieces) {
        if (removed.contains(piece.id)) continue;
        kept.insert(kept.end(), original.begin() + piece.offset,
            original.begin() + piece.linksOffset);
        const auto countAt = kept.size();
        kept.resize(countAt + 4);
        std::uint32_t count = 0;
        for (const auto link : piece.links)
            if (!removed.contains(link)) {
                const auto at = kept.size();
                kept.resize(at + 4);
                PutU32(kept, at, link);
                ++count;
            }
        PutU32(kept, countAt, count);
    }
    Require(kept.size() < parsed.piecesChunk.length, "removal did not shrink Pces");
    const auto reduction = parsed.piecesChunk.length - kept.size();
    Require(reduction <= std::numeric_limits<std::uint32_t>::max(),
        "building repair exceeds chunk length range");
    result.bytes.reserve(original.size() - reduction);
    result.bytes.insert(result.bytes.end(), original.begin(),
        original.begin() + parsed.piecesChunk.start);
    result.bytes.insert(result.bytes.end(), kept.begin(), kept.end());
    result.bytes.insert(result.bytes.end(), original.begin() + parsed.piecesChunk.end,
        original.end());
    for (const auto& chunk : parsed.ancestors) {
        Require(chunk.length >= reduction, "ancestor chunk underflow");
        PutU32(result.bytes, chunk.offset + 4,
            static_cast<std::uint32_t>(chunk.length - reduction));
    }
    PutU32(result.bytes, parsed.piecesChunk.offset + 4,
        static_cast<std::uint32_t>(kept.size()));
    const auto& cust = parsed.ancestors.back();
    PutU32(result.bytes, cust.start,
        static_cast<std::uint32_t>(cust.length - 4 - reduction));

    const auto verified = Parse(result.bytes);
    Require(verified.pieces.size() + removed.size() == parsed.pieces.size(),
        "repaired building count mismatch");
    for (const auto& piece : verified.pieces) {
        Require(!removed.contains(piece.id) && livePersistenceIds.contains(piece.persistenceId),
            "repaired save retains an unresolved building");
        for (const auto link : piece.links)
            Require(!removed.contains(link), "repaired save retains an orphan connection");
    }
    return result;
}

} // namespace PS::WorldBuildingSavePreflight
