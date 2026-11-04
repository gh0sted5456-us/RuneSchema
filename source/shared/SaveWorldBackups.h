#pragma once
#include "SaveBackupStore.h"

namespace PS::WorldBackups {
using namespace SaveBackups;
inline constexpr size_t MaximumWorldBytes=512ull*1024*1024;
inline fs::path Directory(const fs::path& root,const std::string& runtime){return root/"core/backups"/Runtime(runtime)/"worlds";}
inline bool Enabled(const fs::path& directory){return ExperimentalWorldRecovery(directory.parent_path()/"characters");}
inline std::string Slot(const fs::path& value){const auto text=value.u8string();std::string slot(text.begin(),text.end());
    if(slot.empty()||slot.size()>180||slot=="."||slot==".."||slot=="EnhancedInputUserSettings"||slot.back()=='.'||slot.back()==' '||slot.find_first_of("<>:\"/\\|?*")!=slot.npos||std::any_of(slot.begin(),slot.end(),[](unsigned char c){return c<32;}))throw std::runtime_error("Invalid native world slot");return slot;}
inline std::string Key(const std::string& slot){Slot(fs::u8path(slot));auto value=CacheBundleIO::Hash(slot).substr(0,32);for(auto& c:value)if(c>='a'&&c<='f')c=char(c-'a'+'A');return value;}
inline void Validate(std::string_view bytes){
    if(bytes.size()<8||bytes.size()>MaximumWorldBytes||bytes.substr(0,4)!="SAVE")throw std::runtime_error("Backup is not a bounded native world SAVE archive");
    const auto* p=reinterpret_cast<const unsigned char*>(bytes.data()+4);const uint32_t length=uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);
    if(size_t(length)+8!=bytes.size())throw std::runtime_error("World SAVE archive length does not match its root chunk");
}
inline fs::path File(const fs::path& directory,const std::string& key,const std::string& hash){if(SaveBackups::Guid(key)!=key||!CacheBundleIO::Digest(hash))throw std::runtime_error("Invalid world backup identity");const auto file=directory/key/(hash+".sav.bak");CacheBundleIO::Safe(file);return file;}
inline fs::path Store(const fs::path& directory,const std::string& slot,std::string_view bytes,const std::string& reason,Json context=Json::object()){
    if(!Enabled(directory))throw std::runtime_error("Enable experimental world recovery in Helpy settings first");
    Validate(bytes);const auto key=Key(slot),hash=CacheBundleIO::Hash(bytes);const auto file=File(directory,key,hash);Lock lock(directory);
    if(reason!="Before cleanup"&&reason!="Automatic"&&reason!="Before restore"&&reason!="Manual")throw std::runtime_error("Unknown world backup reason");
    if(!fs::exists(file))CacheBundleIO::Publish(file,bytes);
    Verify(file,bytes);
    Json meta={{"Format","RuneSchemaWorldBackup"},{"Version",1},{"Slot",slot},{"Guid",key},{"SHA256",hash},{"Bytes",bytes.size()},{"CreatedUtc",Now()},{"Reasons",Json::array()}};
    if(fs::exists(Metadata(file))){const auto old=Json::parse(CacheBundleIO::Read(Metadata(file),512*1024));if(old.value("Slot",std::string{})!=slot||old.value("SHA256",std::string{})!=hash)throw std::runtime_error("World backup metadata changed");meta=old;}
    auto& reasons=meta["Reasons"];if(!reasons.is_array()||reasons.size()>8)throw std::runtime_error("Invalid world backup reasons");if(std::find(reasons.begin(),reasons.end(),reason)==reasons.end())reasons.push_back(reason);
    context=Context(std::move(context));
    for(const auto& [name,value]:context.items())if(name=="LastPlayedUtc"||(name=="Mods"&&!meta.contains(name)))meta[name]=value;
    meta["LastObservedUtc"]=Now();CacheBundleIO::Publish(Metadata(file),meta.dump());return file;
}
inline fs::path BeforeCleanup(const fs::path& directory,const std::string& slot,std::string_view bytes,Json context,std::string& issue){
    issue.clear();
    try{if(!Enabled(directory))return {};return Store(directory,slot,bytes,"Before cleanup",std::move(context));}
    catch(const std::exception& error){issue=error.what();return {};}
}
struct Entry:SaveBackups::Entry {std::string Slot;};
inline std::vector<Entry> List(const fs::path& directory){
    std::vector<Entry> rows;CacheBundleIO::Safe(directory);if(!fs::exists(directory))return rows;size_t examined=0;
    for(const auto& owner:fs::directory_iterator(directory)){if(++examined>MaximumEntries*2)throw std::runtime_error("World backup listing exceeds its limit");if(SaveBackups::Guid(owner.path().filename().string()).empty())continue;CacheBundleIO::Safe(owner.path());if(!owner.is_directory())continue;
        for(const auto& file:fs::directory_iterator(owner.path())){if(++examined>MaximumEntries*4||rows.size()>=MaximumEntries)throw std::runtime_error("World backup listing exceeds its limit");const auto name=file.path().filename().string();if(name.size()!=72||!name.ends_with(".sav.bak"))continue;const auto hash=name.substr(0,64);if(!CacheBundleIO::Digest(hash))continue;CacheBundleIO::Safe(file.path());
            try{const auto meta=Json::parse(CacheBundleIO::Read(Metadata(file.path()),512*1024));Entry row;row.Slot=meta.at("Slot").get<std::string>();row.Guid=Key(row.Slot);row.Hash=hash;row.Name=row.Slot;
                if(owner.path().filename().string()!=row.Guid||meta.value("SHA256",std::string{})!=hash)throw std::runtime_error("World backup identity mismatch");
                row.Bytes=file.file_size();if(!row.Bytes||row.Bytes>MaximumWorldBytes)continue;row.CreatedUtc=meta.value("CreatedUtc",std::string{});row.LastPlayedUtc=meta.value("LastPlayedUtc",std::string{});row.Mods=meta.value("Mods",Json());row.Reasons=meta.value("Reasons",Json::array());row.ValidMetadata=true;rows.push_back(std::move(row));
            }catch(...){} // Unidentified archives are never assigned to a guessed world slot.
        }
    }std::stable_sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){return a.CreatedUtc>b.CreatedUtc;});return rows;
}
inline std::string Read(const fs::path& directory,const Entry& entry){if(!Enabled(directory))throw std::runtime_error("Enable experimental world recovery in Helpy settings first");if(Key(entry.Slot)!=entry.Guid)throw std::runtime_error("World slot identity mismatch");const auto bytes=CacheBundleIO::Read(File(directory,entry.Guid,entry.Hash),MaximumWorldBytes);Validate(bytes);if(CacheBundleIO::Hash(bytes)!=entry.Hash)throw std::runtime_error("World backup hash mismatch");return bytes;}
inline fs::path Pending(const fs::path& directory,const std::string& slot){return SaveBackups::Pending(directory,Key(slot));}
inline void QueueRestore(const fs::path& directory,const Entry& entry,const std::function<bool()>& stopped){if(!stopped||!stopped())throw std::runtime_error("Close Dragonwilds before scheduling world recovery");(void)Read(directory,entry);Lock lock(directory);const auto pending=Pending(directory,entry.Slot);if(!stopped()||fs::exists(pending)||fs::exists(fs::path(pending.wstring()+L".processing")))throw std::runtime_error("A world restore is queued, needs review, or the game started");
    CacheBundleIO::Publish(Pending(directory,entry.Slot),Json{{"Format","RuneSchemaWorldRestore"},{"Slot",entry.Slot},{"SHA256",entry.Hash},{"RequestedUtc",Now()}}.dump());}
