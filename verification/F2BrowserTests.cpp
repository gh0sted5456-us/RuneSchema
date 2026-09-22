#include "Generator/QuickMenuUI.h"
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
using namespace PS::QuickUI;
namespace D=PS::QuickDecorations;
int checks=0;
void check(bool yes,const std::string& message){++checks;if(!yes)throw std::runtime_error(message);}
Entry item(int i){Entry e;e.id=e.path="/Game/Items/ITEM_"+std::to_string(i)+".ITEM_"+std::to_string(i);e.name="Item "+std::to_string(i);e.icon="/Game/Icons/T_"+std::to_string(i)+".T_"+std::to_string(i);e.power=i%10;e.cooked=true;e.cloneEligible=true;e.assetClass="WearableEquipmentData";e.appearanceGroup="wearable:head";return e;}
Model model(int n=29){Model m;Catalog c;c.authority=true;c.players.push_back({"local","Local player",true});for(int i=0;i<n;++i)c.entries[0].push_back(item(i));m.Update(std::move(c));return m;}
std::vector<Hit> hits(const Frame& f,const std::string& action){std::vector<Hit> out;for(const auto& h:f.hits)if(h.action==action)out.push_back(h);return out;}
bool text(const Frame& f,const std::string& part){for(const auto& d:f.draws)if(d.kind==Draw::Kind::Text&&d.text.find(part)!=d.text.npos)return true;return false;}
const Draw* badge(const Frame& f,const std::string& path){for(const auto& d:f.draws)if(d.kind==Draw::Kind::Badge&&d.text==path)return &d;return nullptr;}
bool badgeShade(const Frame& f,const Draw& badge){const float x=badge.box.x+badge.box.w/2,y=badge.box.y+badge.box.h/2;for(const auto& d:f.draws)if(d.kind==Draw::Kind::Rectangle&&std::abs(d.color[3]-.74f)<.001f&&d.box.Contains(x,y))return true;return false;}
void dump(const Frame& f,const std::string& path){std::ofstream out(path);for(const auto& d:f.draws){out<<int(d.kind)<<'|'<<d.box.x<<'|'<<d.box.y<<'|'<<d.box.w<<'|'<<d.box.h<<'|'<<d.font<<'|'<<d.centreX;for(auto c:d.color)out<<'|'<<c;out<<'|';for(unsigned char c:d.text){static constexpr char h[]="0123456789abcdef";out<<h[c>>4]<<h[c&15];}out<<'|';for(unsigned char c:d.fallback){static constexpr char h[]="0123456789abcdef";out<<h[c>>4]<<h[c&15];}out<<'\n';}}
void bounds(const Frame& f){for(const auto& h:f.hits){check(h.box.x>=0&&h.box.y>=0&&h.box.x+h.box.w<=Width&&h.box.y+h.box.h<=Height,"hit in window");}for(const auto& d:f.draws)if(d.kind!=Draw::Kind::Text)check(d.box.x>=0&&d.box.y>=0&&d.box.x+d.box.w<=Width&&d.box.y+d.box.h<=Height,"draw in window");}
int main(){try{
 check(ItemsPerPage==12&&Columns==4,"12 cards, four columns");
 check(D::LastPage(0,12)==0&&D::LastPage(1,12)==0&&D::LastPage(12,12)==0&&D::LastPage(13,12)==1&&D::LastPage(25,12)==2,"page boundaries");
 check(D::ClampPage(-99,24,12)==0&&D::ClampPage(100,24,12)==1,"page clamp");
 check(D::LastPage(100,0)==0,"invalid page size guarded");
 check(std::string(D::PowerLevelBadge)=="/Game/Art/UI/PowerLevel/T_PowerLevel_NeutralZone.T_PowerLevel_NeutralZone","native neutral power-level icon path");
 for(auto asset:{D::DragonwildsBadge,D::RuneSchemaBadge,D::FavoriteBadge,D::ModdedBadge}){check(std::string(asset).starts_with(D::BadgeRoot),"correct badge folder");check(std::string(asset).find("T_Bade_")==std::string::npos,"no misspelled alias");}
 check(D::ItemOrigin("/Game/Mods/Pack/Item.Item",true,true)==D::Origin::RuneSchema,"runtime wins over mod path");
 check(D::ItemOrigin("/Game/Mods/Pack/Item.Item",true,false)==D::Origin::Modded,"cooked mod path");
 check(D::ItemOrigin("/Game/ModsBackup/Item.Item",true,false)==D::Origin::Dragonwilds,"whole Mods segment only");
 check(D::ItemOrigin("/Game/Items/Item.Item",true,false)==D::Origin::Dragonwilds,"cooked unmodified origin");
 check(D::ItemOrigin("/Game/Items/Item.Item",false,false)==D::Origin::Unknown,"unknown not invented");
 check(D::ModPath("/GAME/MODS/Test.Item"),"case insensitive segment");
 check(D::Token("Item.Quality.Masterwork","masterwork")&&!D::Token("NotMasterwork","masterwork")&&!D::Token("Any",""),"quality token boundaries");
 check(D::ValidFavoritePath("/Game/Items/A.A")&&!D::ValidFavoritePath("/Game/Items/A")&&!D::ValidFavoritePath("C:\\A.A")&&!D::ValidFavoritePath("/Game/A.A\n"),"favorite object paths");
 auto m=model();std::set<std::string> encountered;
 for(int page=0;page<3;++page){const auto f=m.Render();auto cards=hits(f,"toggle-item");check(cards.size()==std::size_t(page<2?12:5),"partial final page");for(const auto& h:cards)check(encountered.insert(h.arg).second,"no repeated cards between pages");m.Activate("page-next");}
 check(encountered.size()==29&&m.scroll[0]==2,"all entries reachable once");
 m.Activate("scroll-start");check(m.scroll[0]==0,"first page");m.Key(34);check(m.scroll[0]==1,"page down");m.Key(33);check(m.scroll[0]==0,"page up");
 m.Activate("scroll-end");m.Focus("filter");for(char c:std::string("Item 28"))m.Character(c);check(m.scroll[0]==0&&m.Filtered(0).size()==1,"search spans all entries and resets page");check(hits(m.Render(),"toggle-item")[0].arg==item(28).path,"last item found from global query");check(m.ActiveSearch()=="Item 28","search hint for whole catalogue");
 m.filters[0].clear();m.ClampScroll();const auto firstFrame=m.Render();const auto mainRects=hits(firstFrame,"toggle-item");m.Activate("item-subtab","clone");m.Activate("clone-source");
 const auto sourceFrame=m.Render();auto sourceRects=hits(sourceFrame,"source-clone");check(sourceRects.size()==mainRects.size(),"same source page count");for(std::size_t i=0;i<mainRects.size();++i){const auto a=mainRects[i].box,b=sourceRects[i].box;check(a.x==b.x&&a.y==b.y&&a.w==b.w&&a.h==b.h,"same placard layout in source viewer");}
 auto sourceStar=hits(sourceFrame,"favorite")[0];m.Click(sourceFrame,sourceStar.box.x+18,sourceStar.box.y+18);check(m.IsFavorite(item(0).path)&&m.cloneSourcePicker&&!m.TakeCommand()&&m.selection.empty(),"favorite click never selects source or submits command");
 m.Back();m.cloneSourceInfo=m.catalog.entries[0][0];m.cloneReady=true;m.Activate("clone-appearance");auto appearanceFrame=m.Render();check(hits(appearanceFrame,"appearance-clone").size()==12,"appearance grid");
 auto appearanceStar=hits(appearanceFrame,"favorite")[1];m.Click(appearanceFrame,appearanceStar.box.x+18,appearanceStar.box.y+18);check(m.IsFavorite(item(1).path)&&m.cloneAppearancePicker&&!m.TakeCommand(),"favorite click never chooses donor");
 m.Activate("picker-favorites");check(m.LootMatches().size()==2&&m.lootScroll==0,"favorites-only source and appearance filter");m.lootFilter="Item 1";check(m.LootMatches().size()==1,"search intersects favorites");
 m.Back();m.Activate("item-subtab","favorites");m.filters[0].clear();auto favFrame=m.Render();check(hits(favFrame,"toggle-item").size()==2,"dedicated favorites page");
 check(badge(favFrame,D::RuneSchemaBadge)&&!text(favFrame,"RuneSchema")&&!text(favFrame,"R3v"),"versionless icon-only header");
 m.Activate("favorite",item(1).path);check(m.Filtered(0).size()==1,"favorites cache invalidation");
 m.favoritesLoaded=true;m.Reset();check(m.IsFavorite(item(0).path)&&m.favoritesLoaded,"preferences survive world reset");check(m.catalog.entries[0].empty(),"world reset retains no assets");
 m.Activate("item-subtab","favorites");auto ghostFrame=m.Render();check(hits(ghostFrame,"toggle-item").empty()&&hits(ghostFrame,"favorite").size()==1,"missing item ghost cannot spawn but can unfavorite");check(text(ghostFrame,"Unavailable"),"missing favorite visible");m.Activate("favorite",item(0).path);check(m.favorites.empty(),"missing favorite removable");
 auto runtime=model(3);auto c=runtime.catalog;c.entries[0][0].runtimeClone=true;c.entries[0][0].cooked=false;c.entries[0][0].cloneEligible=false;runtime.Update(c);runtime.Activate("favorite",item(0).path);runtime.cloneSourcePicker=true;runtime.pickerFavoritesOnly=true;check(runtime.LootMatches().empty(),"favoriting never makes runtime clone a cooked donor");
 auto duplicate=model(2);c=duplicate.catalog;c.entries[0][0].name=c.entries[0][1].name="Same display name";duplicate.Update(c);duplicate.ToggleFavorite(item(0).path);duplicate.ToggleFavorite(item(1).path);check(duplicate.favorites.size()==2,"identity uses paths not duplicate names");
 auto decorated=model(12);c=decorated.catalog;c.entries[0][0].masterwork=true;c.entries[0][0].quest=true;c.entries[0][0].consumable=true;c.entries[0][0].categoryIcon="/Game/Types/C.C";c.entries[0][1].runtimeClone=true;c.entries[0][1].cooked=false;c.entries[0][2].path="/Game/Mods/Test/ITEM_Mod.ITEM_Mod";decorated.Update(c);decorated.ToggleFavorite(item(0).path);const auto decoratedFrame=decorated.Render();
 check(badge(decoratedFrame,D::DragonwildsBadge)&&badge(decoratedFrame,D::RuneSchemaBadge)&&badge(decoratedFrame,D::ModdedBadge),"all three origin textures");
 const auto* favorite=badge(decoratedFrame,D::FavoriteBadge);check(favorite&&favorite->box.w==24&&favorite->box.h==24&&favorite->color[3]==1,"active favorite badge size and alpha");
 check(favorite&&!badgeShade(decoratedFrame,*favorite),"favorite badge has no backing rectangle");
 const auto* sourceBadge=badge(decoratedFrame,D::DragonwildsBadge);check(sourceBadge&&!badgeShade(decoratedFrame,*sourceBadge),"source badge has no backing rectangle");
 auto stars=hits(decoratedFrame,"favorite");check(std::abs(stars[0].box.w-36)<.001f&&stars[0].box.h==36,"comfortable favorite hitbox");
 bool masterEdge=false,questBadge=false;for(const auto& d:decoratedFrame.draws){if(d.kind==Draw::Kind::Rectangle&&d.color[0]==.35f&&d.color[1]==.78f)masterEdge=true;if(d.kind==Draw::Kind::Badge&&d.fallback=="Q")questBadge=true;}check(masterEdge&&questBadge,"masterwork frame and quest marker");
 auto frame=decorated.Render();auto star=hits(frame,"favorite")[0];decorated.Click(frame,star.box.x+18,star.box.y+18);check(!decorated.IsFavorite(item(0).path)&&decorated.selection.empty(),"remove favorite independent of spawn selection");
 for(const auto& d:decorated.Render().draws)if(d.kind==Draw::Kind::Badge&&d.text==D::FavoriteBadge){check(d.color[3]==.38f,"inactive star subdued");break;}
 decorated.favoritesWritable=false;check(text(decorated.Render(),"Favorites: session only"),"save failure stays visible");decorated.cloneSourcePicker=true;check(text(decorated.Render(),"Favorites are session-only"),"save failure in picker");
 dump(decoratedFrame,"v8-items.draws");dump(sourceFrame,"v8-source.draws");dump(appearanceFrame,"v8-appearance.draws");dump(favFrame,"v8-favorites.draws");dump(ghostFrame,"v8-unavailable.draws");
 bounds(decoratedFrame);bounds(sourceFrame);bounds(appearanceFrame);bounds(favFrame);bounds(ghostFrame);
 for(int n=0;n<=73;++n){auto sample=model(n);std::set<std::string> all;for(int p=0;p<=D::LastPage(n,12);++p){sample.scroll[0]=p;for(const auto& h:hits(sample.Render(),"toggle-item"))check(all.insert(h.arg).second,"boundary page not duplicated");}check(all.size()==std::size_t(n),"boundary census reachable");}
 auto shrinking=model(50);shrinking.Activate("scroll-end");auto reduced=shrinking.catalog;reduced.entries[0].resize(2);shrinking.Update(reduced);check(shrinking.scroll[0]==0,"catalog shrink clamps page");
 std::cout<<"PASS: "<<checks<<" F2 browser, badge, favorite and pagination assertions.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
