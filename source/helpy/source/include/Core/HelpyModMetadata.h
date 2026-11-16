#pragma once
#include "Core/ModManager.h"
#include "Core/CacheBundleIO.h"
#include <map>

namespace PS::HelpyModMetadata {
namespace fs=std::filesystem;using Json=nlohmann::json;
inline fs::path Folder(const fs::path& root,const std::string& name){
    if(!ModManager::Name(name))throw std::runtime_error("Invalid mod folder name.");
    const auto folder=fs::absolute(root/"mods"/fs::u8path(name)).lexically_normal();CacheBundleIO::Safe(folder);
    if(!fs::is_directory(folder))throw std::runtime_error("This installed mod folder is unavailable.");return folder;
}
inline fs::path RecordFile(const fs::path& data,const fs::path& root,const std::string& name){
    const auto key=ModManager::Order::Fold(fs::absolute(root).lexically_normal().generic_string())+"/"+ModManager::Order::Fold(name);
    return data/"mod-metadata"/(CacheBundleIO::Hash(key)+".json");
}
inline std::string Fingerprint(const fs::path& folder){
    std::map<std::string,std::string> files;size_t count=0;
    for(auto it=fs::recursive_directory_iterator(folder);it!=fs::recursive_directory_iterator{};++it){
        const auto& entry=*it;const auto relative=entry.path().lexically_relative(folder);const auto text=ModManager::Order::Fold(relative.generic_string());
        const auto attributes=GetFileAttributesW(entry.path().c_str());if(attributes==INVALID_FILE_ATTRIBUTES)throw std::runtime_error("Could not inspect a mod input.");
        if(entry.is_directory()){const auto part=ModManager::Order::Fold(entry.path().filename().string());if(part=="cache"||part=="runtime"||part=="saved"||part=="logs"||part=="settings"||part=="exports"||part.starts_with(".runeschema.cache.")){it.disable_recursion_pending();continue;}if(attributes&FILE_ATTRIBUTE_REPARSE_POINT)throw std::runtime_error("Mod fingerprinting refuses redirected inputs.");continue;}
        if(attributes&FILE_ATTRIBUTE_REPARSE_POINT)throw std::runtime_error("Mod fingerprinting refuses redirected inputs.");
        if(!entry.is_regular_file()||text=="id.txt"||text==".helpy-source"||text=="mod.png"||text=="background.png")continue;
        const auto extension=ModManager::Order::Fold(entry.path().extension().string());
        if(extension!=".pak"&&extension!=".utoc"&&extension!=".ucas"&&extension!=".dll"&&extension!=".exe"&&extension!=".lua"&&extension!=".json"&&extension!=".jsonc"&&extension!=".ini"&&extension!=".uplugin"&&text!="enabled.txt"&&text!="mods.txt"&&text!="runeschema.txt")continue;
        if(++count>100000)throw std::runtime_error("Mod input list exceeds its fingerprint bound.");
        const auto size=entry.file_size();const auto stamp=entry.last_write_time();auto value=std::to_string(size)+":"+std::to_string(stamp.time_since_epoch().count());
        // shortcut: large binary changes use size/time hints; use streaming hashes if tamper-proof verification is needed.
        if(size<=256*1024)value+=":"+CacheBundleIO::Hash(CacheBundleIO::Read(entry.path(),256*1024));
        if(entry.file_size()!=size||entry.last_write_time()!=stamp)throw std::runtime_error("Mod inputs changed while fingerprinting; retry after the update completes.");files.emplace(text,std::move(value));
    }
    std::string manifest;for(const auto& [name,value]:files)manifest+=name+"|"+value+"\n";return CacheBundleIO::Hash(manifest);
}
inline Json Tracking(const fs::path& data,const fs::path& root,const std::string& name){
    const auto file=RecordFile(data,root,name);CacheBundleIO::Safe(file);if(!fs::is_regular_file(file))return Json::object();
    const auto record=Json::parse(CacheBundleIO::Read(file,16*1024));
    if(!record.is_object()||record.value("Kind",std::string{})!="HelpyLocalModIdentity"||record.value("Folder",std::string{})!=name||!CacheBundleIO::Digest(record.value("IDHash",std::string{}))||!CacheBundleIO::Digest(record.value("Fingerprint",std::string{})))throw std::runtime_error("Local mod metadata tracking is invalid; its original file was retained.");return record;
}
inline Json Read(const fs::path& root,const std::string& name,const fs::path& data){
    const auto folder=Folder(root,name),file=folder/"ID.txt";CacheBundleIO::Safe(file);const bool exists=fs::exists(file);
    const auto text=exists?CacheBundleIO::Read(file,64*1024):"ModID="+name+"\nName="+name+"\nAuthor=Unknown Author\nVersion=\nWebsite=\nDescription=\n";
    const auto tracking=Tracking(data,root,name);return {{"Exists",exists},{"Text",text},{"Track",!exists||!tracking.empty()&&tracking.value("Enabled",true)&&tracking.value("IDHash",std::string{})==CacheBundleIO::Hash(text)}};
}
inline void Save(const fs::path& root,const std::string& name,const fs::path& data,const Json& expected,const std::string& text,bool track){
    if(text.size()>64*1024)throw std::runtime_error("ID.txt exceeds 64 KiB.");const auto identity=ModIdentity::Parse(text,name);const auto folder=Folder(root,name),file=folder/"ID.txt";CacheBundleIO::Safe(file);
    const bool exists=fs::exists(file);if(exists!=expected.at("Exists").get<bool>()||(exists&&CacheBundleIO::Read(file,64*1024)!=expected.at("Text").get<std::string>()))throw std::runtime_error("ID.txt changed outside Helpy. Reopen the editor before saving.");
    const auto fingerprint=track?Fingerprint(folder):std::string{};const auto recordFile=RecordFile(data,root,name);CacheBundleIO::Safe(recordFile);
    if(exists){const auto previous=recordFile.parent_path()/"previous"/(CacheBundleIO::Hash(expected.at("Text").get<std::string>())+".txt");CacheBundleIO::Safe(previous);if(!fs::exists(previous))ConfigFiles::Write(previous,expected.at("Text").get<std::string>());}
    const auto marker=folder/".helpy-source";if(track)CacheBundleIO::Safe(marker);
    if(fs::exists(file)!=exists||(exists&&CacheBundleIO::Read(file,64*1024)!=expected.at("Text").get<std::string>()))throw std::runtime_error("ID.txt changed while preparing the update; original retained.");
    if(!ModManager::Order::WriteExpected(file,{exists,exists?expected.at("Text").get<std::string>():std::string{}},text))throw std::runtime_error("ID.txt changed before saving; original retained.");
    try{if(track){const auto record=Json{{"Kind","HelpyLocalModIdentity"},{"Folder",name},{"TrackingId",identity.Id},{"IDHash",CacheBundleIO::Hash(text)},{"Fingerprint",fingerprint}};
        ConfigFiles::Write(recordFile,record.dump());ConfigFiles::Write(marker,record.dump());}
        else if(fs::is_regular_file(recordFile)){auto record=Tracking(data,root,name);record["Enabled"]=false;ConfigFiles::Write(recordFile,record.dump());CacheBundleIO::Safe(marker);if(fs::is_regular_file(marker))ConfigFiles::Write(marker,record.dump());}
    }catch(const std::exception& error){throw std::runtime_error("ID.txt was saved, but local update tracking needs attention: "+std::string(error.what()));}
}
inline void UpdateTracking(Json& snapshot,const fs::path& data){
    if(!snapshot.contains("Root")||!snapshot.contains("Rows"))return;const auto root=fs::u8path(snapshot.at("Root").get<std::string>());
    for(auto& row:snapshot["Rows"])if(row.value("Present",false))try{
        const auto name=row.at("Name").get<std::string>();auto record=Tracking(data,root,name);if(record.empty()||!record.value("Enabled",true))continue;const auto folder=Folder(root,name),id=folder/"ID.txt";CacheBundleIO::Safe(id);
        if(fs::is_regular_file(id)&&CacheBundleIO::Hash(CacheBundleIO::Read(id,64*1024))!=record.at("IDHash").get<std::string>())continue;
        const auto fingerprint=Fingerprint(folder);record["Fingerprint"]=fingerprint;
        const auto file=RecordFile(data,root,name);if(ConfigFiles::Read(file,16*1024)!=record.dump())ConfigFiles::Write(file,record.dump());
        const auto marker=folder/".helpy-source";CacheBundleIO::Safe(marker);if(!fs::exists(marker)||ConfigFiles::Read(marker,16*1024)!=record.dump())ConfigFiles::Write(marker,record.dump());
        row["LocalFingerprint"]=fingerprint;row["TrackingId"]=record.value("TrackingId",name);row["LocalIdentity"]=true;
    }catch(const std::exception& error){row["MetadataTrackingIssue"]=error.what();}
}
}
