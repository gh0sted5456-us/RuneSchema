#include "Core/HelpyGlyphs.h"
#include "Core/PartyStats.h"
#include "Core/PartyStatWire.h"
#include "Core/MapTeleportRisk.h"
#include "Core/MapOceanMask.h"
#include "Core/PlayerPortraits.h"
#include "Core/SpawnPresentation.h"
#include "Core/JournalWorkspace.h"
#include "Core/PartyCardLayout.h"
#include "Core/JournalSections.h"
#include "Core/SpawnNames.h"
#include "Core/PortraitGlyphs.h"
#include <filesystem>
#include <fstream>
#include <set>
#include <iostream>
#include <limits>
static void Check(bool value){if(!value)throw std::runtime_error("Glyph/stat contract regression");}
struct Sink{unsigned Commands=0;void Move(float,float){++Commands;}void Line(float,float){++Commands;}void Quad(float,float,float,float){++Commands;}void Cubic(float,float,float,float,float,float){++Commands;}void Close(){++Commands;}void Finish(){}};
static std::string Svg(const PS::HelpyGlyphs::Glyph& glyph){return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 28 28\"><title>"+std::string(glyph.Label)+"</title><path transform=\"translate(2 2)\" d=\""+glyph.Path+"\" fill=\"none\" stroke=\"#ceb36b\" stroke-width=\"1.6\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/></svg>";}
int main(int argc,char** argv)try{
    using namespace PS;std::set<std::string> ids,paths;
    for(size_t i=0;i<PortraitGlyphs::All.size();++i){Sink sink;PortraitGlyphs::Replay(i,sink);Check(sink.Commands>5);const auto& geometry=PortraitGlyphs::Geometry(i);Check(geometry.front().Points[0]==(i==0?8:i==1?12:i==2?2:9));}
    for(float width:{1.f,80.f,180.f,360.f,720.f})for(size_t count=1;count<=32;++count){size_t total=0;const float side=std::min(width,26.f);for(const auto& line:PartyCardLayout::Centered(count,width,side,6)){
        const float used=line.Count*side+(line.Count-1)*6;Check(std::abs((2*line.Offset+used)-width)<.01f&&line.Offset>=0);total+=line.Count;}Check(total==count);}
    Check(JournalSections::For({{"Type","World"}})=="World"&&JournalSections::For({{"Type","Recipe"}})=="Recipes"&&JournalSections::For({{"Type","Lore"}})=="Knowledge"&&JournalSections::For({{"Name","Recipe-shaped rock"}}).empty());
    Check(SpawnNames::Display({{"Name","FellableTree Maple"},{"Type","Resource"}}).Name=="Maple Tree");
    Check(SpawnNames::Display({{"Name","FelledTree Ash Bare"},{"Type","Resource"}}).Name=="Ash Tree (Felled) (Bare)");
    Check(SpawnNames::Display({{"Name","Internal_Name"},{"DisplayName","A mod author's name"},{"Type","Resource"}}).Name=="A mod author's name");
    {
        const auto draft=JournalWorkspace::Draft("my_mod_lore","Lore","A native story",nlohmann::json::array({"Page one","Page two"}),"/Game/UI/Test.Test","/Game/UI/JournalData/Category.Category");
        Check(draft["my_mod_lore"]["Unlock"]==false&&draft["my_mod_lore"]["PageDescriptions"].size()==2&&!draft["my_mod_lore"].contains("PersistenceID"));
        for(const auto& bad:{"../bad","/Game/Existing.Entry","bad name"}){bool rejected=false;try{JournalWorkspace::Draft(bad,"Lore","Title",nlohmann::json::array({"Text"}),"","");}catch(...){rejected=true;}Check(rejected);}
        bool rejected=false;try{JournalWorkspace::Draft("mod_entry","Lore","Title",nlohmann::json::array({"Text"}),"https://wiki/image.png","");}catch(...){rejected=true;}Check(rejected);
        auto cache=JournalWorkspace::Cache(nlohmann::json::array({{{"Native",true},{"Path","/Game/Journal/Entry.Entry"},{"Pages",nlohmann::json::array({"Text"})}}}),"game-build-source",false);
        Check(JournalWorkspace::ReadCache(cache)["ScanComplete"]==false);
        cache["Entries"][0]["Pages"]=nlohmann::json::array({42});rejected=false;try{JournalWorkspace::ReadCache(cache);}catch(...){rejected=true;}Check(rejected);
    }
    for(const auto& glyph:HelpyGlyphs::All){Check(glyph.Id&&glyph.Label&&glyph.Path);try{Check(ids.insert(glyph.Id).second&&paths.insert(glyph.Path).second);Sink sink;HelpyGlyphs::Visit(glyph,sink);Check(sink.Commands>=3);Check(&HelpyGlyphs::Find(glyph.Id)==&glyph);}catch(const std::exception& error){throw std::runtime_error(std::string(glyph.Id)+": "+error.what());}}
    Check(ids.size()==64&&HelpyGlyphs::Resource("mining","coal")=="coal"&&HelpyGlyphs::Resource("plants","redberry bush")=="berry");
    Check(!SpawnPresentation::Enemy(nlohmann::json{{"Type","AI"},{"Available",true},{"Name","A projectile attack"}}));
    Check(SpawnPresentation::Enemy(nlohmann::json{{"Type","AI"},{"Available",true},{"SpawnableCharacter",true}}));
    {PlayerPortraits::Choices portraits;const std::string a="0123456789abcdef0123456789abcdef",b="1123456789abcdef0123456789abcdef";
        Check(portraits.Set(a,"/Game/UI/Icons/Tea.Tea")&&portraits.Get(a)=="/Game/UI/Icons/Tea.Tea"&&portraits.Get(b).empty());
        Check(!portraits.Set("a mutable name","/Game/x.x")&&!portraits.Set(a,"C:/file.png")&&!portraits.Set(a,"/Game/../evil.x"));
        Check(PlayerPortraits::Choices::Parse(portraits.Json()).Get(a)==portraits.Get(a));Check(portraits.Set(a,"")&&portraits.Get(a).empty());
        Check(PlayerPortraits::Identity("00000000000000000000000000000000").empty());
    }
    {std::vector<unsigned char> pixels(128*128*4,0);
        // Opaque land excludes its interior transparent lake from the sea flood.
        for(int y=32;y<96;++y)for(int x=32;x<96;++x)pixels[(y*128+x)*4+3]=255;
        pixels[(64*128+64)*4+3]=0;const auto sea=MapOceanMask::Mask::Build(pixels,128,128);
        Check(sea.Contains(.1,.1)&&!sea.Contains(.5,.5)&&!sea.Contains(.3,.3)&&!sea.Contains(1,1));
        Check(MapOceanMask::Mask::Build({},128,128).Runs.empty());
    }
    for(const auto& glyph:HelpyGlyphs::All){Sink a,b;HelpyGlyphs::Visit(glyph,a);HelpyGlyphs::Replay(glyph,b);Check(a.Commands==b.Commands&&HelpyGlyphs::Geometry(glyph).size()==a.Commands);}
    using Json=nlohmann::json;const int64_t now=100000;Json row={{"Stats",{{"Health",{{"Current",0},{"Maximum",100},{"ObservedUnixMs",now}}},{"PowerLevel",{{"Value",7},{"ObservedUnixMs",now}}}}}};
    Check(PartyStats::Read(row,"Health",now)->Fraction()==0&&PartyStats::Power(row,now)==7);Check(!PartyStats::Read(row,"Stamina",now));
    for(const auto bad:{-1.0,101.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){row["Stats"]["Health"]["Current"]=bad;Check(!PartyStats::Read(row,"Health",now));}
    row["Stats"]["Health"]["Current"]=50;Check(!PartyStats::Read(row,"Health",now+10001)&&!PartyStats::Power(row,now-1));
    row["Stats"]["Health"]["Maximum"]=0;Check(!PartyStats::Read(row,"Health",now));
    row["Stats"]["Health"]={{"Current",73},{"Maximum",100},{"ObservedUnixMs",now-100}};
    auto wire=PartyStatWire::Encode(row["Stats"],now);auto decoded=PartyStatWire::Decode(wire,9000000,500);
    Check(PartyStats::Read(Json{{"Stats",decoded}},"Health",9000000)->Current==73);
    Check(decoded["Health"]["ObservedUnixMs"]==8999400); // host/client clock skew irrelevant
    for(const char* key:{"Hunger","Thirst","RestLevel"})row["Stats"][key]={{"Current",50.0},{"Maximum",100.0},{"ObservedUnixMs",now}};
    const auto survival=PartyStatWire::Decode(PartyStatWire::Encode(row["Stats"],now),9000000,0);
    for(const char* key:{"Hunger","Thirst","RestLevel"})Check(PartyStats::Read(Json{{"Stats",survival}},key,9000000)->Fraction()==.5);
    Json risk={{"ObservedUnixMs",now},{"LandingFootprintChecked",true},{"LandingRisk","lower"}};
    Check(MapTeleportRisk::Read(risk,now)==MapTeleportRisk::Level::Lower&&MapTeleportRisk::Read(risk,now+10001)==MapTeleportRisk::Level::Unknown);
    risk["LandingRisk"]="higher";Check(MapTeleportRisk::Read(risk,now)==MapTeleportRisk::Level::Higher);
    risk["LandingFootprintChecked"]=false;Check(MapTeleportRisk::Read(risk,now)==MapTeleportRisk::Level::Unknown);
    wire["Values"]["Health"]["AgeMs"]=-1;wire["Values"]["Health"]["ObservedUnixMs"]=9000000;
    Check(!PartyStatWire::Decode(wire,9000000,0).contains("Health"));
    Check(PartyStatWire::Decode(wire,9000000,4001).empty());
    auto map=Json::array({Json{{"Guid","abc"},{"Path","/live/pawn"},{"Location",{1,2,3}}}});
    auto roster=Json::array({Json{{"Guid","abc"},{"Stats",decoded},{"Path","do not copy"}}});
    PartyStatWire::Merge(map,roster,9000000);Check(map[0]["Path"]=="/live/pawn"&&map[0]["Stats"]["Health"]["Current"]==73);
    map[0].erase("Stats");roster.push_back(roster[0]);PartyStatWire::Merge(map,roster,9000000);Check(!map[0].contains("Stats"));
    if(argc==2){
        const std::filesystem::path folder=argv[1];std::filesystem::create_directories(folder);
        std::ofstream gallery(folder/"index.html");gallery<<"<!doctype html><meta charset=\"utf-8\"><title>Helpy original glyphs</title><style>body{background:#101113;color:#ded9c9;font:14px system-ui;margin:32px}main{display:grid;grid-template-columns:repeat(auto-fit,minmax(130px,1fr));gap:12px}figure{margin:0;padding:16px;text-align:center;background:#191a1c;border:1px solid #393528;border-radius:8px}img{width:48px;height:48px}figcaption{margin-top:12px}p{color:#b7ab8d}</style><h1>Helpy · original glyphs</h1><p>64 original scalable vectors · RuneSchema theme tint · MIT · not game artwork. The game's native power-level badge remains separate.</p><main>";
        std::ofstream sheet(folder/"atlas.svg");sheet<<"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1200\" height=\"1020\" viewBox=\"0 0 1200 1020\"><rect width=\"1200\" height=\"1020\" fill=\"#101113\"/><text x=\"32\" y=\"40\" fill=\"#e7dfcb\" font-family=\"sans-serif\" font-size=\"25\">Helpy — 64 original glyphs</text>";
        size_t i=0;for(const auto& glyph:HelpyGlyphs::All){std::ofstream file(folder/(std::string(glyph.Id)+".svg"));file<<Svg(glyph);gallery<<"<figure><img src=\""<<glyph.Id<<".svg\" alt=\"\"><figcaption>"<<glyph.Label<<"</figcaption></figure>";
            const auto x=static_cast<int>(i%8)*146+16,y=static_cast<int>(i/8)*115+65;
            sheet<<"<rect x=\""<<x<<"\" y=\""<<y<<"\" width=\"138\" height=\"107\" rx=\"6\" fill=\"#191a1c\" stroke=\"#393528\"/><path transform=\"translate("<<x+45<<" "<<y+12<<") scale(2)\" d=\""<<glyph.Path<<"\" fill=\"none\" stroke=\"#ceb36b\" stroke-width=\"1.6\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/><text x=\""<<x+69<<"\" y=\""<<y+88<<"\" fill=\"#ded9c9\" font-family=\"sans-serif\" font-size=\"12\" text-anchor=\"middle\">"<<glyph.Label<<"</text>";++i;
        }gallery<<"</main>";sheet<<"</svg>";
    }std::cout<<"64 original glyphs and party-stat validity checks passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
