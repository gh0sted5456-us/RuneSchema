#!/usr/bin/env python3
"""B6E4 contract: inspector actions popup, mute dialog, scroll and arrow direction.

Syntax-compiles the production header action helpers against the vendored
ImGui declarations, using minimal fake model/theme declarations. This is not a
Windows application build or proof of in-game popup positioning.
"""
from __future__ import annotations
import pathlib
import re
import shutil
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[3]
SOURCE = ROOT / 'source/helpy/source/include/Generator/DiagnosticsPanel.h'
IMGUI = ROOT / 'source/helpy/vendor/imgui'
s = SOURCE.read_text(encoding='utf-8')

def extract(tag: str) -> str:
    match = re.search(r'inline\s+[^\n]+?\b'+re.escape(tag)+r'\s*\(',s)
    assert match, f'missing function {tag}'
    begin=s.index('{',match.start())
    depth=0
    for i in range(begin,len(s)):
        if s[i]=='{':depth+=1
        elif s[i]=='}':
            depth-=1
            if depth==0:return s[match.start():i+1]
    raise AssertionError('unbalanced function '+tag)

hdr=extract('DrawInspectorWindowHeader')
layout=s[s.index('struct InspectorPopupPlacement'):s.index('inline bool InspectorMenuItem')]
items=extract('InspectorMenuItem')
inspector=extract('DrawInspector')
issues=extract('DrawIssues')
nav=extract('ToolbarIconButton')
assert 'ImGui::BeginChild("##issue-inspector-window-header",ImVec2(0,headerHeight)' in hdr
assert 'ImGui::OpenPopup("##inspector-actions-menu")' in hdr
assert 'ImGui::BeginPopup("##inspector-actions-menu")' in hdr
assert hdr.count('ImGui::EndPopup()')>=2
assert '##actions-dropdown' in hdr
assert 'ImGui::BeginPopupModal("Mute notifications##inspector-actions"' in hdr
assert 'MuteManager->Add(group,PendingMuteScope,PendingMuteDuration)' in hdr
assert 'ImGui::BeginMenu(' not in hdr, 'nested flyout should not be used near narrow inspector edge'
assert 'ImGui::RadioButton("This issue only"' in hdr
assert 'ImGui::RadioButton("All issues in this category"' in hdr
assert 'ImGui::RadioButton("Until manually unmuted"' in hdr
assert 'PlaceInspectorActions(buttonA,buttonB' in hdr
assert 'SetNextWindowSizeConstraints' in hdr
assert 'theme.textDisabled' in items
for label in ('Acknowledge','Copy summary','Export issue','Mute notifications','Inspect evidence','Raw details'):
    assert label in hdr, f'missing action {label}'
assert 'RequestReport(store,ExportKind::Issue,group.Id)' in hdr
assert 'SelectedOccurrenceEvent=group.EvidenceIds.back();OpenOccurrences=true;' in hdr
assert 'OpenTechnical=true' in hdr
assert 'ImGui::MenuItem(caption.c_str(),nullptr,false,enabled)' in items
assert 'DrawInspectorWindowHeader(store,*group,theme)' in inspector
assert '##issue-inspector-scrollable-details' in inspector
assert 'ImGui::EndChild();' in inspector
assert '##issue-actions' not in inspector, 'old body-level action child still present'
assert '##issue-action-grid' not in inspector, 'old action table still present'
assert 'const float sign=direction<0?1.0f:-1.0f;' in nav
assert 'if(ToolbarIconButton("previous","Previous issue",-1,theme))' in issues
assert 'if(ToolbarIconButton("next","Next issue",1,theme))' in issues
assert 'if(ToolbarIconButton("maximize"' in issues
assert 'ImGui::SameLine();drawNavigation();' in issues[issues.index('if(ToolbarIconButton("maximize"'):]
for direction,expected in ((-1,-1),(1,1)):
    sign=1 if direction<0 else -1
    tip=-sign*.45
    tail=sign*.65
    assert (tip-tail)*expected>0
print('PASS: inspector header actions, evidence/mute flows, scroll containment and nav mapping')

prologue=r'''
#include <imgui.h>
#include <string>
#include <vector>
#include <algorithm>
#include <limits>
#include <cstdint>
namespace LiveLog {enum class Level {Error,Warning,Info}; const char* Label(Level);}
namespace DiagnosticCenterModel {
 struct Group { std::uint64_t Id{}; bool Acknowledged{}; LiveLog::Level Severity{};
     std::vector<std::uint64_t> EvidenceIds; std::string Category; };
 struct Store {void MarkViewed(std::uint64_t,bool);};
}
namespace DiagnosticCenterMutes {
 enum class Scope {Exact, Category}; enum class Duration {Session, Persistent};
 struct Manager { void Add(const DiagnosticCenterModel::Group&,Scope,Duration); };
}
namespace DiagnosticCenterReport {
 struct Options {bool IncludeEvidence=false;};
 std::string Issue(DiagnosticCenterModel::Store&,const DiagnosticCenterModel::Group&,const std::string&,const Options&);
}
namespace HelpyImGuiTheme {
 enum class Icon {Acknowledge,Copy,Export,Bell,LiveLog,Code};
 struct Theme {
    ImVec4 panel{},icon{},text{},textDisabled{},warning{};
    float X(float n) const{return n;}
    float Y(float n) const{return n;}
 };
 bool SoftButton(const char*,ImVec2);
 void DrawIcon(Icon,ImVec2,float,const ImVec4&);
}
namespace PS::DiagnosticsPanel {
 using DiagnosticCenterMutes::Manager;
 inline Manager* MuteManager=nullptr;
 inline bool Historical=false,OpenOccurrences=false,OpenTechnical=false;
 inline std::uint64_t SelectedOccurrenceEvent=0;
 inline std::string ArchivedSessionId;
 inline DiagnosticCenterMutes::Scope PendingMuteScope=DiagnosticCenterMutes::Scope::Exact;
 inline DiagnosticCenterMutes::Duration PendingMuteDuration=DiagnosticCenterMutes::Duration::Session;
 enum class ExportKind {Issue};
 void RequestReport(DiagnosticCenterModel::Store&,ExportKind,std::uint64_t);
 std::string Fitted(const char*,float);
 void DrawSeverityIcon(const ImVec2&,float,LiveLog::Level,const HelpyImGuiTheme::Theme&);
'''
code=prologue+layout+items+'\n'+hdr+'\n}\n'
with tempfile.TemporaryDirectory(prefix='b6e4-header-') as d:
    f=pathlib.Path(d)/'check.cpp';f.write_text(code)
    for compiler in ('g++','clang++'):
        exe=shutil.which(compiler)
        if not exe:continue
        cmd=[exe,'-std=c++20','-fsyntax-only','-I',str(IMGUI),str(f)]
        r=subprocess.run(cmd,capture_output=True,text=True)
        assert r.returncode==0,(compiler+' compiler syntax error:\n'+r.stderr)
        print('PASS:',compiler,'ImGui-native header/menu syntax')
