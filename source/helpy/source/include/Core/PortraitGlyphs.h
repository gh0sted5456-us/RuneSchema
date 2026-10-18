#pragma once
#include "Core/HelpyGlyphs.h"
namespace PS::PortraitGlyphs {
inline constexpr std::array<HelpyGlyphs::Glyph,4> All{{
    {"portrait-player","Player","M8 5 Q8 1 12 1 Q16 1 16 5 Q16 9 12 9 Q8 9 8 5 Z M12 9 L12 16 M4 13 L12 10 L20 13 M12 16 L6 24 M12 16 L18 24"},
    {"portrait-tree","Tree","M12 1 L3 16 L9 16 L9 24 M12 1 L21 16 L15 16 L15 24 M6 11 L18 11 M9 24 L15 24"},
    {"portrait-stone","Stone / ore","M2 24 L1 15 L6 5 L14 1 L21 8 L23 20 L18 24 Z M6 5 L11 15 L21 8 M11 15 L8 24"},
    {"portrait-enemy","Enemy","M9 5 Q9 2 12 2 Q15 2 15 5 Q15 8 12 8 Q9 8 9 5 Z M9 3 L6 0 M15 3 L18 0 M12 8 L12 16 M4 13 L12 10 L20 13 M12 16 L6 24 M12 16 L18 24 M20 8 L20 22 M20 8 L24 10 L24 14 L20 14"}
}};
// HelpyGlyphs::Replay caches only its own pack by address. Keep this separate
// pack's parsed geometry here rather than silently falling back to a map pin.
inline const std::vector<HelpyGlyphs::Segment>& Geometry(size_t kind){
    static const auto shapes=[](){std::array<std::vector<HelpyGlyphs::Segment>,4> result;
        struct Sink{std::vector<HelpyGlyphs::Segment>& Out;
            void Move(float x,float y){Out.push_back({'M',{x,y}});}void Line(float x,float y){Out.push_back({'L',{x,y}});}
            void Quad(float x,float y,float a,float b){Out.push_back({'Q',{x,y,a,b}});}void Cubic(float x,float y,float a,float b,float c,float d){Out.push_back({'C',{x,y,a,b,c,d}});}
            void Close(){Out.push_back({'Z'});}void Finish(){}
        };for(size_t i=0;i<All.size();++i){Sink sink{result[i]};HelpyGlyphs::Visit(All[i],sink);}return result;
    }();return shapes.at(kind);
}
template<class Sink> void Replay(size_t kind,Sink& sink){
    for(const auto& segment:Geometry(kind)){const auto& p=segment.Points;switch(segment.Kind){
        case 'M':sink.Move(p[0],p[1]);break;case 'L':sink.Line(p[0],p[1]);break;case 'Q':sink.Quad(p[0],p[1],p[2],p[3]);break;
        case 'C':sink.Cubic(p[0],p[1],p[2],p[3],p[4],p[5]);break;case 'Z':sink.Close();break;
    }}sink.Finish();
}
}
