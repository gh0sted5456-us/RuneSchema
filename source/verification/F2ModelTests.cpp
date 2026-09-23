#include "Generator/QuickMenuUI.h"
#include "Generator/QuickMenuCatalogRules.h"
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <functional>
using namespace PS::QuickUI;
static int tests=0;
void check(bool condition,const char* label){++tests;if(!condition)throw std::runtime_error(label);}
void rejects(const std::function<void()>& f,const char* label){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected,label);}
Entry item(int n,bool cooked=true,const std::string& cls="/Script/Dominion.WearableEquipmentData"){
    Entry e;e.id="/Game/Items/Item"+std::to_string(n)+".Item"+std::to_string(n);e.path=e.id;
    e.name=n==0?"Chef's Hat":n==1?"Bronze Helmet":"Test Equipment Item "+std::to_string(n);
    e.icon="/Game/Icons/Icon"+std::to_string(n)+".Icon"+std::to_string(n);e.power=n%6;e.cooked=cooked;e.cloneEligible=cooked;e.assetClass=cls;return e;
}
Model fixture(){Model m;Catalog c;c.authority=true;c.players.push_back({"host","Local player",true});
    for(int i=0;i<41;++i)c.entries[0].push_back(item(i,i!=3,i==4?"/Script/Dominion.ItemData":"/Script/Dominion.WearableEquipmentData"));
    for(int t=1;t<=2;++t)for(int i=0;i<103;++i){auto e=item(i);e.id=(t==1?"ai:":"resource:")+std::to_string(i);e.path="/Game/Arbitrary/Variant"+std::to_string(i)+".Variant"+std::to_string(i)+"_C";e.name=(t==1?"AI variant ":"Resource node ")+std::to_string(i);if(t==2)e.resourceFamily="Tree";c.entries[t].push_back(e);}
    for(int i=0;i<33;++i){auto e=item(i);e.assetClass="WearableEquipmentMeshData";c.visuals.push_back(e);}
    for(int i=0;i<31;++i)c.issues.push_back({"/Game/Missing/Asset"+std::to_string(i),"Class could not be resolved"});
    m.Update(c);return m;
}
void inspect(Model& m){std::vector<Model::CloneField> fields={{"Weight","float","2.0","",true,true,{}},{"PowerLevel","int","1","",true,true,{}},{"MaleMeshData","WearableEquipmentMeshData*","\"/Game/Meshes/Male.Male\"","",true,true,"WearableEquipmentMeshData"},{"FemaleMeshData","WearableEquipmentMeshData*","\"/Game/Meshes/Female.Female\"","",true,true,"WearableEquipmentMeshData"},{"PersistenceID","FString","\"old\"","managed",false,true,{}}};
    m.AcceptCloneSource(m.catalog.entries[0][0].path,std::move(fields),"IsSoftDeleted");m.cloneTab=true;m.cloneEditorOpen=true;
}
void bounds(Model& m,const char* label){const auto f=m.Render();check(!f.draws.empty(),label);for(const auto& d:f.draws){if(d.kind!=Draw::Kind::Text)check(d.box.x>=0&&d.box.y>=0&&d.box.x+d.box.w<=Width+.01f&&d.box.y+d.box.h<=Height+.01f,"draw bounds");else check(d.font>=15,"readable minimum text size");}
    for(const auto& h:f.hits)check(h.box.x>=0&&h.box.y>=0&&h.box.x+h.box.w<=Width&&h.box.y+h.box.h<=Height,"hit bounds");}
