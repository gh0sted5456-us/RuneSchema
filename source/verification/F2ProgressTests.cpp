#include "Generator/QuickMenuUI.h"
#include <iostream>
#include <cmath>
using namespace PS::QuickUI;
int checks=0;void check(bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);}
std::vector<Hit> hits(const Frame& f,const std::string& name){std::vector<Hit> out;for(const auto& h:f.hits)if(h.action==name)out.push_back(h);return out;}
bool text(const Frame& f,const std::string& value){for(const auto& d:f.draws)if(d.kind==Draw::Kind::Text&&d.text.find(value)!=d.text.npos)return true;return false;}
float fill(const Frame& f){for(const auto& d:f.draws)if(d.kind==Draw::Kind::Rectangle&&d.box.y==695&&d.color[0]==.25f)return d.box.w;return -1;}
int main(){try{
    Model m;Catalog c;c.authority=true;c.players.push_back({"self","Self",true});
    for(int i=0;i<40;++i){Entry e;e.id=e.path="/Game/I/ITEM_"+std::to_string(i)+".ITEM_"+std::to_string(i);e.name="Item "+std::to_string(i);e.cooked=true;e.cloneEligible=true;c.entries[0].push_back(e);}
    m.Update(c);check(Width==960&&Height==720,"compact layout dimensions");
    m.Activate("refresh");auto refresh=m.TakeCommand();check(refresh&&refresh->kind==Command::Kind::Refresh,"Refresh submits shared index command");m.busy=false;
    m.indexing=true;m.indexStage="Caching resources";m.indexHasTotal=true;m.indexDone=25;m.indexTotal=100;
    auto frame=m.Render();check(std::abs(fill(frame)-738.f*HorizontalFit*.25f)<.01f,"progress is actual checked/known total");
    check(text(frame,"Caching resources")&&text(frame,"25 / 100")&&text(frame,"Items 40"),"stage, checks and category counts visible");
    check(hits(frame,"refresh").empty()&&hits(frame,"cancel").size()==1,"repeat refresh disabled; cancellation stays available");
    m.indexDone=125;check(std::abs(fill(m.Render())-738.f*HorizontalFit)<.01f,"progress fraction clamped");
    m.indexHasTotal=false;check(fill(m.Render())==-1&&!text(m.Render(),"125 / 100"),"unknown total uses indeterminate bar, no fake ratio");
    m.indexing=false;m.indexFinished=true;m.indexHasTotal=true;m.indexTotal=0;m.indexStage="No candidates";
    check(std::abs(fill(m.Render())-738.f*HorizontalFit)<.01f,"zero-work completed state cannot divide by zero");
    m.scroll[0]=2;auto before=hits(m.Render(),"toggle-item");for(int i=0;i<100;++i)m.Wheel(i%2?1:-1);
    check(m.scroll[0]==2&&hits(m.Render(),"toggle-item")[0].arg==before[0].arg,"wheel cannot change visible placards");
    check(hits(m.Render(),"toggle-item").size()==12,"page contains max twelve placards");
    m.cloneSourcePicker=true;m.lootScroll=1;m.Wheel(-1);check(m.lootScroll==1,"source picker wheel stays on page");
    m.Key(34);check(m.lootScroll==2,"source picker explicit Page Down still works");
    m.cloneSourcePicker=false;m.tab=Tab::Enemies;m.scroll[1]=0;m.Wheel(-1);check(m.scroll[1]==0,"enemy wheel cannot page");
    m.tab=Tab::Items;m.quantity="10000";frame=m.Render();bool centred=false;
    const auto field=hits(frame,"focus");for(const auto& d:frame.draws)if(d.kind==Draw::Kind::Text&&d.text=="10000")centred=d.centreX;
    check(centred,"five-digit quantity visible and centered");
    m.cloneSourcePicker=true;m.indexing=true;m.indexHasTotal=true;m.indexStage="Caching items";m.indexDone=3;m.indexTotal=10;
    frame=m.Render();check(text(frame,"Caching items")&&fill(frame)>0,"progress stays visible in source picker");
    const auto favorites=hits(frame,"favorite");check(!favorites.empty()&&std::abs(favorites.front().box.w-36)<.01f,"favorite target preserved at narrower width");
    m.cloneSourcePicker=false;m.selection.insert(c.entries[0][0].path);
    auto unavailable=m.catalog;unavailable.entries[0][0].available=false;m.Update(unavailable);
    bool refused=false;try {m.GiveCommand();}catch(const std::exception&) {refused=true;}
    check(refused,"previously selected item cannot be granted from an unvalidated cache row");
    m.Reset();check(m.indexDone==0&&m.indexTotal==0&&!m.indexing&&!m.indexFinished,"world reset clears index presentation");
    std::cout<<"PASS: "<<checks<<" progress, Refresh, width and explicit-pagination assertions. UI model only.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
