#include "Core/HelpyHotfixInfo.h"
#include "Core/HelpyNotifications.h"
#include <cassert>
using namespace PS;
int main(){
    using namespace HelpyHotfixInfo;
    assert(Tag("0.8.1.0hf","0.8.1.0")&&Tag("v0.8.1.0-hf","0.8.1.0"));
    for(const auto* tag:{"0.8.1.0","0.8.1.0hf1","0.8.2.0hf","0.8.1.0hf+extra","0.8hf"})assert(!Tag(tag,"0.8.1.0"));
    assert(Tag("v1.0.0.J00001-hf","1.0.0.J00001"));
    assert(!Tag("1.0.0.J00002hf","1.0.0.J00001")&&!Tag("1.0.0.0hf","1.0.0.J00001"));
    const std::string tag="0.8.1.0hf",name="RuneSchema-"+tag+"-Win64-hotfix.zip";
    Json asset={{"id",2},{"state","uploaded"},{"size",100},{"name",name},{"digest","sha256:"+std::string(64,'a')},{"browser_download_url","https://github.com/gh0sted5456-us/RuneSchema/releases/download/"+tag+"/"+name}};
    Json release={{"id",1},{"draft",false},{"prerelease",true},{"tag_name",tag},{"published_at","2026-11-03T00:00:00Z"},{"assets",Json::array({asset})}};
    auto found=Select(Json::array({release}),"0.8.1.0");assert(!found.is_null()&&ForPlatform(found,"Win64")==asset&&ForPlatform(found,"WinGDK").is_null());
    const auto id=Identity(found,asset);assert(id=="hotfix:1:2:sha256:"+std::string(64,'a'));
    auto bad=release;bad["draft"]=true;assert(Select(Json::array({bad}),"0.8.1.0").is_null());
    for(const auto* field:{"digest","browser_download_url","state"}){bad=release;bad["assets"][0][field]="invalid";assert(Select(Json::array({bad}),"0.8.1.0").is_null());}
    bad=release;bad["assets"].push_back(asset);assert(ForPlatform(Select(Json::array({bad}),"0.8.1.0"),"Win64").is_null());
    auto newer=release;newer["id"]=3;newer["published_at"]="2026-11-04T00:00:00Z";
    assert(Select(Json::array({newer,release}),"0.8.1.0")["id"]==3);
    HelpyNotifications::Store store;store.Hotfix(id,tag,1);store.Hotfix(id,tag,2);assert(store.Unread()==1&&store.Notices.size()==1);
    auto roundtrip=HelpyNotifications::Store::Parse(store.Document());assert(roundtrip.Notices[0].Action=="hotfix");
    store.AppliedHotfix(id);assert(!store.Unread()&&store.Notices[0].Resolved);
    store.Hotfix("hotfix:new",tag,3);assert(!store.Notices[0].Active&&store.Unread()==1);
}
