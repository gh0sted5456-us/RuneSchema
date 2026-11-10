#include "Core/HelpyNotificationFile.h"
#include "Runtime/HelpyModMetadataWatch.h"
#include <stdexcept>
using namespace PS::HelpyNotifications;
static void Check(bool value){if(!value)throw std::runtime_error("Cache notifications regression");}
int main(){
    Store store;std::vector<Mod> mods{{"siege","Siege","1.0"},{"dye","Dye",""}};
    store.Observe("Game Pass build A",mods,1);Check(store.Notices.empty()&&!store.VanillaPending());
    mods[0].Version="1.1";store.Observe("Game Pass build A",mods,2);Check(store.Unread()==1&&store.Notices[0].Source=="siege");
    store.Read(store.Notices[0].Id);store.Observe("Game Pass build A",mods,3);Check(store.Unread()==0&&store.Notices.size()==1);
    mods[0].Enabled=false;store.Observe("Game Pass build A",mods,4);Check(!store.Notices[0].Active);
    mods[0].Enabled=true;mods[0].Name="Siege renamed folder";store.Observe("Game Pass build A",mods,5);Check(store.Notices[0].Active&&store.Notices.size()==1);
    store.Built("siege","1.0",false);Check(!store.Notices[0].Resolved);
    store.Built("siege","1.1",false);Check(store.Notices[0].Resolved);
    mods[0].Version="1.0";store.Observe("Game Pass build A",mods,6);mods[0].Version="1.1";store.Observe("Game Pass build A",mods,7);Check(store.Unread()==1&&!store.Notices[0].Resolved);
    mods[0].Version="";store.Observe("Game Pass build A",mods,8);Check(store.Unread()==0);
    store.Observe("Game Pass build B",mods,9);Check(store.VanillaPending()&&store.Unread()==1);
    store.Observe("Game Pass build C",mods,10);Check(store.VanillaPending()&&store.Unread()==1);
    store.Built({},"Game Pass build B",true);Check(store.VanillaPending());
    store.Built({},"Game Pass build C",true);Check(!store.VanillaPending());
    store.Result("job-a","siege","Siege",true,"New items and icons cached.",11);store.Result("job-a","siege","Siege",true,"Same result.",12);Check(store.Unread()==1);
    auto document=store.Document();document["Format"]=99;auto restored=Store::Parse(document);Check(restored.Unread()==1&&restored.Versions==store.Versions&&restored.Game==store.Game);
    restored.ReadAll();Check(restored.Unread()==0);Check(Store::Parse(restored.Document()).Unread()==0);
    auto bad=document;bad["Notices"][0]["Action"]="execute";bool refused=false;try{Store::Parse(bad);}catch(...){refused=true;}Check(refused);
    Store releases;
    Check(!releases.ObserveRelease("0.8.1.0","0.8.2.0-pre.20261109",1));
    Check(!releases.ObserveRelease("0.8.1.0","0.8.2.0hf",2));
    Check(!releases.ObserveRelease("0.8.1.0","0.8.0.3",3));
    Check(releases.ObserveRelease("0.8.1.0","v0.8.2.0",4)&&releases.Unread()==1);
    Check(!releases.ObserveRelease("0.8.1.0","0.8.2.0",5)&&releases.Notices.size()==1);
    releases.Dismiss(releases.Notices[0].Id);
    releases=Store::Parse(releases.Document());
    Check(!releases.ObserveRelease("0.8.1.0","v0.8.2.0",6)&&!releases.Unread());
    Check(releases.ObserveRelease("0.8.1.0","1.0.0.J00001",7)&&!releases.Notices[0].Active);
    Check(!releases.ObserveRelease("1.0.0.J00001","",8)&&releases.Notices.back().Resolved&&!releases.Unread());
    releases.DeleteAll();releases=Store::Parse(releases.Document());
    Check(!releases.ObserveRelease("0.8.1.0","1.0.0.J00001",9)&&releases.Notices.empty());
    Store independent;independent.Observe("Steam build A",mods,1);Check(independent.Notices.empty());
    Store tracked;Mod local{"local","Locally named mod","",true,true,std::string(64,'a')};tracked.Observe("",{local},1);Check(tracked.Notices.empty());local.Fingerprint=std::string(64,'b');tracked.Observe("",{local},2);Check(tracked.Unread()==1&&tracked.Notices[0].Version=="files:"+local.Fingerprint);tracked.Built("local","files:"+std::string(64,'a'),false);Check(tracked.Unread()==1);tracked.Built("local","files:"+local.Fingerprint,false);Check(tracked.Unread()==0&&Store::Parse(tracked.Document()).Fingerprints==tracked.Fingerprints);
    restored.Result("newlines","siege","Siege",false,"First line\nSecond line\r\tDetails",12);Check(Store::Parse(restored.Document()).Notices.back().Detail=="First line Second line  Details");
    auto cleared=Store::Parse(tracked.Document());cleared.Observe("build A",{{"siege","Siege","1"}},1);cleared.Observe("build B",{{"siege","Siege","2"}},2);
    cleared.Result("old-job","siege","Siege",true,"Done",3);cleared.Dismiss(cleared.Notices.back().Id);
    const auto versions=cleared.Versions,fingerprints=cleared.Fingerprints;const auto game=cleared.Game;const auto revision=cleared.Revision;
    cleared.DeleteAll();Check(cleared.Notices.empty()&&!cleared.Unread()&&!cleared.VanillaPending()&&cleared.Revision==revision+1);
    cleared=Store::Parse(cleared.Document());cleared.Observe(game,{{"siege","Siege","2"}},4);Check(cleared.Notices.empty()&&cleared.Versions==versions&&cleared.Fingerprints==fingerprints);
    const auto unchanged=cleared.Revision;cleared.DeleteAll();Check(cleared.Revision==unchanged);cleared.Observe("build C",{{"siege","Siege","3"}},5);Check(cleared.Unread()==2);
    for(int i=0;i<2100;++i)independent.Observe("Steam build A",{{"id"+std::to_string(i),"Mod","1"}},12);Check(independent.Versions.size()==2048);Check(Store::Parse(independent.Document()).Versions.size()==2048);
    namespace fs=std::filesystem;const auto root=fs::temp_directory_path()/("helpy-notifications-"+std::to_string(GetCurrentProcessId()));fs::create_directories(root/"mods/Test/cache");
    try{
        File first;first.Path=root/"notifications.json";Check(first.Load().Notices.empty());first.Save(restored);File second;second.Path=first.Path;Check(second.Load().Notices.size()==restored.Notices.size());
        restored.ReadAll();first.Save(restored);bool stale=false;try{second.Save(restored);}catch(...){stale=true;}Check(stale);
        restored.DeleteAll();first.Save(restored);File deleted;deleted.Path=first.Path;Check(deleted.Load().Notices.empty());
        const auto text=PS::ConfigFiles::Read(first.Path);PS::ConfigFiles::Write(first.Path,"broken history");File corrupt;corrupt.Path=first.Path;bool refusedLoad=false;try{corrupt.Load();}catch(...){refusedLoad=true;}Check(refusedLoad&&!corrupt.Ready);Check(PS::ConfigFiles::Read(first.Path)=="broken history");
        PS::HelpyModMetadataWatch watch;Check(watch.Relevant(L"Test/ID.txt")&&watch.Relevant(L"Test/mod.json")&&watch.Relevant(L"runeschema.txt")&&watch.Relevant(L"Test/cache/tips.json")&&watch.Relevant(L"Test/cache/taglines.json")&&watch.Relevant(L"Test/ue4ss/status.txt")&&!watch.Relevant(L"Test/cache/output.json"));watch.Watch(root);
        PS::ConfigFiles::Write(root/"mods/Test/ID.txt","Version=2");bool changed=false;for(int i=0;i<100&&!changed;++i){changed=watch.Changed();if(!changed)Sleep(10);}Check(changed);watch.Close();
    }catch(...){fs::remove_all(root);throw;}fs::remove_all(root);
}
