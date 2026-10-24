#!/usr/bin/env python3
"""B6E4: compile and exercise production popup placement for viewport edges and DPI/density."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

source = Path(__file__).resolve().parents[2] / 'helpy/source/include/Generator/DiagnosticsPanel.h'
s = source.read_text(encoding='utf-8')
start = s.index('struct InspectorPopupPlacement')
end = s.index('inline bool InspectorMenuItem', start)
placement = s[start:end]
assert 'std::clamp(buttonMax.x-width' in placement
assert 'workPos.y+workSize.y' in placement
assert 'theme.X(258.0f)' in placement
assert 'std::min(theme.Y(245.0f)' in placement

cpp = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
struct ImVec2 { float x,y; ImVec2(float a=0,float b=0):x(a),y(b){} };
namespace HelpyImGuiTheme {
struct Theme {
    float scale=1.0f, density=1.0f;
    float X(float n) const {return n*scale;}
    float Y(float n) const {return n*scale*density;}
};
}
''' + placement + r'''
int main(){
    for(float scale:{0.65f,1.0f,1.5f,2.0f,3.0f}){
        for(float density:{0.7f,1.0f,1.35f}){
            HelpyImGuiTheme::Theme theme;theme.scale=scale;theme.density=density;
            for(ImVec2 work:{ImVec2(1300,760),ImVec2(640,420),ImVec2(280,190)}){
                const ImVec2 workPos{18.0f,25.0f};
                for(float horizontal:{0.0f,.5f,1.0f}){
                    for(float vertical:{0.0f,.5f,1.0f}){
                        const float x=workPos.x+work.x*horizontal;
                        const float y=workPos.y+work.y*vertical;
                        const auto p=PlaceInspectorActions(ImVec2(x-18.0f*scale,y-18.0f*scale),ImVec2(x,y),workPos,work,theme);
                        const float margin=std::min(theme.X(10.0f),work.x*.04f);
                        assert(p.Width>=1.0f);
                        assert(p.Width<=theme.X(258.0f)+.01f);
                        assert(p.Position.x>=workPos.x+margin-.01f);
                        assert(p.Position.x+p.Width<=workPos.x+work.x-margin+.01f);
                        assert(p.Position.y>=workPos.y-.01f);
                        assert(p.Position.y<=workPos.y+work.y+.01f);
                        const float menuHeight=std::min(theme.Y(245.0f),std::max(1.0f,work.y-theme.Y(12.0f)));
                        assert(p.Position.y+menuHeight<=workPos.y+work.y+.01f);
                    }
                }
            }
        }
    }
    std::cout<<"PASS: 5 scales x 3 densities x 3 viewport sizes x 9 anchor positions\n";
}
'''
with tempfile.TemporaryDirectory(prefix='b6e4-placement-') as directory:
    f=Path(directory)/'placement.cpp';f.write_text(cpp)
    for compiler in ('g++','clang++'):
        exe=shutil.which(compiler)
        if not exe: continue
        out=Path(directory)/(compiler.replace('+','p'))
        result=subprocess.run([exe,'-std=c++20','-Wall','-Wextra','-Werror',str(f),'-o',str(out)],capture_output=True,text=True)
        assert result.returncode==0,result.stderr
        run=subprocess.run([str(out)],capture_output=True,text=True)
        assert run.returncode==0,run.stderr
        print(compiler+': '+run.stdout.strip())
