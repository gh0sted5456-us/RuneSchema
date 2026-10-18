#pragma once
#include <array>
#include <string>
#include <string_view>
#include <cstdlib>
#include <stdexcept>
#include <vector>
namespace PS::HelpyGlyphs {
// Original 24-unit monoline artwork. This is the canonical source for both
// native ImGui drawing and exported SVGs; no font, raster atlas or web fetch.
struct Glyph {const char* Id;const char* Label;const char* Path;};
inline constexpr std::array<Glyph,64> All{{
 {"travel","Travel","M12 2 L20 20 L12 16 L4 20 Z M12 7 L12 16"},
 {"bosses","Bosses","M3 5 L6 18 L18 18 L21 5 L15 11 L12 3 L9 11 Z M7 21 L17 21"},
 {"dungeons","Dungeons","M4 21 L4 7 L7 7 L7 3 L10 3 L10 7 L14 7 L14 3 L17 3 L17 7 L20 7 L20 21 Z M9 21 L9 15 Q12 10 15 15 L15 21"},
 {"chests","Chests","M3 9 Q3 4 8 4 L16 4 Q21 4 21 9 L21 20 L3 20 Z M3 11 L21 11 M10 10 L14 10 L14 14 L10 14 Z M6 5 L6 20 M18 5 L18 20"},
 {"fishing","Fishing spots","M3 12 Q9 3 16 12 Q9 21 3 12 Z M16 12 L22 6 L22 18 Z M7 10 L7 11"},
 {"mining","Mining","M4 21 L16 5 M3 6 Q13 0 21 10 M14 4 L19 9"},
 {"trees","Trees","M12 2 L5 11 L8 11 L3 17 L10 17 L10 22 L14 22 L14 17 L21 17 L16 11 L19 11 Z"},
 {"runecrafting","Runecrafting","M12 2 L21 12 L12 22 L3 12 Z M9 8 L15 8 L9 16 L15 16 M12 6 L12 18"},
 {"plants","Plants","M12 22 L12 10 M12 16 Q2 15 3 5 Q12 5 12 16 M12 12 Q12 2 21 3 Q22 12 12 12"},
 {"quests","Quests","M7 7 C7 0 20 1 18 8 C17 11 12 10 12 15 M12 19 L12 21"},
 {"npcs","Characters","M8 7 C8 1 16 1 16 7 C16 13 8 13 8 7 Z M3 22 L3 19 C3 11 21 11 21 19 L21 22"},
 {"creatures","Creatures","M6 16 C6 10 18 10 18 16 C18 22 6 22 6 16 Z M4 10 L3 6 M9 7 L8 2 M15 7 L16 2 M20 10 L21 6"},
 {"lore","Lore and books","M12 6 Q6 2 2 5 L2 19 Q7 16 12 20 Q17 16 22 19 L22 5 Q18 2 12 6 L12 20 M5 8 L9 9 M15 9 L19 8"},
 {"places","Landmarks","M5 9 C5 0 19 0 19 9 C19 14 12 22 12 22 C12 22 5 14 5 9 Z M9 9 C9 5 15 5 15 9 C15 13 9 13 9 9 Z"},
 {"ore","Ore","M3 17 L5 8 L13 3 L21 10 L19 20 L9 22 Z M5 8 L12 11 L13 3 M12 11 L19 20 M12 11 L9 22 M12 11 L21 10"},
 {"stone","Stone","M2 16 L7 7 L17 5 L22 15 L17 21 L7 21 Z M7 7 L10 15 L2 16 M10 15 L17 21"},
 {"coal","Coal","M3 18 L5 9 L10 4 L18 6 L22 16 L16 21 L8 21 Z M5 9 L12 10 L18 6 M12 10 L16 21 M8 15 L12 10"},
 {"gem","Gem","M2 9 L7 3 L17 3 L22 9 L12 22 Z M2 9 L22 9 M7 3 L9 9 L12 22 L15 9 L17 3"},
 {"log","Log","M5 19 C0 19 0 11 5 11 C10 11 10 19 5 19 Z M5 11 L17 3 C22 2 24 9 20 11 L5 19 M12 9 L19 5 M13 14 L20 8"},
 {"stump","Stump","M5 9 C5 5 19 5 19 9 C19 13 5 13 5 9 Z M6 11 L6 18 L3 21 L10 20 L14 22 L21 20 L18 17 L18 11 M10 8 L14 10"},
 {"leaf","Leaf","M4 20 Q0 4 21 3 Q23 20 4 20 Z M4 20 L17 7 M9 15 L8 10 M13 11 L18 12"},
 {"herb","Herb","M12 22 L12 3 M12 8 Q3 10 4 3 Q11 1 12 8 M12 14 Q21 16 20 8 Q13 6 12 14 M12 20 Q3 20 4 13 Q10 12 12 20"},
 {"flower","Flower","M12 22 L12 13 M12 17 L18 14 M12 19 L6 16 M10 8 C4 8 5 1 10 4 C12 0 17 1 16 6 C22 8 19 13 15 11 C12 17 7 13 10 8 Z"},
 {"berry","Berries","M6 12 C2 12 2 19 6 19 C10 19 10 12 6 12 Z M16 12 C12 12 12 19 16 19 C20 19 20 12 16 12 Z M11 7 C7 7 7 14 11 14 C15 14 15 7 11 7 Z M11 7 L14 2 M13 5 Q20 0 20 6 Q17 9 13 5"},
 {"mushroom","Mushrooms","M2 12 C2 0 22 0 22 12 Z M9 12 L8 21 L16 21 L15 12 M6 8 L7 7 M13 5 L14 6 M18 8 L17 8"},
 {"crop","Crops","M12 22 L12 3 M12 9 L6 4 L6 10 L12 14 L18 10 L18 4 L12 9 M12 18 L4 13 L4 18 L12 22 L20 18 L20 13 L12 18"},
 {"fish","Fish","M2 12 Q9 2 17 12 Q9 22 2 12 Z M17 12 L22 7 L22 17 Z M8 6 L12 3 L13 7 M6 11 L6 12 M11 8 Q8 12 11 16"},
 {"water","Water","M12 2 C10 6 4 12 4 16 C4 24 20 24 20 16 C20 12 14 6 12 2 Z M8 16 Q8 20 12 20"},
 {"feather","Feathers","M3 22 L18 5 M6 18 Q2 5 18 2 Q24 12 9 18 M8 16 L8 10 M11 13 L17 13 M13 10 L13 6"},
 {"hide","Hide","M8 2 L16 2 L17 6 L22 8 L19 13 L21 20 L15 19 L12 22 L9 19 L3 20 L5 13 L2 8 L7 6 Z"},
 {"bone","Bones","M5 8 C0 8 1 1 5 3 C8 0 12 6 8 8 L16 16 C20 12 24 17 21 20 C22 24 15 24 16 20 Z"},
 {"meat","Meat","M4 5 C12 0 23 4 21 14 C20 22 9 24 4 18 C0 14 6 12 4 5 Z M9 7 Q16 4 17 12 Q16 19 9 17 Q13 12 9 7"},
 {"health","Health","M12 21 C9 18 1 13 2 7 C3 1 10 1 12 6 C14 1 21 1 22 7 C23 13 15 18 12 21 Z"},
 {"stamina","Stamina","M13 2 L4 14 L11 14 L9 22 L21 9 L14 9 L17 2 Z"},
 {"special","Special power","M12 1 L15 9 L23 12 L15 15 L12 23 L9 15 L1 12 L9 9 Z M12 8 L12 16"},
 {"hunger","Food","M3 2 L3 8 Q6 12 9 8 L9 2 M6 2 L6 22 M18 2 C12 7 14 12 18 12 L18 22 M18 2 L18 12"},
 {"thirst","Thirst","M5 3 L19 3 L17 21 L7 21 Z M6 10 Q9 7 12 10 Q15 13 18 10"},
 {"rest","Rest","M3 20 L3 6 M3 15 L21 15 L21 20 M3 11 L8 11 L8 15 M9 8 L19 8 Q21 8 21 11 L21 15 M12 2 L17 2 L12 6 L17 6"},
 {"power","Power level fallback","M12 2 L21 12 L12 22 L3 12 Z M9 16 L9 7 L13 7 Q18 10 13 12 L9 12"},
 {"shield","Defence","M3 5 L12 2 L21 5 L20 14 Q18 20 12 23 Q6 20 4 14 Z M12 3 L12 21"},
 {"sword","Sword","M3 21 L7 17 M4 14 L10 20 M7 17 L19 3 L22 2 L21 6 L10 19"},
 {"axe","Axe","M4 22 L17 3 M9 6 Q18 0 22 9 L17 13 L11 10"},
 {"pickaxe","Pickaxe","M5 22 L17 4 M3 8 Q14 0 22 10 M12 5 L17 10"},
 {"bow","Bow","M5 3 Q24 12 5 21 L5 3 M2 12 L22 12 M18 8 L22 12 L18 16"},
 {"staff","Staff","M6 23 L14 7 M11 6 L14 1 L20 2 L22 7 L17 11 L11 6 Z M14 4 L18 7"},
 {"helmet","Helmet","M4 20 L4 10 C4 0 20 0 20 10 L20 20 L14 22 L14 14 L10 14 L10 22 Z M5 10 L10 11 M14 11 L19 10"},
 {"body","Body armour","M8 2 L10 6 L14 6 L16 2 L22 7 L18 12 L17 22 L7 22 L6 12 L2 7 Z M12 7 L12 20"},
 {"legs","Leg armour","M5 2 L19 2 L20 21 L14 21 L12 9 L10 21 L4 21 Z M5 6 L19 6"},
 {"gloves","Gloves","M8 22 L4 16 L2 9 Q4 6 6 11 L7 12 L7 4 Q9 2 10 5 L10 11 L11 2 Q14 1 14 5 L14 11 L16 4 Q19 3 18 8 L17 14 L20 9 Q23 9 21 14 L17 22 Z"},
 {"boots","Boots","M5 2 L16 2 L15 13 L21 16 L21 21 L3 21 L3 16 L6 12 Z M4 17 L20 17"},
 {"party","Party","M9 7 C9 1 15 1 15 7 C15 12 9 12 9 7 Z M5 22 L5 18 C5 11 19 11 19 18 L19 22 M3 6 L3 10 M21 6 L21 10 M1 20 L1 16 L4 13 M23 20 L23 16 L20 13"},
 {"teleport","Teleport","M7 3 C0 3 0 21 7 21 C14 21 14 3 7 3 Z M8 12 L22 12 M17 7 L22 12 L17 17"},
 {"recall","Recall","M17 3 C24 3 24 21 17 21 C10 21 10 3 17 3 Z M16 12 L2 12 M7 7 L2 12 L7 17"},
 {"harvest","Harvest","M3 21 L15 9 M11 2 Q23 4 21 14 Q18 19 11 19 M8 14 L13 19"},
 {"timer","Timer","M12 5 C1 5 1 23 12 23 C23 23 23 5 12 5 Z M9 1 L15 1 M12 1 L12 5 M12 9 L12 14 L16 16 M19 4 L21 6"},
 {"respawn","Respawn","M20 8 C16 0 3 3 3 12 C3 22 17 24 21 16 M20 2 L20 8 L14 8 M8 14 L12 10 L16 14 M12 10 L12 19"},
 {"cache","Cache","M3 6 C3 1 21 1 21 6 C21 11 3 11 3 6 Z M3 6 L3 18 C3 23 21 23 21 18 L21 6 M3 12 C3 17 21 17 21 12"},
 {"refresh","Refresh","M20 8 C17 0 3 3 3 12 M20 2 L20 8 L14 8 M4 16 C7 24 21 21 21 12 M4 22 L4 16 L10 16"},
 {"search","Search","M3 10 C3 0 17 0 17 10 C17 20 3 20 3 10 Z M15 16 L22 23"},
 {"download","Download","M12 2 L12 16 M6 10 L12 16 L18 10 M3 16 L3 22 L21 22 L21 16"},
 {"import","Import","M14 3 L21 3 L21 21 L3 21 L3 14 M2 3 L12 13 M12 6 L12 13 L5 13"},
 {"export","Export","M14 3 L21 3 L21 10 M21 3 L11 13 M9 3 L3 3 L3 21 L21 21 L21 15"},
 {"equip","Equip","M8 3 L10 6 L14 6 L16 3 L21 7 L18 11 L17 20 L7 20 L6 11 L2 7 Z M9 13 L12 16 L18 10"},
 {"warning","Warning","M12 2 L23 21 L1 21 Z M12 8 L12 14 M12 18 L12 18.5"}
}};
inline const Glyph& Find(std::string_view id){for(const auto& glyph:All)if(id==glyph.Id)return glyph;return All[13];}
// Shared restricted SVG path interpreter. Paths are compile-time owned artwork,
// never user/server input. A malformed authored path fails tests, not silently.
template<class Sink> void Visit(const Glyph& glyph,Sink& sink){
    const char* p=glyph.Path;const auto number=[&](){while(*p==' '||*p==',')++p;char* end=nullptr;const float n=std::strtof(p,&end);if(end==p||n<0||n>24)throw std::runtime_error("Invalid glyph coordinate");p=end;return n;};
    while(*p){if(*p==' '){++p;continue;}const char command=*p++;switch(command){
        case 'M':{const float x=number(),y=number();sink.Move(x,y);break;}
        case 'L':{const float x=number(),y=number();sink.Line(x,y);break;}
        case 'Q':{const float x=number(),y=number(),a=number(),b=number();sink.Quad(x,y,a,b);break;}
        case 'C':{const float x=number(),y=number(),a=number(),b=number(),c=number(),d=number();sink.Cubic(x,y,a,b,c,d);break;}
        case 'Z':sink.Close();break;
        default:throw std::runtime_error("Unsupported glyph path command");
    }}sink.Finish();
}
struct Segment{char Kind;std::array<float,6> Points{};};
inline const std::vector<Segment>& Geometry(const Glyph& glyph){
    // Parse once for the entire pack, not once per icon/marker/frame.
    static const auto shapes=[](){std::array<std::vector<Segment>,All.size()> result;
        struct Sink{std::vector<Segment>& Out;
            void Move(float x,float y){Out.push_back({'M',{x,y}});}void Line(float x,float y){Out.push_back({'L',{x,y}});}
            void Quad(float x,float y,float a,float b){Out.push_back({'Q',{x,y,a,b}});}void Cubic(float x,float y,float a,float b,float c,float d){Out.push_back({'C',{x,y,a,b,c,d}});}
            void Close(){Out.push_back({'Z'});}void Finish(){}
        };for(size_t i=0;i<All.size();++i){Sink sink{result[i]};Visit(All[i],sink);}return result;
    }();
    for(size_t i=0;i<All.size();++i)if(&glyph==&All[i])return shapes[i];return shapes[13];
}
template<class Sink> void Replay(const Glyph& glyph,Sink& sink){
    for(const auto& segment:Geometry(glyph)){const auto& p=segment.Points;switch(segment.Kind){
        case 'M':sink.Move(p[0],p[1]);break;case 'L':sink.Line(p[0],p[1]);break;case 'Q':sink.Quad(p[0],p[1],p[2],p[3]);break;
        case 'C':sink.Cubic(p[0],p[1],p[2],p[3],p[4],p[5]);break;case 'Z':sink.Close();break;
    }}sink.Finish();
}
inline std::string Resource(std::string_view group,const std::string& lowerName){
    const auto has=[&](const char* s){return lowerName.find(s)!=std::string::npos;};
    if(group=="mining"){if(has("coal"))return "coal";if(has("gem"))return "gem";if(has("stone")||has("granite")||has("gypsum"))return "stone";return "ore";}
    if(group=="plants"){if(has("mushroom"))return "mushroom";if(has("berry"))return "berry";if(has("flower"))return "flower";if(has("herb")||has("marrentill")||has("guam"))return "herb";if(has("crop")||has("potato")||has("cabbage")||has("onion"))return "crop";return "plants";}
    if(group=="fishing")return "fish";return std::string(group);
}
}
