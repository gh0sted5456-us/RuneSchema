#pragma once
#include <algorithm>
namespace PS::DesktopBayLayout {
struct Rect {float X=0,Y=0,Width=0,Height=0;};
struct Canvas {Rect Main,Bay,Cache,Map,Recipes,Runtime,Party,Inventory;float X=0,Y=0,Width=0,Height=0;};
inline Canvas Arrange(Rect main,Rect bay,Rect cache,Rect map,Rect recipes,Rect runtime,Rect party,Rect inventory){
    Canvas result;result.X=std::min({main.X,bay.X,cache.X,map.X,recipes.X,runtime.X,party.X,inventory.X});result.Y=std::min({main.Y,bay.Y,cache.Y,map.Y,recipes.Y,runtime.Y,party.Y,inventory.Y});
    for(auto* rect:{&main,&bay,&cache,&map,&recipes,&runtime,&party,&inventory}){result.Width=std::max(result.Width,rect->X+rect->Width-result.X);result.Height=std::max(result.Height,rect->Y+rect->Height-result.Y);}
    for(auto* rect:{&main,&bay,&cache,&map,&recipes,&runtime,&party,&inventory}){rect->X-=result.X;rect->Y-=result.Y;}
    result.Main=main;result.Bay=bay;result.Cache=cache;result.Map=map;result.Recipes=recipes;result.Runtime=runtime;result.Party=party;result.Inventory=inventory;return result;
}
inline Canvas Arrange(Rect main,Rect bay){
    Canvas result;result.X=std::min(main.X,bay.X);result.Y=std::min(main.Y,bay.Y);
    result.Width=std::max(main.X+main.Width,bay.X+bay.Width)-result.X;
    result.Height=std::max(main.Y+main.Height,bay.Y+bay.Height)-result.Y;
    main.X-=result.X;main.Y-=result.Y;bay.X-=result.X;bay.Y-=result.Y;result.Main=main;result.Bay=bay;return result;
}
inline Canvas Arrange(Rect main,Rect bay,Rect cache){
    Canvas result;result.X=std::min({main.X,bay.X,cache.X});result.Y=std::min({main.Y,bay.Y,cache.Y});
    result.Width=std::max({main.X+main.Width,bay.X+bay.Width,cache.X+cache.Width})-result.X;
    result.Height=std::max({main.Y+main.Height,bay.Y+bay.Height,cache.Y+cache.Height})-result.Y;
    for(auto* rect:{&main,&bay,&cache}){rect->X-=result.X;rect->Y-=result.Y;}
    result.Main=main;result.Bay=bay;result.Cache=cache;return result;
}
inline Canvas Arrange(Rect main,Rect bay,Rect cache,Rect map,Rect recipes){
    Canvas result;result.X=std::min({main.X,bay.X,cache.X,map.X,recipes.X});result.Y=std::min({main.Y,bay.Y,cache.Y,map.Y,recipes.Y});
    for(auto* rect:{&main,&bay,&cache,&map,&recipes}){result.Width=std::max(result.Width,rect->X+rect->Width-result.X);result.Height=std::max(result.Height,rect->Y+rect->Height-result.Y);}
    for(auto* rect:{&main,&bay,&cache,&map,&recipes}){rect->X-=result.X;rect->Y-=result.Y;}
    result.Main=main;result.Bay=bay;result.Cache=cache;result.Map=map;result.Recipes=recipes;return result;
}
inline Canvas Arrange(Rect main,Rect bay,Rect cache,Rect map,Rect recipes,Rect runtime){
    Canvas result;result.X=std::min({main.X,bay.X,cache.X,map.X,recipes.X,runtime.X});result.Y=std::min({main.Y,bay.Y,cache.Y,map.Y,recipes.Y,runtime.Y});
    for(auto* rect:{&main,&bay,&cache,&map,&recipes,&runtime}){result.Width=std::max(result.Width,rect->X+rect->Width-result.X);result.Height=std::max(result.Height,rect->Y+rect->Height-result.Y);}
    for(auto* rect:{&main,&bay,&cache,&map,&recipes,&runtime}){rect->X-=result.X;rect->Y-=result.Y;}
    result.Main=main;result.Bay=bay;result.Cache=cache;result.Map=map;result.Recipes=recipes;result.Runtime=runtime;return result;
}
inline Canvas Arrange(Rect main,Rect bay,Rect cache,Rect map,Rect recipes,Rect runtime,Rect party){
    Canvas result;result.X=std::min({main.X,bay.X,cache.X,map.X,recipes.X,runtime.X,party.X});result.Y=std::min({main.Y,bay.Y,cache.Y,map.Y,recipes.Y,runtime.Y,party.Y});
    for(auto* rect:{&main,&bay,&cache,&map,&recipes,&runtime,&party}){result.Width=std::max(result.Width,rect->X+rect->Width-result.X);result.Height=std::max(result.Height,rect->Y+rect->Height-result.Y);}
    for(auto* rect:{&main,&bay,&cache,&map,&recipes,&runtime,&party}){rect->X-=result.X;rect->Y-=result.Y;}
    result.Main=main;result.Bay=bay;result.Cache=cache;result.Map=map;result.Recipes=recipes;result.Runtime=runtime;result.Party=party;return result;
}
}
