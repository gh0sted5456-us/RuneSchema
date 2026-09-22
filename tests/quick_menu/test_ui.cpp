#include "Generator/QuickMenuUI.h"
#include <cstdlib>
#include <iostream>
#include <random>
using namespace PS::QuickUI;
static int checks=0;
void Check(bool value,const char* reason){++checks;if(!value){std::cerr<<"FAIL: "<<reason<<"\n";std::exit(1);}}
template<class F> void Throws(F&& fn,const char* reason){bool failed=false;try{fn();}catch(const std::exception&){failed=true;}Check(failed,reason);}
Catalog Fixture(int items=153) {
    Catalog c;c.authority=true;c.players={{"self","Host",true},{"peer","Guest",false}};
    for(int i=0;i<items;++i)c.entries[0].push_back({"/Item"+std::to_string(i),"Item "+std::to_string(i),"/Item"+std::to_string(i),{},i%2?"rune air":"armour iron"});
    for(int i=0;i<77;++i){c.entries[1].push_back({"@loaded-ai:/Enemy"+std::to_string(i),"Goblin "+std::to_string(i),"/Enemy"+std::to_string(i),{},"enemy"});c.entries[1].back().nodeKind="AI";}
    for(int i=0;i<45;++i){c.entries[2].push_back({"@loaded-resource:/Node"+std::to_string(i),"Ore "+std::to_string(i),"/Node"+std::to_string(i),{},"resource"});c.entries[2].back().nodeKind="Resource";c.entries[2].back().resourceFamily="Mineral";}
    return c;
}
void Click(Model& m,const std::string& action,const std::string& arg={}) {
    const auto frame=m.Render();bool found=false;
    for(const auto& h:frame.hits)if(h.action==action&&(arg.empty()||h.arg==arg)) {
        m.Click(frame,h.box.x+h.box.w/2,h.box.y+h.box.h/2);found=true;break;
    }
    Check(found,("clickable: "+action).c_str());
}
int main() {
    Model m;m.Update(Fixture());Check(m.recipient=="self","self is the default, not array order");
    auto frame=m.Render();std::vector<Hit> tiles;for(const auto& h:frame.hits)if(h.action=="toggle-item")tiles.push_back(h);
    Check(tiles.size()==12,"only three visible rows are drawn");
    for(int row=0;row<3;++row)for(int col=0;col<4;++col){Check(tiles[row*4+col].box.y==tiles[row*4].box.y,"four across");if(col)Check(tiles[row*4+col].box.x>tiles[row*4+col-1].box.x,"even column progression");}
    Check(frame.rightHits.size()==12,"every visible item supports right-click details");
    Click(m,"toggle-item","/Item0");Click(m,"toggle-item","/Item1");Check(m.selection.size()==2,"multi-select");
    m.Activate("scroll-end");frame=m.Render();bool last=false;for(const auto& h:frame.hits)if(h.arg=="/Item152")last=true;Check(last,"paging reaches final item");Check(m.selection.size()==2,"paging retains selection");
    m.Focus("filter");for(char c:std::string("rune air"))m.Character(c);Check(m.Filtered(0).size()==76,"multi-token filter");Check(m.scroll[0]==0,"filter resets scroll");Check(m.selection.size()==2,"filter retains selection");
    Click(m,"player-picker");auto rosterCommand=m.TakeCommand();Check(rosterCommand&&rosterCommand->kind==Command::Kind::Players,"player picker requests a fresh roster");m.busy=false;Click(m,"player","peer");Check(m.recipient=="peer"&&m.recipientName=="Guest","connected player selection");
    auto replaced=Fixture();replaced.players[1].id="peer-reconnected";m.Update(replaced);Check(m.recipient=="peer-reconnected"&&m.Recipient(),"unique remembered player identity rebinds a replaced controller path");
    m.quantity="7";Click(m,"give");auto command=m.TakeCommand();Check(command&&command->kind==Command::Kind::Give,"give command");Check(command->player=="peer-reconnected"&&command->grants.size()==2&&command->grants[0].count==7,"batch target, selection and quantity");
    m.Activate("give");Check(!m.TakeCommand(),"double-click cannot requeue pending work");m.busy=false;
    auto disconnected=Fixture();disconnected.players.erase(disconnected.players.begin()+1);m.Update(disconnected);Check(m.recipient=="peer-reconnected"&&!m.Recipient(),"disconnect does not reroute");Throws([&]{m.GiveCommand();},"disconnected target rejected");
    m.Update(Fixture());m.catalog.authority=false;Throws([&]{m.GiveCommand();},"clients cannot bypass authority");m.catalog.authority=true;
    m.quantity="0";Throws([&]{m.GiveCommand();},"zero grant rejected");m.quantity="10001";Throws([&]{m.GiveCommand();},"excessive grant rejected");m.quantity="1.2";Throws([&]{m.GiveCommand();},"fractional count rejected");m.quantity="3";
    Click(m,"tab","1");Check(m.tab==Tab::Enemies,"ENEMIES tab");Click(m,"node-subtab","1");Click(m,"node","@loaded-ai:/Enemy0");Check(m.node.has_value(),"selection opens second window");
    frame=m.Render();for(const auto& h:frame.hits)Check(h.action!="tab"&&h.action!="toggle-item"&&h.action!="give","modal blocks underlying controls");
    m.name="Captain Goblin";m.scale="1.75";m.count="8";m.effect=Effect::Ghost;
    Click(m,"loot-picker");m.Focus("loot-filter");for(char c:std::string("Item 15"))m.Character(c);Check(m.LootMatches().size()>0,"loot typed search");Click(m,"add-loot","/Item15");Check(!m.lootPicker&&m.drops.size()==1,"result populates loot");
    m.drops[0].min="2";m.drops[0].max="5";m.drops[0].chance="12.5";
    auto spawn=m.SpawnCommand();Check(!spawn.resource&&spawn.definition=="@loaded-ai:/Enemy0"&&spawn.name=="Captain Goblin","enemy request identity");Check(spawn.scale==1.75&&spawn.count==8&&spawn.effect==Effect::Ghost,"enemy scale/count/effect");Check(spawn.loot.size()==1&&spawn.loot[0].min==2&&spawn.loot[0].max==5&&spawn.loot[0].chance==12.5,"additional loot data");
    for(const auto& bad:{"nan","inf","-1","0.09","10.01","1abc",""}){m.scale=bad;Throws([&]{m.SpawnCommand();},"invalid scale");}m.scale="1";
    m.count="21";Throws([&]{m.SpawnCommand();},"spawn count bound");m.count="2";
    m.drops[0].min="6";Throws([&]{m.SpawnCommand();},"loot min/max ordering");m.drops[0].min="1";
    m.drops[0].chance="101";Throws([&]{m.SpawnCommand();},"loot chance bound");m.drops[0].chance="0";Check(m.SpawnCommand().loot[0].chance==0,"zero drop chance allowed");
    m.Key(27);Check(!m.node&&!m.closeRequested,"escape returns to enemy list");
    Click(m,"tab","2");Click(m,"node-subtab","1");Click(m,"node","@loaded-resource:/Node0");m.name="Copper test";m.scale="2";m.count="3";m.effect=Effect::None;
    Check(m.SpawnCommand().resource&&m.SpawnCommand().count==3,"resource uses same full configuration flow");
    m.Focus("name");m.Key('A',true);m.Character(0x1f409);m.Character('X');m.Key(37);m.Key(8);Check(m.name=="X","UTF-8 backspace at caret boundary");m.Key(46);Check(m.name.empty(),"delete at caret");
    m.Character(0xd800);Check(m.name.empty(),"unpaired surrogate never enters UTF8");
    m.Character(0x00e9);m.Key(36);m.Character('A');Check(m.name=="A\xc3\xa9","home inserts without corrupting UTF8");
    m.Key(35);m.Key(8);Check(m.name=="A","end and backspace");
    m.Key(27);m.Key(27);Check(m.closeRequested,"escape closes main window");
    m.Reset();Check(m.selection.empty()&&m.recipient.empty()&&m.catalog.entries[0].empty()&&!m.node&&!m.busy,"world reset drops all identities and selections");
    m.Update(Fixture(25000));m.Activate("scroll-end");Check(m.scroll[0]==2083,"large roster paging is bounded");
    m.filters[0]="no such asset";m.ClampScroll();Check(m.scroll[0]==0,"empty result page clamps");
    m.filters[0].clear();m.scroll[0]=0;
    for(int i=0;i<65;++i) { m.Activate("toggle-item","/Item"+std::to_string(i)); }
    Check(m.selection.size()==64,"selection limit");
    m.Activate("selected-only");Check(m.Filtered(0).size()==64,"selected-only retains offscreen items");
    m.Activate("clear");Check(m.Filtered(0).empty(),"clear selected-only view");
    m.selectedOnly=false;m.Update(Fixture());
    Click(m,"index");Check(m.busy&&m.indexing&&m.TakeCommand()->kind==Command::Kind::Index,"explicit packaged index action");Click(m,"cancel");Check(m.TakeCommand()->kind==Command::Kind::Cancel,"cancel bypasses the occupied work slot");m.busy=false;m.indexing=false;
    std::vector<Grant> batch{{"/One",1},{"/Two",2},{"/Three",3}};int calls=0;
    auto results=ExecuteBatch(batch,[&](const Grant&){++calls;});Check(calls==3&&results.size()==3,"complete batch executes once per item");
    calls=0;results=ExecuteBatch(batch,[&](const Grant&){if(++calls==2)throw std::runtime_error("capacity");});
    Check(calls==2&&results[0].state==BatchRow::State::Confirmed&&results[1].state==BatchRow::State::Unconfirmed&&results[2].state==BatchRow::State::NotAttempted,"partial batch stops without retry or false rollback");
    Throws([&]{ExecuteBatch(std::vector<Grant>{{"/One",1},{"/One",1}},[](const Grant&){});},"duplicate batch validation before mutation");
    Throws([&]{ExecuteBatch(std::vector<Grant>{{"bad",1}},[](const Grant&){});},"invalid path rejected before mutation");
    m.Reset();m.Update(Fixture());
    const auto* cached=m.Filtered(0).data();m.Render();Check(m.Filtered(0).data()==cached,"render reuses filtered indices instead of rebuilding the full roster");
    m.grantReport={{"Iron armour","Confirmed","Inventory delta confirmed."},{"Air rune","Unconfirmed","Inventory full; check before retrying."}};
    m.Activate("report");Check(m.reportOpen,"last receipt can be inspected");
    for(const auto& h:m.Render().hits)Check(h.action!="give"&&h.action!="toggle-item"&&h.action!="tab","receipt blocks underlying controls");
    m.Focus("filter");m.Character('Z');Check(m.filters[0].empty(),"report consumes text rather than editing behind the modal");
    m.Key(27);Check(!m.reportOpen&&!m.closeRequested,"receipt Escape returns to menu");
    m.Update(Fixture(31));Check(m.Filtered(0).size()==31,"replacement catalog invalidates cache");
    m.Activate("toggle-item","/Item0");m.Activate("selected-only");Check(m.Filtered(0).size()==1,"selection cache is separate from normal filter");
    m.selection.clear();m.selection.insert("/Item1");Check(m.catalog.entries[0][m.Filtered(0)[0]].path=="/Item1","equal-size changed selections invalidate cached indices");
    // Interaction fuzzing: all hit rectangles must remain in the logical canvas.
    std::mt19937 rng(17);m.Reset();m.Update(Fixture());
    for(int n=0;n<800;++n){
        if(n%17==0)m.Back();
        if(n%7==0)m.Wheel(static_cast<int>(rng()%21)-10);
        const auto f=m.Render();
        for(const auto& h:f.hits)Check(h.box.x>=0&&h.box.y>=0&&h.box.x+h.box.w<=Width&&h.box.y+h.box.h<=Height,"hit targets inside canvas");
        for(const auto& h:f.rightHits)Check(h.box.x>=0&&h.box.y>=0&&h.box.x+h.box.w<=Width&&h.box.y+h.box.h<=Height,"right-click targets inside canvas");
        if(!f.hits.empty()){const auto& h=f.hits[rng()%f.hits.size()];m.Click(f,h.box.x+1,h.box.y+1);}
        m.TakeCommand();m.busy=false;m.indexing=false;m.closeRequested=false;
    }
    std::cout<<checks<<" QuickMenuUI interaction/batch checks passed.\n";
}
