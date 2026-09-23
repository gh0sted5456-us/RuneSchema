#include "Generator/F2CatalogPlan.h"
#include "Generator/F2ReferenceTree.h"
#include "Generator/F2BundledPaths.h"
#include "Generator/AuthoringPolicy.h"
#include "Generator/QuickMenuDecorations.h"
#include <iostream>
#include <set>
using namespace PS::F2Catalog;
namespace R=PS::F2ReferenceTree;
int checks=0;
void check(bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);}
template<class F>void rejects(F f,const char* why){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,why);}
using Key=std::pair<std::string,bool>;
std::map<Key,R::Tree> fixture() {
    std::map<Key,R::Tree> db;
    const auto put=[&](const char* id,std::vector<R::Node> nodes){db[{id,false}]={id,false,std::move(nodes)};};
    put("main",{{"1.0.0.2","dataset",true}});put("dataset",{{"json","json",true}});
    put("json",{{"RSDragonwilds","game",true}});put("game",{{"Content","content",true},{"Plugins","plugins",true}});
    put("content",{{"Gameplay","gameplay",true},{"Art","do-not-request-art",true}});
    db[{"gameplay",true}]={"gameplay",false,{{"Items/ITEM_A.json","a",false},{"Items/ITEM_A_MeshData.json","m",false},
        {"AI/Wolf/BP_Wolf.json","w",false},{"World/Mining/BP_Stone.json","s",false},
        {"World/Trees/BP_Tree.json","t",false},{"AI/Texture.json","n",false}}};
    put("plugins",{{"GameFeatures","features",true}});
    put("features",{{"DowdunReach","plugin",true},{"EmptyFeature","empty",true}});
    put("plugin",{{"Content","plugin-content",true}});put("empty",{{"Config","unused",true}});
    put("plugin-content",{{"Gameplay","plugin-gameplay",true},{"Art","never-art",true}});
    db[{"plugin-gameplay",true}]={"plugin-gameplay",false,{{"World/Trees/Maple/BP_FellableTree_Maple.json","p",false},
        {"Equipment/ITEM_Cape.json","c",false},{"AI/DA_Data.json","d",false}}};
    return db;
}
int main(){try{
    Sources sources;
    for(const auto& root:ResourceRoots())check(ValidRoot(root),"default resource root accepted");
    for(const auto* p:{"C:\\Game","/Game/../Other","/Game/","/Game//Test","/Script/Dominion","/Engine","/Temp/A","/Memory","/Game/Bad Path"})check(!ValidRoot(p),"unsafe root rejected");
    check(Under("/Game/Gameplay/World/Trees/Oak.A","/game/gameplay/world/trees"),"root match insensitive");
    check(!Under("/Game/Gameplay/World/TreesBackup/A.A","/Game/Gameplay/World/Trees"),"root boundary enforced");
    for(const auto* p:{"/Game/A.A","/DowdunReach/Gameplay/World/BP_Maple.BP_Maple_C"})check(ValidObjectPath(p),"valid reference accepted");
    for(const auto* p:{"/Game/A","/Game/A.A\n","/Game/A.1.2","http://somewhere/A.A","/Game/A.A:Subobject","/Script/X.X"})check(!ValidObjectPath(p),"bad reference rejected");
    check(!SimpleToken("../../x")&&!SimpleToken("main?bad=1")&&!SimpleToken("a/b"),"public route components bounded");
    check(PS::Authoring::SoftDeleteName("bSoftDeleted"),"canonical exported soft-delete recognized");
    check(PS::QuickDecorations::MissingDisplayName("<MISSING STRING TABLE ENTRY>"),"missing localized string detected");
    check(PS::QuickDecorations::ReadableAssetName("/Game/Cap/ITEM_Cape_Attack.ITEM_Cape_Attack")=="Cape Attack","readable path fallback");
    auto item=FromExport("Content/Gameplay/Character/Player/Equipment/ITEM_Helm.json",sources);
    check(item&&item->kind==Kind::Item&&item->path=="/Game/Gameplay/Character/Player/Equipment/ITEM_Helm.ITEM_Helm","item export maps exact mount/object");
    auto node=FromExport("Plugins/GameFeatures/DowdunReach/Content/Gameplay/World/Trees/BP_Maple.json",sources);
    check(node&&node->kind==Kind::Resource&&node->path=="/DowdunReach/Gameplay/World/Trees/BP_Maple.BP_Maple_C","plugin mount not rewritten under Game");
    check(!FromExport("Content/Gameplay/Items/ITEM_Helm_MeshData_Male.json",sources),"mesh data not an inventory item");
    check(!FromExport("Content/Gameplay/World/Weather/BP_Rain.json",sources),"outside configured roots excluded");
    sources.resources.push_back("/Game/Gameplay/World/Weather");check(FromExport("Content/Gameplay/World/Weather/BP_Rain.json",sources).has_value(),"user-added root included as candidate only");
    check(!FromExport("Engine/Content/ITEM_Fake.json",sources),"engine namespace excluded");
    check(!FromExport("Plugins/GameFeatures/DowdunReach/Elsewhere/ITEM_Fake.json",sources),"invalid plugin hierarchy rejected");
    Plan p;
    for(int i=0;i<200;++i)check(p.Add({"/Game/I/ITEM_"+std::to_string(i)+".ITEM_"+std::to_string(i),"Item "+std::to_string(i),Kind::Item}),"candidate inserted");
    check(!p.Add({"/game/i/item_0.item_0","renamed",Kind::Resource}),"case variants deduplicated");
    p.Prioritize("Item 199");auto first=p.Pop();check(first&&first->name=="Item 199"&&p.Done()==1,"search loads matching unseen candidate first");
    std::set<std::string> seen{first->path};while(auto next=p.Pop())check(seen.insert(next->path).second,"no double processing after priority");
    check(p.Empty()&&p.Done()==200&&p.Total()==200,"known total checks all entries");
    p.Add({"/Game/New.N","New",Kind::Item});check(!p.Empty()&&p.Total()==201,"late public results extend plan");
    check(p.Pop()->path=="/Game/New.N"&&p.Empty(),"late result resolved once");
    Plan cleared;cleared.Add({"/Game/A.A","first",Kind::Item});cleared.Add({"/Game/B.B","second",Kind::Item});
    cleared.Prioritize("second");cleared.Prioritize("");check(cleared.Pop()->name=="first","clearing search discards old load priorities");
    const auto longPath="/Game/"+std::string(200,'a')+"/ITEM_Long.ITEM_Long";
    cleared.Add({longPath,"long",Kind::Item});cleared.Prioritize(longPath);check(cleared.Pop()->path==longPath,"long exact paths prioritized");
    Plan cap;for(std::size_t i=0;i<MaxEntries;++i)cap.Add({"/Game/Limit/A"+std::to_string(i)+".A","A",Kind::ClassCandidate});
    rejects([&]{cap.Add({"/Game/Limit/Overflow.A","A",Kind::ClassCandidate});},"candidate cap rejects overflow");
    check(!cap.Add(cap.entries[0]),"duplicate at limit does not throw");
    std::set<std::string> seeds;for(const auto& entry:BundledPaths()){check(ValidObjectPath(entry.path),"bundled path valid");check(seeds.insert(entry.path).second,"bundled paths distinct");}
    auto db=fixture();std::set<Key> requested;
    R::Fetch fetch=[&](const std::string& id,bool recursive){requested.insert({id,recursive});return db.at({id,recursive});};
    std::string revision;auto rows=R::Collect(fetch,Sources{},revision);
    check(rows.size()==6&&revision=="main","reference tree produces six candidates");
    check(!requested.contains({"do-not-request-art",true})&&!requested.contains({"never-art",true}),"art trees not downloaded");
    check(std::any_of(rows.begin(),rows.end(),[](const auto& row){return row.path.starts_with("/DowdunReach/");}),"plugin candidates preserved");
    db[{"gameplay",true}].truncated=true;db[{"gameplay",true}].nodes={{"Items/ITEM_FakePartial.json","bad",false}};
    db[{"gameplay",false}]={"gameplay",false,{{"Items","items",true},{"AI","ai",true}}};
    db[{"items",true}]={"items",false,{{"ITEM_A.json","a",false}}};db[{"ai",true}]={"ai",false,{{"BP_Wolf.json","w",false}}};
    rows=R::Collect(fetch,Sources{},revision);check(rows.size()==4,"truncated recursive response split into shallow subtrees");
    check(std::none_of(rows.begin(),rows.end(),[](const auto& row){return row.path.find("FakePartial")!=row.path.npos;}),"partial recursive results discarded");
    check(requested.contains({"gameplay",false})&&requested.contains({"items",true}),"fallback traverses missing subtrees");
    const auto retained=rows;db.erase({"ai",true});
    rejects([&]{rows=R::Collect(fetch,Sources{},revision);},"failed reference traversal throws");check(rows==retained,"failed traversal does not replace caller's prior index");
    db=fixture();db[{"main",false}].truncated=true;rejects([&]{R::Collect(fetch,Sources{},revision);},"truncated root rejected");
    db=fixture();db[{"plugin",false}].nodes.clear();check(R::Collect(fetch,Sources{},revision).size()==4,"feature without Content safely skipped");
    db=fixture();db[{"gameplay",true}]={"gameplay",true,{}};db[{"gameplay",false}]={"gameplay",false,{{"Loop","gameplay",true}}};
    rejects([&]{R::Collect(fetch,Sources{},revision);},"cyclic/unbounded tree nesting stopped");
    std::cout<<"PASS: "<<checks<<" path-plan, reference traversal, limit and policy assertions. HTTP/Unreal mocked or absent.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