inline bool ApplyQueued(const fs::path& directory,const std::string& slot,std::string_view current,const std::function<void(const std::string&)>& apply){
    if(!Enabled(directory))return false;
    const auto pending=Pending(directory,slot);CacheBundleIO::Safe(pending);if(!fs::exists(pending))return false;const auto requested=CacheBundleIO::Read(pending,4096);const auto request=Json::parse(requested);
    if(request.value("Format",std::string{})!="RuneSchemaWorldRestore"||request.value("Slot",std::string{})!=slot)throw std::runtime_error("World restore request identity mismatch");Entry entry;entry.Slot=slot;entry.Guid=Key(slot);entry.Hash=request.at("SHA256").get<std::string>();const auto bytes=Read(directory,entry);
    Store(directory,slot,current,"Before restore",{{"Mods",ActiveMods()}});Lock lock(directory);const auto processing=fs::path(pending.wstring()+L".processing");CacheBundleIO::Safe(processing);
    if(CacheBundleIO::Read(pending,4096)!=requested)throw std::runtime_error("World restore request changed; no world was replaced");
    if(!MoveFileExW(pending.c_str(),processing.c_str(),MOVEFILE_WRITE_THROUGH))throw std::runtime_error("World restore is busy or needs review");
    apply(bytes);if(!MoveFileExW(processing.c_str(),fs::path(processing.wstring()+L".loaded").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("World loaded, but its restore receipt needs review");return true;
}
inline void RestoreFile(const fs::path& directory,const Entry& entry,const fs::path& nativeRoot,const std::string& expected,const std::function<bool()>& stopped){
    const auto target=nativeRoot/(fs::u8path(Slot(fs::u8path(entry.Slot))).wstring()+L".sav");CacheBundleIO::Safe(nativeRoot);CacheBundleIO::Safe(target);
    if(!stopped||!stopped())throw std::runtime_error("Close Dragonwilds before restoring a world");const auto before=CacheBundleIO::Read(target,MaximumWorldBytes);Validate(before);const auto bytes=Read(directory,entry);
    if(CacheBundleIO::Hash(before)!=expected)throw std::runtime_error("World changed; refresh before restoring");Store(directory,entry.Slot,before,"Before restore");Lock lock(directory);
    if(!stopped())throw std::runtime_error("The game started; restore was not applied");Verify(target,before);CacheBundleIO::Publish(target,bytes);Verify(target,bytes);
}
}
