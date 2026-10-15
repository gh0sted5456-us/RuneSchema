#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace DragonWilds::ModOrderState {
struct Entry { std::string Name; bool Enabled=true; };
struct Snapshot {
    bool Exists=false;
    std::string Contents;
    bool operator==(const Snapshot&) const=default;
};
struct Plan {
    std::vector<Entry> Entries;
    std::vector<std::string> Active;
    bool Changed=false;
};
std::string Fold(std::string_view name);
std::vector<Entry> Parse(std::string_view contents);
Plan Reconcile(const std::vector<Entry>& entries,const std::vector<std::string>& discovered);
std::string Serialize(const std::vector<Entry>& entries,const Snapshot& previous,bool preserveComments);
Snapshot Read(const std::filesystem::path& path);
// Cooperative RuneSchema writers serialize and reject stale snapshots. External
// tools must still avoid editing the list concurrently with a save operation.
bool WriteExpected(const std::filesystem::path& path,const Snapshot& expected,std::string_view contents);
}
