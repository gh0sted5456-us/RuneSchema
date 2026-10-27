#define NOMINMAX
#include "Core/LearnedItemCatalog.h"
#include "Core/CacheBundleItems.h"
#include <cassert>
#include <iostream>
using namespace PS;using Json=nlohmann::json;namespace fs=std::filesystem;
int main(){
    const auto root=fs::temp_directory_path()/("rs-learned-items-"+std::to_string(GetCurrentProcessId()));
    const auto cache=root/"steam"/"cache"/"item-icons";
    HelpySourceCache::Snapshot active;active.Authoritative=true;active.Key="test";active.SourceKeys["example"]="content-v1";
    active.Owners["example"]={"test-mod"};active.DeclaredOwners=active.Owners;
    Json row={{"Path","/Example/Items/ITEM_Cannon.ITEM_Cannon"},{"Name","Cannon"},{"SourceKind","modded"},{"SourceName","test-mod"}};
    {LearnedItemCatalog::Store writer;writer.Observe(cache,active,"en-US",Json::array({row}));
     auto second=row;second["Path"]="/Example/Items/ITEM_Cannonball.ITEM_Cannonball";second["Name"]="Cannonball";
     writer.Observe(cache,active,"en-US",Json::array({second}));writer.Shutdown();assert(writer.Error.empty());}
    const auto saved=CacheBundleStore::Read(cache,root,active,"en-US","items");assert(saved.size()==2);
    assert(CacheBundleStore::Read(cache,root/"renamed-install",active,"en-US","items").size()==2);
    auto updated=active;updated.SourceKeys["example"]="content-v2";assert(CacheBundleStore::Read(cache,root,updated,"en-US","items").size()==2);
    const auto presentation=HelpyPresentationCache::Directory(cache)/"index.json";
    CacheBundleIO::Publish(presentation,"{}"); // An existing downloaded index must not hide later learned routes.
    const auto native=cache.parent_path()/"item-catalogues"/"native-items.json";
    std::vector<std::shared_ptr<const Json>> oldRows;
    for(int i=0;i<17000;++i)oldRows.push_back(std::make_shared<const Json>(Json{{"Name","Stone"+std::to_string(i)},{"Path","/Example/Stone"+std::to_string(i)+".Stone"}}));
    PagedItemCatalog::Save(native,Json::array({"item-browse-v2","older",Json::array(),"en-US"}).dump(),oldRows);
    CacheBundleItems::Store reader;
    const auto lookup=[&]{std::optional<Json> page;for(int i=0;i<4000;++i){page=reader.Lookup(cache,root,active,"en-US","trebuchet","",28);if(page&&!page->at("Rows").empty())break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}return page;};
    for(int i=0;i<100;++i){reader.Lookup(cache,root,active,"en-US","trebuchet","",28);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    auto found=row;found["Path"]="/Example/Items/ITEM_Trebuchet.ITEM_Trebuchet";found["Name"]="Trebuchet";
    {LearnedItemCatalog::Store writer;writer.Observe(cache,active,"en-US",Json::array({found}));writer.Shutdown();}
    const auto refreshed=lookup();assert(refreshed&&refreshed->at("Rows").size()==1&&refreshed->value("HasMore",false));
    assert(refreshed->at("NextCursor").get<std::string>().starts_with("P:"));reader.Shutdown();
    assert(!fs::exists(cache.parent_path()/"item-catalogues"/"native-items.json")); // Partial knowledge never certifies a complete catalogue.
    auto disabled=active;disabled.Owners.clear();disabled.DeclaredOwners.clear();assert(CacheBundleStore::Read(cache,root,disabled,"en-US","items").empty());
    auto invalid=row;invalid["Path"]="/Example/Items/Unknown.Unknown";invalid["SourceName"]="disabled-mod";
    {LearnedItemCatalog::Store writer;writer.Observe(cache,active,"en-US",Json::array({invalid}));writer.Shutdown();}
    assert(CacheBundleStore::Read(cache,root,active,"en-US","items").size()==3);
    fs::remove_all(root);std::cout<<"Targeted observations survive restart, merge without a full manifest, and respect enabled ownership\n";
}
