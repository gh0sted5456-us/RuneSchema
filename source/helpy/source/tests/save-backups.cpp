#include "SaveBackupStore.h"
#include "SaveWorldBackups.h"
#include "Core/HelpyWindowPlacement.h"
#include <cassert>
#include <iostream>
#undef assert
#define assert(value) ((value)?void(0):throw std::runtime_error("line "+std::to_string(__LINE__)+": "+#value))
int main(int argc,char** argv)try{
    using namespace PS::SaveBackups;assert(argc==2);const auto base=fs::absolute(argv[1]).lexically_normal();
    const auto root=base/("save-backups-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
    assert(root.parent_path()==base);fs::create_directories(root);
    struct Cleanup{fs::path Path,Base;~Cleanup(){if(Path.parent_path()==Base){std::error_code error;fs::remove_all(Path,error);}}}cleanup{root,base};
    const std::string guid="1234567890ABCDEF1234567890ABCDEF";
    const auto save=[&](int gold){return Json{{"meta_data",{{"char_guid",guid},{"char_name","Recovery fixture"}}},{"GameProgress",{{"Gold",gold}}}}.dump();};
    const auto old=save(1),current=save(20);const auto directory=Directory(root,"steam");
    const auto rejects=[](const auto& fn){try{fn();}catch(...){return true;}return false;};
    {namespace window=PS::HelpyWindowPlacement;const auto fitted=window::Fit({-5000,-5000,-4600,-4700},{-1920,0,0,1080},760,560);
        assert(fitted.left==-1920&&fitted.top==0&&fitted.right-fitted.left==760&&fitted.bottom-fitted.top==560);
        const auto small=window::Fit({0,0,1800,1200},{0,0,600,400},760,560);assert(small.right==600&&small.bottom==400);
        const auto preferences=root/"window-placement.json";PS::ConfigFiles::Write(preferences,"damaged preferences");window::Load(preferences);assert(!window::Ready&&PS::ConfigFiles::Read(preferences)=="damaged preferences");
        PS::ConfigFiles::Write(preferences,"{\"main\":[0,0,1000,700,96]}");window::Load(preferences);assert(window::Ready&&window::Windows.at("main")[2]==1000);
        const auto handle=CreateWindowExW(0,L"STATIC",L"Helpy placement fixture",WS_OVERLAPPEDWINDOW,100,100,640,400,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);assert(handle);
        struct Destroy{HWND Value;~Destroy(){DestroyWindow(Value);}}destroy{handle};
        window::Remember(handle,"fixture");assert(window::Windows.contains("fixture"));
        assert(SetWindowPos(handle,nullptr,200,200,500,350,SWP_NOACTIVATE|SWP_NOZORDER));window::Restore(handle,"fixture",420,320);RECT restored{};assert(GetWindowRect(handle,&restored));
        assert(restored.right-restored.left==640&&restored.bottom-restored.top==400);
        PS::ConfigFiles::Write(preferences,"{}");assert(SetWindowPos(handle,nullptr,200,200,500,350,SWP_NOACTIVATE|SWP_NOZORDER));window::Remember(handle,"fixture");
        assert(PS::ConfigFiles::Read(preferences)=="{}"&&!window::Status.empty());
    }
    for(const auto* runtime:{"STEAM","EPIC","GAMEPASS","SERVER"})assert(Directory(root,runtime)==Directory(root,Runtime(runtime)));
    assert(Runtime("GaMePaSs")=="gamepass"&&rejects([]{Runtime("../GAMEPASS");})&&rejects([]{Runtime("UNKNOWN");}));
    assert(!Automatic(directory)&&!ExperimentalWorldRecovery(directory));SetAutomatic(directory,true);assert(Automatic(directory));
    SetWorldRecovery(directory,true);assert(Automatic(directory)&&ExperimentalWorldRecovery(directory));
    SetAutomatic(directory,false);assert(!Automatic(directory)&&ExperimentalWorldRecovery(directory));
    SetWorldRecovery(directory,false);SetAutomatic(directory,true);assert(Automatic(directory)&&!ExperimentalWorldRecovery(directory));
    ObserveMod("Fixture","Fixture mod","1.2");const auto file=Store(directory,old,guid,"Before cleanup",{{"Mods",ActiveMods()},{"LastPlayedUtc",Now()}});
    assert(Read(directory,guid,PS::CacheBundleIO::Hash(old))==old);
    assert(Store(directory,old,guid,"Automatic")==file);
    auto rows=List(directory);assert(rows.size()==1&&rows[0].Reasons.size()==2&&rows[0].Mods[0]["Id"]=="Fixture"&&rows[0].Name=="Recovery fixture");
    auto entry=rows[0];assert(rejects([&]{Read(directory,"../escape",entry.Hash);}));
    assert(Context({{"Mods",Json::array({{{"Name",17}}})}}).at("Mods").is_null());
    assert(Context({{"Mods",ActiveMods()}}).at("Mods").size()==1);
    assert(rejects([&]{Store(directory,old,"2234567890ABCDEF1234567890ABCDEF","Manual");}));
    {Lock lock(directory);assert(rejects([&]{Store(directory,current,guid,"Manual");}));}
    const auto native=root/"native/SaveCharacters",target=native/(guid+".json");PS::CacheBundleIO::Publish(target,current);
    const auto unchanged=PS::CacheBundleIO::Hash(current);
    assert(rejects([&]{RestoreFile(directory,entry,native,target,unchanged,[]{return false;});}));
    assert(rejects([&]{RestoreFile(directory,entry,native,target,"changed",[]{return true;});}));
    RestoreFile(directory,entry,native,target,unchanged,[]{return true;});assert(PS::CacheBundleIO::Read(target,MaximumBytes)==old);
    assert(List(directory).size()==2);bool preserved=false;for(const auto& row:List(directory))if(row.Hash==unchanged)preserved=std::find(row.Reasons.begin(),row.Reasons.end(),"Before restore")!=row.Reasons.end();assert(preserved);
    assert(rejects([&]{RestoreFile(directory,entry,root/"other",target,PS::CacheBundleIO::Hash(old),[]{return true;});}));
    assert(rejects([&]{QueueRestore(directory,entry,[]{return false;});}));QueueRestore(directory,entry,[]{return true;});
    assert(rejects([&]{QueueRestore(directory,entry,[]{return true;});}));
    std::string applied;assert(ApplyQueued(directory,current,[&](const auto& bytes){applied=bytes;}));assert(Decode(applied)==Decode(old));
    assert(!ApplyQueued(directory,current,[](const auto&){throw std::runtime_error("must not replay");}));
    QueueRestore(directory,entry,[]{return true;});assert(rejects([&]{ApplyQueued(directory,current,[](const auto&){throw std::runtime_error("native failure");});}));
    assert(!fs::exists(Pending(directory,guid))&&fs::exists(fs::path(Pending(directory,guid).wstring()+L".processing")));
    assert(!ApplyQueued(directory,current,[](const auto&){throw std::runtime_error("must not replay failed application");}));
    assert(rejects([&]{QueueRestore(directory,entry,[]{return true;});}));
    CancelRequest(directory,guid,[]{return true;});assert(!fs::exists(fs::path(Pending(directory,guid).wstring()+L".processing")));
    PS::CacheBundleIO::Publish(file,"corrupt");assert(rejects([&]{Read(directory,entry.Guid,entry.Hash);}));
    assert(rejects([&]{QueueRestore(directory,entry,[]{return true;});}));
    assert(rejects([&]{RestoreFile(directory,entry,native,target,PS::CacheBundleIO::Hash(old),[]{return true;});}));assert(PS::CacheBundleIO::Read(target,MaximumBytes)==old);
    std::string utf16="\xff\xfe";for(unsigned char c:old){utf16.push_back(c);utf16.push_back('\0');}assert(Decode(utf16)==Decode(old));
    assert(rejects([&]{Decode(utf16+"x");}));
    {namespace world=PS::WorldBackups;const auto worlds=world::Directory(root,"steam");
        const auto archive=[](const std::string& payload){std::string bytes="SAVE";for(unsigned i=0;i<4;++i)bytes.push_back(char(payload.size()>>(i*8)));return bytes+payload;};
        const auto older=archive("older whole-world fixture"),newer=archive("current fixture");
        assert(!world::Enabled(worlds));assert(rejects([&]{world::Store(worlds,"WorldFixture",older,"Manual");}));
        std::string backupIssue;
        assert(world::BeforeCleanup(worlds,"WorldFixture",older,Json::object(),backupIssue).empty()&&backupIssue.empty());
        assert(!fs::exists(worlds));
        assert(!world::ApplyQueued(worlds,"WorldFixture",newer,[](const auto&){throw std::runtime_error("disabled recovery must not run");}));
        SetWorldRecovery(directory,true);assert(world::Enabled(worlds));
        const auto file=world::Store(worlds,"WorldFixture",older,"Before cleanup",{{"Mods",ActiveMods()}});auto entries=world::List(worlds);assert(entries.size()==1&&entries[0].Slot=="WorldFixture"&&entries[0].Mods.is_array());
        assert(world::BeforeCleanup(worlds,"WorldFixture",older,Json::object(),backupIssue)==file&&backupIssue.empty());
        const auto entry=entries[0];const auto native=root/"native/SaveGames";const auto target=native/"WorldFixture.sav";PS::CacheBundleIO::Publish(target,newer);
        assert(rejects([&]{world::RestoreFile(worlds,entry,native,PS::CacheBundleIO::Hash(newer),[]{return false;});}));
        world::RestoreFile(worlds,entry,native,PS::CacheBundleIO::Hash(newer),[]{return true;});assert(PS::CacheBundleIO::Read(target,world::MaximumWorldBytes)==older);
        world::QueueRestore(worlds,entry,[]{return true;});std::string loaded;
        SetWorldRecovery(directory,false);
        assert(!world::ApplyQueued(worlds,"WorldFixture",newer,[](const auto&){throw std::runtime_error("disabled queued world restore");}));
        assert(fs::exists(world::Pending(worlds,"WorldFixture"))&&rejects([&]{world::Read(worlds,entry);}));
        SetWorldRecovery(directory,true);
        assert(world::ApplyQueued(worlds,"WorldFixture",newer,[&](const auto& bytes){loaded=bytes;}));assert(loaded==older&&loaded.size()>newer.size());
        assert(!world::ApplyQueued(worlds,"WorldFixture",newer,[](const auto&){throw std::runtime_error("world replay");}));
        assert(rejects([&]{world::Store(worlds,"../escape",older,"Manual");}));assert(rejects([&]{world::Validate("SAVEbad");}));
        PS::CacheBundleIO::Publish(file,archive("changed"));assert(rejects([&]{world::Read(worlds,entry);}));
        assert(world::BeforeCleanup(worlds,"WorldFixture",older,Json::object(),backupIssue).empty()&&!backupIssue.empty());
        assert(PS::CacheBundleIO::Read(file,world::MaximumWorldBytes)==archive("changed"));
        assert(PS::CacheBundleIO::Read(target,world::MaximumWorldBytes)==older);
    }
    std::cout<<"Verified whole-save backups, metadata, identity checks, offline restoration and one-shot native restore claims.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