void dump(Model& m,const std::string& path){std::ofstream out(path);for(const auto& d:m.Render().draws){out<<int(d.kind)<<'|'<<d.box.x<<'|'<<d.box.y<<'|'<<d.box.w<<'|'<<d.box.h<<'|'<<d.font<<'|'<<d.centreX;for(auto c:d.color)out<<'|'<<c;out<<'|';for(unsigned char c:d.text){static constexpr char hex[]="0123456789abcdef";out<<hex[c>>4]<<hex[c&15];}out<<'\n';}}
int main(){try{
    PS::Authoring::PermanentAsset=false;PS::Authoring::PermanentSpawn=false;PS::Authoring::EnemyPower=-1;
    check(Width<920,"narrower than v5");check(Columns==4,"four placard columns");
    std::set<std::string> ids;std::ofstream idFile("ids.txt");
    for(int i=0;i<10000;++i){const auto id=PS::ClonePresentation::NewId("Expanded Loot");check(PS::ClonePresentation::ValidId(id,"Expanded Loot"),"valid id");check(ids.insert(id).second,"unique generated id");idFile<<id<<'\n';}
    check(PS::ClonePresentation::Prefix("!!!")=="RS7Mod","empty tag fallback");
    check(PS::ClonePresentation::ModFragment("Expanded Loot")=="Expand","mod partial length");
    check(!PS::ClonePresentation::ValidId("RS7Expand1234567890123","Expanded Loot"),"bad padding rejected");
    check(!PS::ClonePresentation::VisualField("Icon")&&!PS::ClonePresentation::VisualField("PowerLevel")&&!PS::ClonePresentation::VisualField("PersistenceID"),"nonvisual fields excluded");
    check(PS::ClonePresentation::VisualField("MaleMeshData")&&PS::ClonePresentation::VisualField("FemaleMeshData"),"wearable visual fields recognized");
    check(PS::ClonePresentation::VisualField("HeldEquipmentActorClass"),"equipped held appearance not only dropped mesh");
    check(PS::ClonePresentation::CompatibleAppearance("HeldEquipmentData","HeldEquipmentData","held:sword","held:sword"),"same held family allowed");
    check(!PS::ClonePresentation::CompatibleAppearance("HeldEquipmentData","HeldEquipmentData","held:sword","held:bow"),"cross held family blocked");
    check(!PS::ClonePresentation::CompatibleAppearance("HeldEquipmentData","HeldEquipmentData","held:unresolved","held:unresolved"),"unknown held family blocked");
    check(PS::QuickCatalogRules::BlueprintCandidate("CustomWolf","/Script/CoreUObject.Class"),"non BP class included");
    check(PS::QuickCatalogRules::BlueprintCandidate("CustomTree_C","Class"),"generated class included");
    check(PS::QuickCatalogRules::BlueprintCandidate("Mystery",""),"missing metadata not omitted");
    check(PS::QuickCatalogRules::ResourceAncestry("CustomGatherableComponent"),"gatherable node family");
    check(!PS::QuickCatalogRules::BlueprintCandidate("Icon","Texture2D"),"known texture not loaded as class");
    auto m=fixture();bounds(m,"items layout");dump(m,"items.draws");
    m.Activate("scroll-end");check(m.scroll[0]==3,"all 41 items paged");m.Wheel(1);check(m.scroll[0]==3,"mouse wheel cannot change item pages");m.Page(1);check(m.scroll[0]==2,"explicit page control moves one page");
    m.tab=Tab::Enemies;m.Activate("node-subtab","1");m.Activate("scroll-end");check(m.scroll[1]==11,"all AI rows paged");m.tab=Tab::Resources;m.Activate("scroll-end");check(m.scroll[2]==11,"all resource rows paged");
    m.filters[2]="102";check(m.Filtered(2).size()==1,"last resource searchable");m.filters[2].clear();m.ClampScroll();bounds(m,"resource layout");
    m.tab=Tab::Items;inspect(m);const auto id=m.cloneId;bounds(m,"clone layout");dump(m,"clone.draws");for(int i=0;i<50;++i)m.Render();check(m.cloneId==id,"ID stable across redraws");
    m.cloneFieldPicker=true;check(m.CloneMatches().size()==4,"managed identity hidden from property chooser");m.cloneFieldPicker=false;check(m.CloneMatches().empty(),"only overrides in main property list");
    m.cloneSourcePicker=true;check(m.LootMatches().size()==40,"runtime clones excluded from source picker");m.cloneSourcePicker=false;m.cloneAppearancePicker=true;check(m.LootMatches().size()==39,"donor picker filters item class and cooked provenance");bounds(m,"appearance picker");dump(m,"picker.draws");m.Back();check(!m.closeRequested&&!m.cloneAppearancePicker,"picker back returns to clone");
    m.cloneAcknowledged=true;auto command=m.CloneCommand();check(command.persistenceId==id&&command.modTag=="RuneSchema","request carries preview identity");check(command.iconPath==m.cloneSourceInfo.icon,"auto icon inherits source preview");
    m.appearanceMode=Model::Appearance::Copy;rejects([&]{m.CloneCommand();},"missing donor blocks create");m.cloneAppearance=m.catalog.entries[0][1].path;m.cloneAppearanceInfo=m.catalog.entries[0][1];m.cloneAppearanceReady=true;m.cloneIcon="/Game/Icons/Custom.Custom";m.iconMode=Model::IconMode::Override;command=m.CloneCommand();check(command.appearanceSource==m.cloneAppearance&&command.iconPath==m.cloneIcon,"appearance and icon stay independent");dump(m,"copy.draws");
    m.appearanceMode=Model::Appearance::Mesh;rejects([&]{m.CloneCommand();},"missing mesh blocks create");m.cloneMesh=m.catalog.visuals[0].path;command=m.CloneCommand();check(command.meshField=="MaleMeshData"&&command.appearanceSource.empty(),"direct mesh request uses selected field only");
    m.cloneMeshPicker=true;bounds(m,"mesh picker");m.cloneMeshFieldPicker=true;bounds(m,"mesh slot popup");m.Back();check(m.cloneMeshPicker&&!m.cloneMeshFieldPicker,"nested back closes slot popup first");m.Back();check(!m.cloneMeshPicker&&!m.closeRequested,"mesh back retains main clone screen");
    m.cloneAdvancedOpen=true;bounds(m,"advanced settings");dump(m,"advanced.draws");m.Activate("clone-field","Weight");bounds(m,"JSON field");m.cloneValue="3.5";m.Activate("clone-apply");check(m.cloneEdits["Weight"]=="3.5","property override retained");m.Back();check(!m.cloneAdvancedOpen&&!m.closeRequested,"advanced back");
    m.cloneCreated=true;rejects([&]{m.CloneCommand();},"duplicate creation blocked");m.Activate("clone-new-id");check(m.cloneId!=id&&!m.cloneCreated&&!m.cloneAcknowledged,"new clone ID is explicit");
    const auto before=m.cloneId;m.Focus("clone-mod");m.Key('A',true);m.Character('M');m.Character('y');check(m.cloneId!=before&&m.cloneId.starts_with("RS7My"),"mod tag refreshes ID prefix");
    m.coverageOpen=true;bounds(m,"coverage");m.Activate("scroll-end");check(m.coverageScroll==4,"every retained issue reachable");m.Back();
    m.tab=Tab::Enemies;m.node=m.catalog.entries[1][0];bounds(m,"AI dialog");dump(m,"node.draws");PS::Authoring::EnemyPower=3;bounds(m,"AI power override");
    m.node->available=false;m.node->detail="Abstract base class";rejects([&]{m.SpawnCommand();},"browse only class cannot spawn");m.Back();
    m.playerPicker=true;bounds(m,"player picker");m.Back();m.ShowActionResult("Action stopped","A clear failure message.",false);bounds(m,"result modal");m.Back();
    m.cloneModeOpen=true;bounds(m,"mode menu");m.Back();
    auto blocked=fixture();inspect(blocked);blocked.cloneSoftDelete.clear();blocked.cloneAcknowledged=true;
    rejects([&]{blocked.CloneCommand();},"temporary source without soft-delete blocked");
    bool tempLabel=false,createHit=false;
    for(const auto& d:blocked.Render().draws)if(d.text=="Temp blocked")tempLabel=true;
    for(const auto& h:blocked.Render().hits)if(h.action=="create-clone")createHit=true;
    check(tempLabel&&!createHit,"temporary restriction visible and enforced");
    PS::Authoring::PermanentAsset=true;check(blocked.CloneCommand().permanent,"permanent path still available");PS::Authoring::PermanentAsset=false;
    blocked.cloneEdits["Name"]="\"Old name\"";blocked.Focus("clone-name");blocked.Character('X');check(!blocked.cloneEdits.contains("Name"),"basic name edit supersedes advanced name");
    blocked.cloneEdits["Icon"]="\"/Game/Icons/Old.Old\"";blocked.Focus("clone-icon");blocked.Character('X');check(!blocked.cloneEdits.contains("Icon"),"basic icon edit supersedes advanced icon");
    auto held=fixture();inspect(held);held.cloneAppearancePicker=true;held.cloneSourceInfo.appearanceGroup="held:sword";
    for(auto& e:held.catalog.entries[0])e.appearanceGroup="held:bow";
    held.catalog.entries[0][1].appearanceGroup="held:sword";
    check(held.LootMatches().size()==1,"donor picker enforces held category");
    auto pending=fixture();inspect(pending);pending.Activate("source-clone",pending.catalog.entries[0][1].path);
    check(!pending.cloneReady&&pending.cloneId.empty()&&pending.cloneSourceInfo.name=="Bronze Helmet","pending source selection has no stale identity or card");
    std::cout<<"PASS: "<<tests<<" assertions; generated 10000 identifiers; all UI modal bounds checked.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<tests<<" assertions: "<<e.what()<<'\n';return 1;}}
