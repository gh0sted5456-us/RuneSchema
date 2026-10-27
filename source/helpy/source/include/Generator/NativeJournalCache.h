#pragma once
#include "Core/ProcessExitFuture.h"
#include "Core/JournalWorkspace.h"
#include "Core/HelpyIconPack.h"
#include "Generator/ToolRequest.h"
#include "Runtime/HostServices.h"
#include <future>
#include <fstream>
#include <set>
namespace PS::NativeJournalCache {
using Json=nlohmann::json;using Clock=std::chrono::steady_clock;
inline Json Entries=Json::array(),Building=Json::array();
inline std::set<std::string> Seen;
inline std::string Key,ScanKey,Status="Reading native journal cache.";
inline bool Initialized=false,Loaded=false,Refresh=true,Scanning=false;
inline int Cursor=0;
inline size_t BuildingBytes=0;
inline uint64_t Pending=0,Sequence=0x5600000000000000ull,Generation=0;
inline uint64_t Revision=1;
inline Clock::time_point Next{},Started{};
inline PS::ProcessExitFuture<Json> Reader;
inline PS::ProcessExitFuture<Json> BundleReader;
inline Json BundleArt=Json::array();
inline std::string BundleStamp;
inline Clock::time_point NextBundle{};
inline PS::ProcessExitFuture<std::string> Writer;
inline std::optional<Json> SaveWanted;
inline void Poll(){
    if(!Initialized){Initialized=true;const auto file=HostServices::CacheDirectory()/"journal"/"catalog.json";
        const auto supplied=HostServices::ModDirectory()/"assets"/"journal"/"vanilla.json",local=HostServices::CacheDirectory()/"journal"/"vanilla.json";
        Reader=std::async(std::launch::async,[file,supplied,local]()->Json{
            const auto read=[](const auto& path)->Json{try{HelpyIconPack::Safe(path);if(!std::filesystem::exists(path)||std::filesystem::file_size(path)>16*1024*1024)return Json::object();std::ifstream stream(path);return JournalWorkspace::ReadCache(Json::parse(stream));}catch(...){return Json::object();}};
            auto doc=read(file);if(doc.contains("Entries"))return doc;
            auto base=read(local);if(!base.contains("Entries"))base=read(supplied);if(base.contains("Entries"))for(const auto& row:base["Entries"])if(row.value("SourceKind",std::string{})!="vanilla"||row.value("ModifiedByRuneSchema",false))return Json::object();
            return base;
        });
    }
    if(Reader.valid()&&Reader.wait_for(std::chrono::seconds(0))==std::future_status::ready){const auto doc=Reader.get();Loaded=true;if(doc.contains("Entries")){Entries=doc["Entries"];++Revision;for(auto& row:Entries)row["ObservedCurrent"]=false;Key=doc.value("SourceKey",std::string{});Status="Cached native entries ready; checking the runtime in the background.";}}
    if(BundleReader.valid()&&BundleReader.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        const auto doc=BundleReader.get();
        if(doc.contains("Records")){
            BundleStamp=doc.value("Stamp",std::string{});BundleArt=Json::array();auto merged=Json::array();std::set<std::string> paths;size_t bytes=0;
            for(const auto& row:Entries)if(!row.value("BundleReference",false)){merged.push_back(row);paths.insert(row.value("Path",std::string{}));bytes+=row.dump().size();}
            for(const auto& reference:doc["Records"]){auto row=reference.at("Data");if(BundleArt.size()<32768)BundleArt.push_back(row);
                if(reference.at("Kind")=="journal"&&merged.size()<8192&&paths.insert(row.value("Path",std::string{})).second){const auto length=row.dump().size();if(bytes+length<=12*1024*1024){bytes+=length;merged.push_back(std::move(row));}}
            }Entries=std::move(merged);++Revision;
        }
    }
    if(Loaded&&!BundleReader.valid()&&Clock::now()>=NextBundle&&HelpySourceCache::Ready()){
        NextBundle=Clock::now()+std::chrono::seconds(3);const auto snapshot=HelpySourceCache::Current;const auto root=HostServices::ModDirectory(),cache=HelpySourceCache::Directory(false);const auto previous=BundleStamp;
        BundleReader=std::async(std::launch::async,[snapshot,root,cache,previous]()->Json{try{
            const auto file=CacheBundleStore::Directory(cache,root)/"index.json";CacheBundleIO::Safe(file);if(!std::filesystem::exists(file))return Json::object();
            const auto stamp=snapshot.Key+"|"+std::to_string(std::filesystem::last_write_time(file).time_since_epoch().count())+"|"+std::to_string(std::filesystem::file_size(file));
            if(stamp==previous)return Json::object();return {{"Stamp",stamp},{"Records",CacheBundleStore::Read(cache,root,snapshot,CacheBundleStore::Locale())}};
        }catch(...){return Json::object();}});
    }
    if(Writer.valid()&&Writer.wait_for(std::chrono::seconds(0))==std::future_status::ready){const auto error=Writer.get();if(!error.empty())Status=error;}
    if(SaveWanted&&!Writer.valid()){const auto doc=std::move(*SaveWanted);SaveWanted.reset();const auto folder=HostServices::CacheDirectory()/"journal";
        Writer=std::async(std::launch::async,[doc,folder](){try{const auto text=doc.dump();HelpyIconPack::Publish(folder/"catalog.json",{text.begin(),text.end()});
            auto vanilla=Json::array();for(const auto& row:doc["Entries"])if(row.value("SourceKind",std::string{})=="vanilla"&&!row.value("ModifiedByRuneSchema",false))vanilla.push_back(row);
            auto base=JournalWorkspace::Cache(vanilla,doc.value("SourceKey",std::string{}),false);base["Coverage"]="Observed, unmodified vanilla entries only; not a certified full-game dump.";const auto data=base.dump();HelpyIconPack::Publish(folder/"vanilla.json",{data.begin(),data.end()});return std::string{};}catch(const std::exception& e){return std::string("Journal cache write failed: ")+e.what();}});
    }
    const auto generation=SpawnToolRequests::CurrentGeneration();
    if(Generation!=generation){Generation=generation;Pending=0;Refresh=true;Scanning=false;Cursor=0;BuildingBytes=0;Seen.clear();Building=Json::array();for(auto& row:Entries)row["ObservedCurrent"]=false;}
    Json receipt;
    if(Pending&&SpawnToolRequests::TakeCompleted(Pending,receipt)){
        Pending=0;Next=Clock::now()+std::chrono::milliseconds(80);
        if(!receipt.contains("JournalCatalog")){Scanning=false;Refresh=false;Status=receipt.value("Status",std::string("Native journal read failed; previous cache retained."));}
        else{const auto& doc=receipt["JournalCatalog"];const auto key=doc.value("SourceKey",std::string{});
            if(Cursor&&key!=ScanKey){Cursor=0;BuildingBytes=0;Building=Json::array();Seen.clear();Status="Mounted content changed; restarting the native read without clearing the cache.";}
            else{ScanKey=key;for(auto row:doc.value("Entries",Json::array())){const auto path=row.value("Path",std::string{});const auto bytes=row.dump().size();if(!path.empty()&&Building.size()<8192&&BuildingBytes+bytes<=12*1024*1024&&Seen.insert(path).second){BuildingBytes+=bytes;row["ObservedCurrent"]=true;row["NativeSourceKey"]=key;Building.push_back(std::move(row));}}
                Cursor=doc.value("Cursor",0);if(!doc.value("HasMore",false)){
                    const auto observed=Building.size();
                    for(auto row:Entries){const auto path=row.value("Path",std::string{});const auto bytes=row.dump().size();if(!path.empty()&&Building.size()<8192&&BuildingBytes+bytes<=12*1024*1024&&Seen.insert(path).second){BuildingBytes+=bytes;row["ObservedCurrent"]=false;Building.push_back(std::move(row));}}
                    if(!Building.empty()){Entries=Building;++Revision;Key=ScanKey;SaveWanted=JournalWorkspace::Cache(Entries,Key,true);}
                    Scanning=false;Refresh=false;Status=std::to_string(observed)+" currently loaded native entries observed; "+std::to_string(Entries.size())+" references retained (8192-entry / 12 MiB cap). Full vanilla coverage and discovery/read state are not inferred.";
                }else Status="Cached entries remain usable; native journal read: "+std::to_string(Building.size())+" entries observed.";
            }
        }
    }
    if(Pending&&Clock::now()-Started>std::chrono::seconds(15)){Pending=0;Refresh=false;Scanning=false;Status="Native journal read timed out; previous cache retained. Refresh to retry.";}
    if(Loaded&&Refresh&&!Pending&&Clock::now()>=Next&&SpawnToolRequests::IsAvailable()){
        if(!Scanning){Scanning=true;Cursor=0;BuildingBytes=0;Building=Json::array();Seen.clear();}
        const auto id=++Sequence;if(SpawnToolRequests::TrySubmit({{"Action","JournalCatalog"},{"Cursor",Cursor},{"_RequestId",id},{"_Source","HelpyStandalone"}})){Pending=id;Started=Clock::now();}
    }
}
inline void Shutdown(){if(Reader.valid())Reader.wait();if(BundleReader.valid())BundleReader.wait();if(Writer.valid())Writer.wait();}
}
