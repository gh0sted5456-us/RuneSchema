#!/usr/bin/env python3
"""B6E2: check centered X strokes, ring geometry and DPI-relative optical adjustment."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[2] / "helpy/source/include/Generator/DiagnosticsPanel.h").read_text()


def fn(signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for end in range(brace, len(source)):
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
            if depth == 0:
                return source[start:end + 1]
    raise ValueError("unclosed function")


color = fn("inline const ImVec4& SeverityColor")
draw = fn("inline void DrawSeverityIcon")
assert 'markCenter(center.x-size*0.022f,center.y-size*0.012f)' in draw
assert 'draw->AddCircle(center,r*0.92f,color,0,w)' in draw
assert draw.count('draw->AddLine(ImVec2(markCenter.') == 2

cpp = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
struct ImVec2 { float x,y; ImVec2(float a,float b):x(a),y(b){} };
struct ImVec4 { unsigned id; };
using ImU32=unsigned;
namespace LiveLog { enum class Level { Error,Warning,Diagnostic,Info }; }
namespace HelpyImGuiTheme {
struct Theme { ImVec4 error{11},warning{22},accent{33},info{44}; };
}
struct Segment { ImVec2 a,b; };
struct FakeDraw {
    std::vector<Segment> lines;
    ImVec2 circleCenter{0,0};
    float circleRadius=0;
    void AddCircle(ImVec2 c,float radius,unsigned,int,float){circleCenter=c;circleRadius=radius;}
    void AddLine(ImVec2 a,ImVec2 b,unsigned,float){lines.push_back({a,b});}
    void AddTriangle(ImVec2,ImVec2,ImVec2,unsigned,float){}
    void AddCircleFilled(ImVec2,float,unsigned,int){}
} g;
namespace ImGui {
FakeDraw* GetWindowDrawList(){return &g;}
unsigned GetColorU32(const ImVec4& color){return color.id;}
}
''' + color + '\n' + draw + r'''
int main(){
    HelpyImGuiTheme::Theme t;
    for(float size: {8.0f,12.0f,15.0f,18.0f,30.0f,45.0f,72.0f}){
        g=FakeDraw{};
        const ImVec2 c{129.25f,24.75f};
        DrawSeverityIcon(c,size,LiveLog::Level::Error,t);
        assert(g.lines.size()==2);
        assert(std::abs(g.circleCenter.x-c.x)<1.e-5f);
        assert(std::abs(g.circleCenter.y-c.y)<1.e-5f);
        assert(std::abs(g.circleRadius-size*.46f)<1.e-4f);
        const auto& a=g.lines[0];const auto& b=g.lines[1];
        const auto center1=ImVec2((a.a.x+a.b.x)/2,(a.a.y+a.b.y)/2);
        const auto center2=ImVec2((b.a.x+b.b.x)/2,(b.a.y+b.b.y)/2);
        assert(std::abs(center1.x-center2.x)<1.e-4f);
        assert(std::abs(center1.y-center2.y)<1.e-4f);
        // The same small visual alignment correction is applied to both strokes.
        assert(std::abs((c.x-center1.x)/size-.022f)<1.e-5f);
        assert(std::abs((c.y-center1.y)/size-.012f)<1.e-5f);
        // X ends remain symmetric and inside the circle at every scale.
        assert(std::abs(a.a.x-a.b.x)==std::abs(b.a.x-b.b.x));
        for(const Segment s: g.lines){
            for(const ImVec2 p:{s.a,s.b}){
                const float dx=p.x-c.x,dy=p.y-c.y;
                assert(std::sqrt(dx*dx+dy*dy)<g.circleRadius);
            }
        }
    }
    std::cout<<"PASS: X internal symmetry, optical centering and scale\n";
}
'''
with tempfile.TemporaryDirectory(prefix="diag-b6e2-") as directory:
    p=Path(directory)/"test.cpp"
    p.write_text(cpp)
    for cc in ("g++","clang++"):
        if shutil.which(cc):
            target=Path(directory)/(cc+"-run")
            subprocess.run([cc,"-std=c++17","-Wall","-Wextra","-Werror",str(p),"-o",str(target)],check=True)
            output=subprocess.run([str(target)],check=True,capture_output=True,text=True)
            print(f"{cc}: {output.stdout.strip()}")
print("PASS: B6E2 source and optical-centering checks")
