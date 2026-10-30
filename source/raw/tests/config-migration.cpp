#include "Utility/ConfigStore.h"
#include <iostream>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif
namespace fs=std::filesystem;
#define Check(expression) do{if(!(expression))throw std::runtime_error("Failed: " #expression);}while(false)
template<class F>void Reject(F&& operation){bool failed=false;try{operation();}catch(const std::exception&){failed=true;}Check(failed);}
int main(int argc,char** argv){
    try{
        using namespace PS;
        const auto root=fs::current_path()/("config-migration-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Check(fs::create_directory(root));
        const auto write=[](const fs::path& path,std::string_view text){ConfigFiles::Write(path,text);};
        const auto legacy=std::string("// preserve this original comment\n")+R"({"advancedLogging":true,"loaders":{"spawns":false},"helpyAuthority":{"permittedPlayerNames":["NuLL; # Silver","Unicode \u2605"]},"future":{"enabled":true,"empty":{},"nested":{"value":7}}})";
        const auto decoded=DecodeSettings(StripJsonComments(legacy));
        const auto document=SettingsDocument(decoded,nlohmann::json::parse(StripJsonComments(legacy)));
        const auto ini=EncodeIniDocument(document);
        Check(DecodeIniDocument(ini)==document);
        Check(DecodeSettings(DecodeIniDocument(ini).dump()).advancedLogging);
        Check(!DecodeSettings("{}").archiveLogs);
    Check(!DecodeSettings("{}").logsInUE4SS);
    auto logLocation=PSConfigSettings{};logLocation.logsInUE4SS=true;
    Check(DecodeSettings(DecodeIniDocument(EncodeIniDocument(SettingsDocument(logLocation))).dump()).logsInUE4SS);
        auto archiveSettings=PSConfigSettings{};archiveSettings.archiveLogs=true;
        Check(DecodeSettings(DecodeIniDocument(EncodeIniDocument(SettingsDocument(archiveSettings))).dump()).archiveLogs);
        Reject([]{DecodeIniDocument("[RuneSchema]\narchiveLogs=yes\n");});
        Check(DecodeIniDocument("\xEF\xBB\xBF[RuneSchema]\nadvancedLogging = true ; note\n[plugins]\ncompatibilityNotices = quiet # note\n").at("plugins").at("compatibilityNotices")=="quiet");
        Reject([]{DecodeIniDocument("[RuneSchema]\nadvancedLogging=true\nadvancedLogging=false");});
        Reject([]{DecodeIniDocument("[RuneSchema]\nadvancedLogging=yes");});
        Reject([]{DecodeIniDocument("[plugins]\ncompatibilityNotices=invalid");});
        Reject([]{DecodeIniDocument("[broken\nkey=true");});
        Reject([]{DecodeIniDocument("[helpyAuthority]\npermittedPlayerNames=[\"broken\"");});
        Reject([]{DecodeIniDocument("[RuneSchema]\nloaders=true\n[loaders]\nspawns=true");});
        Reject([]{DecodeIniDocument("; only comments\n");});
        const auto fresh=root/"fresh";
        auto created=SettingsStore::Load(fresh);
        Check(created.File.filename()=="settings.ini"&&fs::exists(created.File)&&!fs::exists(fresh/"settings.jsonc"));
        Check(!created.Settings.advancedLogging&&SettingsStore::Load(fresh).Settings.loaders.spawns);
        const auto migrate=root/"migrate";
        write(migrate/"settings.jsonc",legacy);
        // A shipped INI beside an existing JSONC must not mask user values.
        write(migrate/"settings.ini",EncodeIniDocument(SettingsDocument(PSConfigSettings{})));
        auto first=SettingsStore::Load(migrate);
        Check(first.Settings.advancedLogging&&!first.Settings.loaders.spawns);
        Check(first.File.extension()==".ini"&&fs::exists(migrate/"settings.ini.migration"));
        Check(ConfigFiles::Read(migrate/"settings.jsonc")==legacy);
        Check(first.Document.at("future")==document.at("future"));
        auto second=SettingsStore::Load(migrate);
        Check(second.Settings.advancedLogging&&!fs::exists(migrate/"settings.jsonc")&&!fs::exists(migrate/"settings.ini.migration"));
        bool archived=false;
        for(const auto& file:fs::directory_iterator(migrate))if(file.path().filename().string().starts_with("settings.jsonc.migrated-")){Check(ConfigFiles::Read(file.path())==legacy);archived=true;}
        Check(archived&&SettingsStore::Load(migrate).Document.at("future")==document.at("future"));
        const auto edits=root/"legacy-edits";write(edits/"settings.jsonc",legacy);(void)SettingsStore::Load(edits);
        write(edits/"settings.jsonc",R"({"advancedLogging":false,"loaders":{"spawns":false}})");
        Check(!SettingsStore::Load(edits).Settings.advancedLogging&&fs::exists(edits/"settings.jsonc"));
        Check(!SettingsStore::Load(edits).Settings.advancedLogging&&!fs::exists(edits/"settings.jsonc"));
        const auto conflict=root/"conflict";write(conflict/"settings.jsonc",legacy);(void)SettingsStore::Load(conflict);
        write(conflict/"settings.ini","[RuneSchema]\nadvancedLogging=false\n");write(conflict/"settings.jsonc","{\"advancedLogging\":false}");
        Check(!SettingsStore::Load(conflict).Settings.advancedLogging&&fs::exists(conflict/"settings.jsonc"));
        Check(ConfigFiles::Read(conflict/"settings.ini")=="[RuneSchema]\nadvancedLogging=false\n");
        const auto iniEdit=root/"ini-edit";write(iniEdit/"settings.jsonc",legacy);(void)SettingsStore::Load(iniEdit);
        write(iniEdit/"settings.ini","[RuneSchema]\nadvancedLogging=false\n");
        Check(!SettingsStore::Load(iniEdit).Settings.advancedLogging&&!fs::exists(iniEdit/"settings.jsonc"));
        const auto invalid=root/"invalid-ini";write(invalid/"settings.jsonc",legacy);(void)SettingsStore::Load(invalid);
        write(invalid/"settings.ini","[RuneSchema]\nadvancedLogging=wrong\n");
        Check(SettingsStore::Load(invalid).Settings.advancedLogging&&fs::exists(invalid/"settings.jsonc"));
        Check(ConfigFiles::Read(invalid/"settings.ini")=="[RuneSchema]\nadvancedLogging=wrong\n");
        const auto invalidJson=root/"invalid-json";write(invalidJson/"settings.jsonc","{broken");write(invalidJson/"settings.ini",ini);
        Check(SettingsStore::Load(invalidJson).Settings.advancedLogging&&ConfigFiles::Read(invalidJson/"settings.jsonc")=="{broken");
        const auto invalidReceipt=root/"invalid-receipt";write(invalidReceipt/"settings.jsonc",legacy);(void)SettingsStore::Load(invalidReceipt);
        write(invalidReceipt/"settings.ini.migration","broken");
        Check(SettingsStore::Load(invalidReceipt).Settings.advancedLogging&&fs::exists(invalidReceipt/"settings.jsonc"));
        const auto blocked=root/"blocked";write(blocked/"settings.jsonc",legacy);fs::create_directory(blocked/"settings.ini");
        auto deferred=SettingsStore::Load(blocked);Check(deferred.Settings.advancedLogging&&deferred.File.extension()==".jsonc");
        Check(ConfigFiles::Read(blocked/"settings.jsonc")==legacy);
        const auto bad=root/"bad";write(bad/"settings.jsonc","{broken");
        Reject([&]{SettingsStore::Load(bad);});Check(ConfigFiles::Read(bad/"settings.jsonc")=="{broken"&&!fs::exists(bad/"settings.ini"));
#ifdef _WIN32
        const auto firstLocked=root/"locked-first-write";write(firstLocked/"settings.jsonc",legacy);
        write(firstLocked/"settings.ini","[RuneSchema]\nadvancedLogging=false\n");
        const auto firstIni=firstLocked/"settings.ini";
        HANDLE firstHandle=CreateFileW(firstIni.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        Check(firstHandle!=INVALID_HANDLE_VALUE);
        const auto stillLegacy=SettingsStore::Load(firstLocked);CloseHandle(firstHandle);
        Check(stillLegacy.Settings.advancedLogging&&stillLegacy.File.extension()==".jsonc");
        Check(ConfigFiles::Read(firstLocked/"settings.jsonc")==legacy&&!fs::exists(firstLocked/"settings.ini.migration"));
        Check(ConfigFiles::Read(firstIni)=="[RuneSchema]\nadvancedLogging=false\n");
        Check(SettingsStore::Load(firstLocked).File.extension()==".ini"&&fs::exists(firstLocked/"settings.jsonc"));
        const auto unreadable=root/"locked-read";write(unreadable/"settings.jsonc",legacy);write(unreadable/"settings.ini",ini);
        const auto unreadableJson=unreadable/"settings.jsonc";
        HANDLE readHandle=CreateFileW(unreadableJson.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        Check(readHandle!=INVALID_HANDLE_VALUE);
        const auto fallbackIni=SettingsStore::Load(unreadable);CloseHandle(readHandle);
        Check(fallbackIni.Settings.advancedLogging&&fallbackIni.File.extension()==".ini"&&ConfigFiles::Read(unreadableJson)==legacy);
        const auto locked=root/"locked";write(locked/"settings.jsonc",legacy);(void)SettingsStore::Load(locked);
        const auto path=locked/"settings.jsonc";
        HANDLE handle=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        Check(handle!=INVALID_HANDLE_VALUE);
        const auto loadedLocked=SettingsStore::Load(locked);
        Check(loadedLocked.Settings.advancedLogging&&fs::exists(path));CloseHandle(handle);
        Check(SettingsStore::Load(locked).Settings.advancedLogging&&!fs::exists(path));
#endif
        if(argc>1){
            const auto shipped=DecodeIniDocument(ConfigFiles::Read(fs::path(argv[1])/"source/settings/settings.ini"));
            const auto previous=nlohmann::json::parse(StripJsonComments(ConfigFiles::Read(fs::path(argv[1])/"source/settings/settings.jsonc")));
            Check(SettingsDocument(DecodeSettings(shipped.dump()))==SettingsDocument(DecodeSettings(previous.dump())));
            const auto build=ConfigFiles::Read(fs::path(argv[1])/"build/build.ps1");
            Check(build.find("$sourceSettingsFile = Join-Path $SourceRoot 'settings\\settings.ini'")!=std::string::npos);
            Check(build.find("Remove-Item -LiteralPath $legacyPackagedSettings -Force")!=std::string::npos);
        }
        fs::remove_all(root);
        std::cout<<"INI roundtrip, unknown keys, two-launch migration, defaults, conflicts, malformed files, backups and failed cleanup passed.\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
