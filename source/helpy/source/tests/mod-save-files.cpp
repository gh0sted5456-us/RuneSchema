#include "Core/ModSaveFiles.h"
#include <cassert>
#include <iostream>
using namespace PS::ModSaveFiles;
namespace CacheBundleIO=PS::CacheBundleIO;
template<class F> void Reject(F action){bool failed=false;try{action();}catch(...){failed=true;}assert(failed);}
int main(){
    const auto test=fs::absolute(fs::temp_directory_path()/("mod-save-files-"+std::to_string(GetCurrentProcessId()))).lexically_normal();
    if(!fs::create_directory(test))throw std::runtime_error("Test folder already exists");
    const auto mod=test/"mod",local=test/"LocalAppData",output=local/"RSDragonwilds/RuneSchema/steam/mod-save-backups";
    const Roots roots{{"Mod",mod},{"LocalAppData",local},{"RoamingAppData",test/"Roaming"},{"GameSaved",test/"Saved"}};
    const Json entry={{"Name","JBBB_save"},{"Extension",".sav"},{"Root","GameSaved"},{"Path","SaveGames"}};
    try{
        fs::create_directories(mod);assert(Read(mod)["Files"].empty());
        CacheBundleIO::Publish(mod/"ID.txt","ModID=Example\nSaveFileName=JBBB_save\nSaveFileExtension=.sav\nSaveFileRoot=GameSaved\nSaveFilePath=SaveGames\n[Tips]\nSaveFileName=ignored\n");
        assert(Read(mod)["Files"]==Json::array({entry}));
        CacheBundleIO::Publish(mod/"cache/savefiles.json",Json{{"SchemaVersion",1},{"Files",Json::array({entry})}}.dump());
        assert(Read(mod)["Files"].size()==1);
        for(const auto name:{"../save","a/b","CON","COM1","bad ","","a:s","nul.sav"}){auto row=entry;row["Name"]=name;Reject([&]{Validate({{"SchemaVersion",1},{"Files",Json::array({row})}});});}
        for(const auto path:{"../private","C:/private","/private","a//b","a/../b","a\\..\\b"}){auto row=entry;row["Path"]=path;Reject([&]{Validate({{"SchemaVersion",1},{"Files",Json::array({row})}});});}
        for(const auto extension:{"sav",".*",".sav.exe:",".save/file",""}){auto row=entry;row["Extension"]=extension;Reject([&]{Validate({{"SchemaVersion",1},{"Files",Json::array({row})}});});}
        auto row=entry;row["Root"]="C:/";Reject([&]{Validate({{"SchemaVersion",1},{"Files",Json::array({row})}});});
        Reject([&]{Validate({{"SchemaVersion",1},{"Files",Json::array({entry,entry})}});});
        auto missing=Scan(mod,roots);assert(!missing["Resolved"][0]["Exists"].get<bool>());
        Reject([&]{Backup(mod,roots,output,"Example","1",missing,[]{return true;});});
        const auto source=Resolve(entry,roots);const std::string bytes("opaque\0save\xff",12);CacheBundleIO::Publish(source,bytes);
        auto expected=Scan(mod,roots);Reject([&]{Backup(mod,roots,output,"Example","1",expected,[]{return false;});});
        {const auto writer=CreateFileW(source.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);assert(writer!=INVALID_HANDLE_VALUE);
            Reject([&]{Backup(mod,roots,output,"Example","1",expected,[]{return true;});});CloseHandle(writer);}
        const auto backup=Backup(mod,roots,output,"Example","1",expected,[]{return true;});
        const auto manifest=Json::parse(CacheBundleIO::Read(backup/"manifest.json",65536));assert(manifest["Mod"]=="Example"&&manifest["Version"]=="1"&&manifest["Files"].size()==1);
        const auto copied=backup/manifest["Files"][0]["File"].get<std::string>();assert(CacheBundleIO::Read(copied,100)==bytes&&CacheBundleIO::Read(source,100)==bytes&&manifest["Files"][0]["SHA256"]==CacheBundleIO::Hash(bytes));
        int checks=0;Reject([&]{Backup(mod,roots,output,"Example","1",expected,[&]{return ++checks==1;});});assert(CacheBundleIO::Read(source,100)==bytes);
        CacheBundleIO::Publish(mod/"ID.txt","SaveFileName=other\nSaveFileExtension=.sav\nSaveFileRoot=GameSaved\n");Reject([&]{Backup(mod,roots,output,"Example","1",expected,[]{return true;});});
        assert(fs::is_regular_file(backup/"manifest.json"));
        auto unicode=entry;unicode["Name"]="JBBB_\xc3\xb1";CacheBundleIO::Publish(mod/"ID.txt","ModID=Example");CacheBundleIO::Publish(mod/"cache/savefiles.json",Json{{"SchemaVersion",1},{"Files",Json::array({unicode})}}.dump());
        CacheBundleIO::Publish(Resolve(unicode,roots),bytes);const auto unicodeBackup=Backup(mod,roots,output,"Example","1",Scan(mod,roots),[]{return true;});
        const auto unicodeManifest=Json::parse(CacheBundleIO::Read(unicodeBackup/"manifest.json",65536));assert(unicodeManifest["Files"][0]["Source"].get<std::string>().find("\xc3\xb1")!=std::string::npos);
        assert(CacheBundleIO::Read(unicodeBackup/fs::u8path(unicodeManifest["Files"][0]["File"].get<std::string>()),100)==bytes);
    }catch(...){CacheBundleIO::Safe(test);fs::remove_all(test);throw;}
    CacheBundleIO::Safe(test);fs::remove_all(test);std::cout<<"Mod-owned declarations, paths, writer locks, verified backups and stale-state guards passed.\n";
}
