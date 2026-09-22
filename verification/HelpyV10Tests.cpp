#include "Generator/QuickMenuSpawnRequest.h"
#include "Generator/F2CatalogPlan.h"
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <variant>
using namespace PS::QuickUI;
using K=PS::HelpyNodes::Kind;
static unsigned checks=0;
void verify(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
template<class F>void fails(F f,const char* why){bool failed=false;try{f();}catch(const std::exception&){failed=true;}verify(failed,why);}
Entry nodeEntry(K k,int i) {
 Entry e;e.id=(k==K::Npc?"@runeschema-npc:Example:Npc":k==K::AI?"@loaded-ai:":"@loaded-resource:")+std::to_string(i);
 e.path=std::string(k==K::Tree?"/Game/Gameplay/World/Trees/":k==K::Mineral?"/UmbralSands/Gameplay/World/Mining/":"/Game/Gameplay/NPCs/")+"BP_Test"+std::to_string(i)+".BP_Test"+std::to_string(i)+"_C";
 e.name=std::string(k==K::Npc?"Portable vendor ":k==K::AI?"AI variant ":k==K::Tree?"Tree variant ":k==K::Mineral?"Ore variant ":"Fishing resource ")+std::to_string(i);
 e.nodeKind=k==K::Npc?"NPC":k==K::AI?"AI":"Resource";e.resourceFamily=k==K::Tree?"Tree":k==K::Mineral?"Mineral":"";
 if(k==K::OtherResource)e.path="/Fishing/Gameplay/World/Fishing/BP_Spot"+std::to_string(i)+".BP_Spot"+std::to_string(i)+"_C";
 e.cooked=k!=K::Npc;e.runeSchemaManaged=k==K::Npc;e.temporaryAllowed=e.permanentAllowed=k==K::Npc;
 return e;
}
Model nodesModel(int each=23) {
 Model m;Catalog c;c.authority=true;c.players.push_back({"local","Local player",true});
 for(int i=0;i<each;++i){for(auto k:{K::Npc,K::AI})c.entries[1].push_back(nodeEntry(k,i));for(auto k:{K::Tree,K::Mineral,K::OtherResource})c.entries[2].push_back(nodeEntry(k,i));}
 Entry item;item.id=item.path="/Game/Items/ITEM_Test.ITEM_Test";item.name="Example item";item.cooked=true;c.entries[0].push_back(item);
 m.Update(c);return m;
}
std::size_t hits(const Frame& f,const std::string& a){return std::count_if(f.hits.begin(),f.hits.end(),[&](const auto& h){return h.action==a;});}
bool text(const Frame& f,const std::string& s){return std::any_of(f.draws.begin(),f.draws.end(),[&](const auto& d){return d.text==s;});}
void frameBounds(const Frame& f){for(const auto& h:f.hits)verify(h.box.x>=0&&h.box.y>=0&&h.box.x+h.box.w<=Width+.01&&h.box.y+h.box.h<=Height+.01,"node hit bounds");}
int main(){try {
 PS::Authoring::PermanentSpawn=false;PS::Authoring::EnemyPower=-1;
 auto m=nodesModel();m.tab=Tab::Enemies;
 verify(m.Filtered(1).size()==23,"NPC tab excludes AI");verify(text(m.Render(),"NPCs & AI"),"main heading");
 verify(hits(m.Render(),"node")==9&&hits(m.Render(),"node-favorite")==9,"nine node and independent star controls");
 for(const auto sub:{0,1}){m.Activate("node-subtab",std::to_string(sub));verify(m.scroll[1]==0,"subtab resets page");
  verify(m.Filtered(1).size()==23,"separate NPC / AI sets");m.Activate("scroll-end");verify(m.scroll[1]==2&&hits(m.Render(),"node")==5,"partial final page");
  m.Wheel(6);verify(m.scroll[1]==2,"wheel cannot turn node pages");m.Page(1);verify(m.scroll[1]==1,"explicit node paging");frameBounds(m.Render());}
 m.filters[1]="variant 22";m.ClampScroll();verify(m.Filtered(1).size()==1&&m.scroll[1]==0,"search covers all AI before paging");
 m.filters[1].clear();m.Activate("node-subtab","0");m.filters[1]="vendor 22";verify(m.Filtered(1).size()==1,"search on NPC tab");m.filters[1].clear();
 m.tab=Tab::Resources;verify(m.Filtered(2).size()==23,"tree tab only trees");
 m.Activate("node-subtab","1");verify(m.Filtered(2).size()==23,"stone and ore only minerals");m.Activate("other-resources");verify(m.Filtered(2).size()==46,"other resources remain accessible without fourth tab");
 m.Activate("node-subtab","0");verify(m.Filtered(2).size()==23,"other option never contaminates tree tab");
 verify(PS::HelpyNodes::ResourceKind("/Game/Gameplay/World/Mining/BP.BP_C","Tree")==K::Tree,"native evidence before path");
 verify(PS::HelpyNodes::ResourceKind("/Game/Gameplay/World/Trees/BP.BP_C","Mineral")==K::Mineral,"native mineral evidence before tree path");
 verify(PS::HelpyNodes::ResourceKind("/DowdunReach/Gameplay/World/Trees/Maple/BP.BP_C")==K::Tree,"plugin tree path");
 verify(PS::HelpyNodes::ResourceKind("/ScornedWilderness/Gameplay/World/Mining/BP.BP_C")==K::Mineral,"plugin mining path");
 const auto npc=nodeEntry(K::Npc,3),ai=nodeEntry(K::AI,3),tree=nodeEntry(K::Tree,3),ore=nodeEntry(K::Mineral,4),other=nodeEntry(K::OtherResource,5);
 m.tab=Tab::Enemies;const auto nk=m.NodeFavoriteKey(npc,1),ak=m.NodeFavoriteKey(ai,1);
 m.Activate("node-favorite",nk);verify(m.IsFavorite(nk)&&!m.node&&!m.TakeCommand(),"favorite never opens / spawns actor");
 m.Activate("node-favorite",ak);m.Activate("node-subtab","2");verify(m.Filtered(1).size()==2,"NPC and AI favorites together");
 m.ToggleFavorite(m.catalog.entries[0][0].path);m.tab=Tab::Items;m.favoritesTab=true;verify(m.BrowserEntries(0).size()==1,"item favorites never display typed node entries");
 m.tab=Tab::Resources;for(const auto& e:{tree,ore,other})m.Activate("node-favorite",m.NodeFavoriteKey(e,2));m.Activate("node-subtab","2");verify(m.Filtered(2).size()==3,"resource favorites include every family");
 m.tab=Tab::Enemies;m.npcTab=2;auto c=m.catalog;std::erase_if(c.entries[1],[&](const auto& e){return e.id==npc.id;});m.Update(c);
 const auto& browsed=m.BrowserEntries(1);verify(browsed.size()==2,"missing NPC favorite retained");
 const auto unavailable=std::find_if(browsed.begin(),browsed.end(),[&](const auto& e){return e.id==npc.id;});
 verify(unavailable!=browsed.end()&&!unavailable->available&&!unavailable->temporaryAllowed,"missing favorite cannot assert permission");
 m.Activate("node",npc.id);verify(!m.node,"unavailable favorite cannot resolve source from saved label");m.Activate("node-favorite",nk);verify(!m.IsFavorite(nk),"missing favorite can be removed");
 verify(m.NodeFavoriteKey(nodeEntry(K::Npc,6),1)!=m.NodeFavoriteKey(nodeEntry(K::AI,6),1),"typed identities separate NPC and AI");
 for(const auto* key:{"@npc:","@ai:\nbad","@resource:a\\b","@other:value",""})verify(!PS::HelpyNodes::ValidFavoriteKey(key),"malformed favorite key rejected");
 verify(PS::HelpyNodes::ValidFavoriteKey("@npc:@runeschema-npc:Mod:Vendor"),"authored mod and definition preserved");
 verify(!PS::QuickDecorations::ValidFavoritePath(nk)&&PS::QuickDecorations::ValidFavoriteKey(nk),"item-only path validator not broadened");
 auto stored=m.favorites;m.Reset();verify(m.favorites==stored,"world reset retains favorites but not actor commands");
 m=nodesModel();m.tab=Tab::Enemies;m.Activate("node",npc.id);m.name="Portable vendor";m.npcDuration="60";m.count="not a batch";m.effect=Effect::Ghost;PS::Authoring::EnemyPower=99;
 auto cmd=m.SpawnCommand();verify(cmd.npc&&!cmd.permanent&&cmd.durationSeconds==60&&cmd.count==1&&cmd.effect==Effect::Inherit&&cmd.powerLevel==-1,"NPC request independent from AI state");
 using Val=std::variant<std::string,bool,int,double>;std::map<std::string,Val> encoded;unsigned drops=0;
 EncodeSpawn(cmd,[&](const char* key,auto value){encoded[key]=value;},[&](const Loot&){++drops;});
 verify(std::get<bool>(encoded.at("NPC"))&&std::get<int>(encoded.at("DurationSeconds"))==60&&drops==0,"NPC request encoder carries lifetime");
 verify(!encoded.contains("PowerLevel")&&!encoded.contains("GhostMesh")&&!encoded.contains("AppendAdditionalDrops"),"no AI mutation flags on NPC request");
 for(const auto value:{"0","4","86401","NaN","-1","5.5",""}){m.npcDuration=value;fails([&]{(void)m.SpawnCommand();},"bad duration rejected before queue");}
 m.npcDuration="5";verify(m.SpawnCommand().durationSeconds==5,"minimum lifetime");m.npcDuration="86400";verify(m.SpawnCommand().durationSeconds==86400,"maximum lifetime");
 m.npcDuration="ignored";PS::Authoring::PermanentSpawn=true;verify(m.SpawnCommand().durationSeconds==0,"permanent excludes old lifetime field");
 m.Focus("scale");m.Key(9,false);verify(m.focus=="name","hidden duration is absent from permanent keyboard order");
 frameBounds(m.Render());PS::Authoring::PermanentSpawn=false;m.npcDuration="300";frameBounds(m.Render());
 c=m.catalog;for(auto& e:c.entries[1])if(e.id==npc.id){e.temporaryAllowed=false;e.temporaryReason="No native save exclusion";}
 m.Update(c);fails([&]{(void)m.SpawnCommand();},"backend permissions refreshed before submit even if open UI is stale");
 c=m.catalog;for(auto& e:c.entries[1])if(e.id==npc.id)e.path="/Game/Other/BP.BP_C";m.Update(c);fails([&]{(void)m.SpawnCommand();},"changed selected class refused");
 auto invalid=cmd;invalid.count=2;fails([&]{EncodeSpawn(invalid,[](const char*,auto){},[](const Loot&){});},"NPC encoder disallows batch");
 invalid=cmd;invalid.powerLevel=3;fails([&]{EncodeSpawn(invalid,[](const char*,auto){},[](const Loot&){});},"NPC encoder disallows power");
 invalid=cmd;invalid.effect=Effect::Ghost;fails([&]{EncodeSpawn(invalid,[](const char*,auto){},[](const Loot&){});},"NPC encoder disallows effect");
 invalid=cmd;invalid.loot.push_back({"/Game/I.I",1,1,100});fails([&]{EncodeSpawn(invalid,[](const char*,auto){},[](const Loot&){});},"NPC encoder disallows loot");
 m=nodesModel();m.tab=Tab::Enemies;m.Activate("dismiss-npcs");auto dismiss=m.TakeCommand();verify(dismiss&&dismiss->kind==Command::Kind::DismissNpcs&&dismiss->player=="local","dismiss routes through game-thread request");
 m=nodesModel();m.catalog.authority=false;m.Activate("dismiss-npcs");verify(!m.TakeCommand(),"non-authority cannot dismiss");
 const auto lease=PS::HelpyNodes::Lease::Start(100,5);verify(!lease.Due(104.999)&&lease.Due(105)&&lease.Due(1000),"precise deadline boundary");
 auto retire=lease;retire.Retire();verify(retire.Due(0),"retirement remains sticky until cleanup confirms");
 verify(!lease.Due(std::numeric_limits<double>::quiet_NaN()),"bad elapsed input cannot counterfeit deadline");
 fails([]{(void)PS::HelpyNodes::Lease::Start(-1,5);},"negative clock rejected");fails([]{(void)PS::HelpyNodes::Lease::Start(0,0);},"zero lifetime rejected (would clear native timer)");
 PS::F2Catalog::Sources sources;
 auto path=PS::F2Catalog::FromExport("Content/Gameplay/NPCs/BP_Test.json",sources);
 verify(path&&path->kind==PS::F2Catalog::Kind::Npc,"public index NPC path classified without native spawn claim");
 verify(PS::F2Catalog::ParseKind("NPC")==PS::F2Catalog::Kind::Npc,"NPC cache kind accepted");verify(std::string(PS::F2Catalog::KindName(PS::F2Catalog::Kind::Npc))=="NPC","NPC cache kind encoded");
 // Exhaust every short final-page size in both catalogues.
 for(int n=0;n<30;++n){m=nodesModel(n);for(int t:{1,2}){m.tab=static_cast<Tab>(t);for(int sub:{0,1}){m.Activate("node-subtab",std::to_string(sub));m.Activate("scroll-end");verify(hits(m.Render(),"node")<=9,"every node page bounded to nine rows");verify(!m.TakeCommand(),"paging never mutates world");}}}
 std::cout<<"PASS: "<<checks<<" Helpy v10 tab, favorites, request and lifetime-policy assertions. Model only.\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
