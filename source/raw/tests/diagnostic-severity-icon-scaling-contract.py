#!/usr/bin/env python3
"""Portable B6E check: severity icon DPI/density scaling and theme tint."""
from __future__ import annotations
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

SOURCE = Path(__file__).resolve().parents[2] / "helpy/source/include/Generator/DiagnosticsPanel.h"
content = SOURCE.read_text(encoding="utf-8")


def extract(name: str) -> str:
    pos = content.find(f"inline {name}")
    if pos < 0:
        raise AssertionError(f"Missing {name}")
    start = content.index("{", pos)
    depth = 0
    for i in range(start, len(content)):
        if content[i] == "{":
            depth += 1
        elif content[i] == "}":
            depth -= 1
            if depth == 0:
                return content[pos:i + 1]
    raise AssertionError(f"Unterminated {name}")


severity_color = extract("const ImVec4& SeverityColor")
severity_icon = extract("void DrawSeverityIcon")
metric = content[content.index("inline bool DrawMetricCard"):content.index("inline bool ToolbarIconButton")]
issue = content[content.index("inline void DrawIssueRow"):content.index("inline void DrawActivityRow")]
assert "iconSize=theme.X(18.0f)" in metric, "Overview icon must use uniform DPI scale"
assert "iconSize=theme.X(15.0f)" in issue, "Issue card icon must use uniform DPI scale"
assert "iconSize*0.5f+theme.Y(6.0f)" in metric, "Overview icon top-edge clearance is required"
assert "w=std::max(1.0f,size*0.11f)" in severity_icon, "Stroke thickness must track icon size"
assert "ImGui::GetColorU32(SeverityColor(level,theme))" in severity_icon

cpp = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
struct ImVec2 { float x,y; ImVec2(float a,float b):x(a),y(b){} };
struct ImVec4 { unsigned id; };
using ImU32 = unsigned;
namespace LiveLog { enum class Level { Error, Warning, Diagnostic, Info }; }
namespace HelpyImGuiTheme {
struct Theme {
    ImVec4 error{11}, warning{22}, accent{33}, info{44};
    float scale=1.0f, density=1.0f;
    float X(float n) const { return n*scale; }
    float Y(float n) const { return n*scale*density; }
};
}
struct FakeDraw {
    unsigned lastColor=0;
    float radius=0, thickness=0, minX=1.e8f, maxX=-1.e8f, minY=1.e8f, maxY=-1.e8f;
    void point(ImVec2 p){ minX=std::min(minX,p.x); maxX=std::max(maxX,p.x); minY=std::min(minY,p.y); maxY=std::max(maxY,p.y); }
    void AddCircle(ImVec2 c,float r,unsigned col,int,float w){lastColor=col;radius=r;thickness=w;point({c.x-r,c.y-r});point({c.x+r,c.y+r});}
    void AddLine(ImVec2 a,ImVec2 b,unsigned col,float w){lastColor=col;thickness=w;point(a);point(b);}
    void AddTriangle(ImVec2 a,ImVec2 b,ImVec2 c,unsigned col,float w){lastColor=col;thickness=w;point(a);point(b);point(c);}
    void AddCircleFilled(ImVec2 c,float r,unsigned col,int){lastColor=col;point({c.x-r,c.y-r});point({c.x+r,c.y+r});}
} g;
namespace ImGui { FakeDraw* GetWindowDrawList(){return &g;} unsigned GetColorU32(const ImVec4& t){return t.id;} }
''' + severity_color + '\n' + severity_icon + r'''
int main(){
    using Level=LiveLog::Level;
    for(float dpi:{0.65f,1.0f,1.5f,2.0f,3.0f}){
        for(float density:{0.65f,1.0f,1.35f}){
            HelpyImGuiTheme::Theme theme; theme.scale=dpi; theme.density=density;
            for(float logical:{15.0f,18.0f}){
                const float size=theme.X(logical);
                for(auto severity:{Level::Error,Level::Warning,Level::Diagnostic,Level::Info}){
                    g=FakeDraw{};
                    DrawSeverityIcon({0,0},size,severity,theme);
                    const unsigned expected=severity==Level::Error?11:severity==Level::Warning?22:severity==Level::Diagnostic?33:44;
                    assert(g.lastColor==expected);
                    assert(g.thickness>0);
                    assert(std::abs(g.thickness-std::max(1.0f,size*.11f))<1.e-4f);
                    assert(g.maxX>0 && g.minX<0 && g.maxY>0 && g.minY<0);
                    if(severity==Level::Error || severity==Level::Info || severity==Level::Diagnostic){
                        assert(std::abs((g.maxX-g.minX)-(g.maxY-g.minY))<1.e-4f);
                    }
                    // A change to row density must not change the icon's geometry.
                    const float expectedWidth=(severity==Level::Warning?1.72f*.5f:1.84f*.5f)*size;
                    assert(std::abs((g.maxX-g.minX)-expectedWidth)<1.e-3f);
                }
                // Metric icon always clears the top of its card at any density.
                const float centerY=std::max(theme.Y(18.0f),size*.5f+theme.Y(6.0f));
                assert(centerY-size*.5f>=0.0f);
            }
        }
    }
    std::cout<<"PASS: 5 scales x 3 densities x 2 icon sizes x 4 severities\n";
}
'''
compilers = [c for c in ("g++", "clang++") if shutil.which(c)]
if not compilers:
    raise SystemExit("No GCC/Clang available")
with tempfile.TemporaryDirectory(prefix="diagnostic-icons-") as d:
    src=Path(d)/"icons.cpp"
    src.write_text(cpp)
    for name in compilers:
        exe=Path(d)/name
        subprocess.run([name,"-std=c++17","-Wall","-Wextra","-Werror","-o",str(exe),str(src)],check=True)
        out=subprocess.run([str(exe)],check=True,capture_output=True,text=True)
        print(name+": "+out.stdout.strip())
print("PASS: source contract and proportional icon drawing")
