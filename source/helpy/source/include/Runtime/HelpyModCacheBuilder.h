#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
namespace PS::HelpyModCacheBuilder {
struct View {
    bool Running=false,Repair=false,Vanilla=false,Hotfix=false;std::string State="Ready",Stage="Choose a mod and press Start.",Owner,Console,SourceId,SourceVersion,SourceFingerprint;
    unsigned Percent=0;std::filesystem::path Cache,Output,Job;std::vector<std::string> Warnings;
};
const View& Current();
struct BatchView {bool Active=false,Paused=false;unsigned Total=0,Completed=0,Failed=0,Skipped=0;std::vector<std::string> Results;};
const BatchView& CurrentBatch();
std::filesystem::path InstalledFolder(const std::filesystem::path& root,const std::filesystem::path& folder,const std::string& owner);
std::wstring Arguments(const std::filesystem::path& script,const std::filesystem::path& input,bool creation,const std::filesystem::path& progress,const std::filesystem::path& control,bool installNet,const std::filesystem::path& output={},const std::string& platform="Win64",const std::filesystem::path& backups={},bool vanilla=false);
bool StartInstalled(const std::filesystem::path& root,const std::filesystem::path& folder,const std::string& owner,bool installNet);
bool StartVanilla(const std::filesystem::path& root,const std::filesystem::path& cache,const std::string& version,bool installNet);
bool StartEnabled(const std::filesystem::path& root,const nlohmann::json& rows,bool installNet);
bool StartCreation(const std::vector<std::filesystem::path>& inputs,const nlohmann::json& metadata,const std::string& platform,bool installNet);
bool StartRepair(const std::filesystem::path& root,const std::string& version);
bool StartHotfix(const std::filesystem::path& root,const std::string& version,const nlohmann::json& release,const std::string& identity,const std::filesystem::path& receipt);
bool ApplyHotfix();
void Control(const std::string& action);
void Poll(bool advance=true); // Explicit jobs only; progress reads are limited to four/second, including in the tray.
std::filesystem::path ReadyBundle();
void BundleApplied();
void Shutdown() noexcept;
}
