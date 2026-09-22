#include "Generator/QuickMenuSpawnRequest.h"
#include "Generator/QuickMenuCatalogRules.h"
#include <cstdlib>
#include <iostream>
#include <variant>
using namespace PS::QuickUI;
namespace Rules=PS::QuickCatalogRules;
static int checks=0;
void Check(bool value,const char* why){++checks;if(!value){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);}}
template<class Fn>void Reject(Fn fn,const char* why){bool threw=false;try{fn();}catch(const std::exception&){threw=true;}Check(threw,why);}
Catalog Fixture(){
    Catalog c;c.authority=true;c.players={{"/World.PC1","Host",true},{"/World.PC2","Guest",false}};
    for(int i=0;i<120;++i){auto path="/Game/Items/ITEM_Test"+std::to_string(i)+".ITEM_Test"+std::to_string(i);
        c.entries[0].push_back({path,"Test item "+std::to_string(i),path,"/Game/Icons/T_Test.T_Test","rune"});}
    for(int i=0;i<127;++i){auto path="/Game/AI/BP_Test"+std::to_string(i)+".BP_Test"+std::to_string(i)+"_C";
        Entry entry{"@loaded-ai:"+path,"AI variant "+std::to_string(i),path,{},"ai"};entry.nodeKind="AI";c.entries[1].push_back(std::move(entry));}
    const std::string path="/Game/Gameplay/World/Mining/BP_OreNode_Stone.BP_OreNode_Stone_C";
    Entry resource{"@loaded-resource:"+path,"Stone node",path,{},"stone mining ore"};resource.nodeKind="Resource";resource.resourceFamily="Mineral";c.entries[2].push_back(std::move(resource));return c;
}
Hit Control(Model& m,const std::string& action,const std::string& arg={}){
    for(const auto& h:m.Render().hits)if(h.action==action&&(arg.empty()||h.arg==arg))return h;
    Check(false,("missing control "+action).c_str());return {};
}
void Click(Model& m,const std::string& action,const std::string& arg={}){
    const auto h=Control(m,action,arg);m.Click(m.Render(),h.box.x+h.box.w*.5f,h.box.y+h.box.h*.5f);
}
using Value=std::variant<std::string,bool,int,double>;
struct Packet{std::map<std::string,Value> values;std::vector<Loot> drops;};
Packet Encode(const Command& c){Packet p;EncodeSpawn(c,[&](const char* key,const auto& v){p.values[key]=v;},[&](const Loot& l){p.drops.push_back(l);});return p;}
std::string Text(const Packet& p,const char* key){return std::get<std::string>(p.values.at(key));}
void CheckGeometry(Model& m){const auto frame=m.Render();for(const auto* hits:{&frame.hits,&frame.rightHits})for(const auto& h:*hits){Check(h.box.x>=0&&h.box.y>=0,"no negative hit position");Check(h.box.x+h.box.w<=Width&&h.box.y+h.box.h<=Height,"all hit bounds inside compact panel");}}
int main(){
    Check(Width==680&&Height==720,"compact logical panel");Model m;m.Update(Fixture());CheckGeometry(m);
    auto f=m.Render();int tiles=0;bool gold=false,ash=false;
    for(const auto& h:f.hits)if(h.action=="toggle-item")++tiles;
    for(const auto& d:f.draws)if(d.kind==Draw::Kind::Rectangle){
        gold|=d.color[0]>.4f&&d.color[0]>d.color[1]&&d.color[1]>d.color[2];
        ash|=d.color[0]<.1f&&d.color[1]<.1f&&d.color[2]<.1f&&d.color[3]>.9f;
    }
    Check(tiles==12,"four-across item grid retained");Check(gold&&ash,"gold frame and opaque ash-black fill");
    Click(m,"tab","1");Click(m,"node-subtab","1");m.Activate("scroll-end");Check(m.scroll[1]==14,"last page of every discovered AI is reachable");
    Click(m,"node",m.catalog.entries[1].back().id);CheckGeometry(m);
    m.name="Patrol leader";m.scale="1.6";m.count="3";m.effect=Effect::Ghost;m.recipient="/World.PC2";
    Click(m,"loot-picker");f=m.Render();bool pickerIcon=false;
    for(const auto& h:f.hits)if(h.action=="add-loot")for(const auto& d:f.draws)
        if(d.kind==Draw::Kind::Icon&&d.box.y>=h.box.y&&d.box.y+d.box.h<=h.box.y+h.box.h)
            pickerIcon|=d.box.x>=h.box.x&&d.box.x+d.box.w<=h.box.x+h.box.w;
    Check(pickerIcon,"loot picker placard renders the item icon");CheckGeometry(m);
    Click(m,"add-loot",m.catalog.entries[0][0].path);Check(m.drops[0].icon==m.catalog.entries[0][0].icon,"selected loot carries native icon path");
    m.drops[0].min="2";m.drops[0].max="6";m.drops[0].chance="17.5";
    f=m.Render();bool selectedIcon=false;const auto minimum=Control(m,"focus","min:"+m.drops[0].item);
    for(const auto& d:f.draws)if(d.kind==Draw::Kind::Icon&&d.text==m.drops[0].icon)
        selectedIcon|=d.box.x+d.box.w<minimum.box.x&&d.box.y>=minimum.box.y-4;
    Check(selectedIcon,"selected loot icon left of row fields");
    // A scan in progress is not a pending mutation. Exercise actual click -> command -> encoder.
    m.indexing=true;m.busy=false;Click(m,"spawn");const auto command=m.TakeCommand();
    Check(command&&command->kind==Command::Kind::Spawn,"spawn click submits while scan is active");
    const auto packet=Encode(*command);
    Check(Text(packet,"Action")=="Spawn","shared settings action name");
    Check(Text(packet,"Player")=="/World.PC2","exact connected recipient");
    Check(Text(packet,"Definition")==m.node->id,"definition preserved");Check(Text(packet,"Class")==m.node->path,"class path always supplied");
    Check(Text(packet,"Name")=="Patrol leader","entered name preserved");
    Check(std::get<int>(packet.values.at("Count"))==3,"entered count preserved");
    Check(std::get<double>(packet.values.at("Scale"))==1.6,"entered scale preserved");
    Check(std::get<bool>(packet.values.at("GhostMesh")),"visual effect preserved");
    Check(!std::get<bool>(packet.values.at("Resource")),"AI resource flag false");
    Check(std::get<bool>(packet.values.at("AppendAdditionalDrops")),"base loot retained");
    Check(packet.drops.size()==1&&packet.drops[0].min==2&&packet.drops[0].max==6&&packet.drops[0].chance==17.5,"additional drops reach request encoder");
    m.Activate("spawn");Check(!m.TakeCommand(),"busy duplicate never requeues");m.busy=false;
    m.ShowActionResult("Spawn completed","Spawned 3 temporary actors.",true);Check(m.actionSuccess,"success receipt marked");
    const auto oldScroll=m.dropScroll;m.Wheel(-30);Check(m.dropScroll==oldScroll,"result modal blocks underlying scroll");
    for(const auto& h:m.Render().hits)Check(h.action=="close","result modal only allows dismissal");
    m.Focus("name");m.Character('X');Check(m.name=="Patrol leader","result modal blocks underlying edit");
    Click(m,"close");Check(!m.actionReportOpen&&m.node&&m.name=="Patrol leader"&&!m.closeRequested,"receipt dismissal retains form");
    m.count="0";Click(m,"spawn");Check(m.actionReportOpen&&!m.actionSuccess&&!m.TakeCommand(),"validation failure visibly reported without submission");
    Click(m,"close");m.count="3";m.ShowActionResult("Spawn stopped","Spawn stopped [ground placement]: no ground.",false);
    Check(m.actionMessage.find("ground placement")!=std::string::npos,"backend error detail displayed");m.Key(27);m.Key(27);
    Click(m,"tab","2");Click(m,"node-subtab","1");Click(m,"node",m.catalog.entries[2][0].id);m.effect=Effect::None;
    const auto resource=Encode(m.SpawnCommand());Check(std::get<bool>(resource.values.at("Resource")),"stone selected through resource tab");
    Check(!std::get<bool>(resource.values.at("GhostMesh")),"None sends false, not inherited ghost");m.effect=Effect::Inherit;
    Check(!Encode(m.SpawnCommand()).values.contains("GhostMesh"),"inherit leaves original effect untouched");
    auto stale=m.catalog;stale.entries[2][0].path="/Game/Replaced.Replaced_C";m.Update(stale);
    Reject([&]{m.SpawnCommand();},"changed class mapping rejected");m.Update(Fixture());
    m.recipient="/World.Disconnected";Click(m,"spawn");
    Check(m.actionReportOpen&&!m.TakeCommand()&&!m.actionSuccess,"missing recipient gives a visible error instead of an inert button");
    m.Key(27);m.recipient="/World.PC1";
    auto invalid=m.SpawnCommand();invalid.classPath="not a path";Reject([&]{Encode(invalid);},"partial class path rejected");
    invalid=m.SpawnCommand();invalid.count=21;Reject([&]{Encode(invalid);},"excessive count rejected");
    invalid=m.SpawnCommand();invalid.loot={{"/Game/Item.Item",4,1,100}};Reject([&]{Encode(invalid);},"inverted additional loot range rejected");
    // Discovery candidates come from mounted records, not a fixed enemy whitelist.
    for(const auto* name:{"BP_OreNode_Copper","BP_OreNode_Stone_C","BP_MiningRock_Granite","BP_MiningRock_RuneEssence_C"}){
        Check(Rules::MiningName(name),"real mining family recognized");Check(Rules::BlueprintCandidate(name,""),"ore and stone survive missing metadata");
        Check(Rules::Priority("/Game/Anywhere",name)==0,"mining scheduled first regardless of folder");
    }
    for(const auto* name:{"CompletelyNewCreature","PassiveAnimal","BossVariant","MerchantVariant"})
        Check(Rules::BlueprintCandidate(name,"/Script/Engine.BlueprintGeneratedClass"),"all blueprint candidates admitted for native inheritance check");
    Check(Rules::BlueprintCandidate("BP_FH_Tree_Willow_01","None"),"unnamed class metadata still supports known blueprint convention");
    Check(Rules::BlueprintCandidate("AI_NewCreature_C",""),"AI fallback");
    Check(!Rules::BlueprintCandidate("T_StoneIcon","Texture2D"),"nonblueprint texture rejected");
    Check(!Rules::BlueprintCandidate("SK_Boss","SkeletalMesh"),"skeletal mesh is not a spawn class");
    for(const auto* name:{"HarvestableActor","TreeBase","FellableTree","Sapling","OreNode","MiningRock","BP_BlightwoodRoot","SplittableLog"})
        Check(Rules::ResourceAncestry(name),"resource ancestry includes existing families");
    Check(!Rules::ResourceAncestry("DominionAICharacter"),"AI not classified as a resource by ancestry tokens");
    Check(Rules::GeneratedClassPath("/Game/World/BP_Test","BP_Test")=="/Game/World/BP_Test.BP_Test_C","add generated class suffix");
    Check(Rules::GeneratedClassPath("/Game/World/BP_Test","BP_Test_C")=="/Game/World/BP_Test.BP_Test_C","never double suffix");
    for(const auto* name:{"", "BP_Bad/Name", "BP_Bad.Name", "BP_Bad\n", "BP Bad"})
        Check(Rules::GeneratedClassPath("/Game/Test",name).empty(),"reject malformed asset names");
    Check(Rules::GeneratedClassPath("/Game/Test.Test","BP_Test").empty(),"package cannot already contain object suffix");
    Check(Rules::GeneratedClassPath(std::string("/Game/A\0B",9),"BP_Test").empty(),"control bytes rejected");
    std::cout<<checks<<" compact UI/catalog rules/spawn-encoder checks passed.\n";
}
