#include "Core/HelpyCacheOwnership.h"
#include <cassert>
int main(){
    using namespace PS;using namespace HelpyCacheOwnership;
    HelpySourceCache::Snapshot snapshot;snapshot.Authoritative=true;snapshot.Key="0123456789abcdef";snapshot.ContentRevision=7;
    snapshot.SourceKeys={{"shared","1111111111111111"},{"game","2222222222222222"}};
    auto declared=std::make_shared<Owners>(),authored=std::make_shared<Owners>();
    for(int i=0;i<300;++i)(*declared)["/shared/item"+std::to_string(i)]={"dye"};
    (*declared)["/shared/conflict"]={"dye","spears"};(*authored)["/shared/conflict"]={"third"};
    (*authored)["/shared/art"]={"dye"};(*declared)["/shared/other"]={"spears"};(*declared)["/unregistered/a"]={"dye"};
    snapshot.AssetOwners=declared;snapshot.AuthoredAssets=authored;
    size_t calls=0;Read read=[&](const Json& request,Json& reply){++calls;reply=Page(snapshot,request);return true;};
    const auto result=Fetch(snapshot,"mods/dye",read);
    assert(calls==3&&result.AssetOwners->size()==302&&!result.AuthoredAssets);
    assert(result.AssetOwners->at("/shared/conflict")==std::set<std::string>({"dye","spears","third"}));
    assert(result.AssetOwners->at("/shared/art").contains("dye"));
    assert(!result.AssetOwners->contains("/shared/other")&&!result.AssetOwners->contains("/unregistered/a"));
    const auto all=Fetch(snapshot,{},read);assert(all.AssetOwners->size()==303);
    const auto reject=[&](Read provider){bool failed=false;try{Fetch(snapshot,"mods/dye",provider);}catch(const std::exception&){failed=true;}assert(failed);};
    reject([](const Json&,Json&){return false;});
    reject([&](const Json& q,Json& r){r=Page(snapshot,q);r["ContentRevision"]=8;return true;});
    reject([&](const Json& q,Json& r){r=Page(snapshot,q);r["Paths"]["/shared/other"]={"spears"};return true;});
    reject([&](const Json& q,Json& r){r=Page(snapshot,q);r["Next"]=q["After"];r["Complete"]=false;return true;});
    reject([&](const Json& q,Json& r){auto changed=snapshot;changed.Key="1111111111111111";r=Page(changed,q);return true;});
    // Page boundaries preserve a cursor even if a mod owns no entries in a scan.
    Owners sparse;for(int i=0;i<3000;++i)sparse["/shared/other"+std::to_string(i)]={"spears"};
    snapshot.AssetOwners=std::make_shared<Owners>(sparse);snapshot.AuthoredAssets.reset();
    const auto none=Fetch(snapshot,"mods/dye",read);assert(none.AssetOwners->empty());
}
