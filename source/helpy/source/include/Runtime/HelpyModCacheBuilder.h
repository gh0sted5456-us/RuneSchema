#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
namespace PS::HelpyModCacheBuilder {
struct View {
    bool Running=false,Repair=false;std::string State="Ready",Stage="Choose a mod and press Start.",Owner,Console;
    unsigned Percent=0;std::filesystem::path Cache,Output,Job;std::vector<std::string> Warnings;
};
const View& Current();
struct BatchView {bool Active=false,Paused=false;unsigned Total=0,Completed=0,Failed=0,Skipped=0;std::vector<std::string> Results;};
const BatchView& CurrentBatch();
std::filesystem::path InstalledFolder(const std::filesystem::path& root,const std::filesystem::path& folder,const std::string& owner);
std::wstring Arguments(const std::filesystem::path& script,const std::filesystem::path& input,bool creation,const std::filesystem::path& progress,const std::filesystem::path& control,bool installNet,const std::filesystem::path& output={},const std::string& platform="Win64",const std::filesystem::path& backups={});
bool StartInstalled(const std::filesystem::path& root,const std::filesystem::path& folder,const std::string& owner,bool installNet);
bool StartEnabled(const std::filesystem::path& root,const nlohmann::json& rows,bool installNet);
bool StartCreation(const std::vector<std::filesystem::path>& inputs,const nlohmann::json& metadata,const std::string& platform,bool installNet);
bool StartRepair(const std::filesystem::path& root,const std::string& version);
void Control(const std::string& action);
// shortcut: queue advancement waits while Helpy is hidden; add background completion handling when tray batches are needed.
void Poll(bool advance=true); // A running explicit job only, at most four reads/second while Helpy is visible.
std::filesystem::path ReadyBundle();
void BundleApplied();
void Shutdown() noexcept;
}
