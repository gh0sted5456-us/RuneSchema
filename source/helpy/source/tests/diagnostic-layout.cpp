#include "Generator/DiagnosticsPanel.h"
#include <iostream>
#include <stdexcept>
namespace PS::HelpyControllerDevice {Reading Read(bool){return {};}void Shutdown(){}}
int main()try{
    using namespace PS;
    HelpyPaths::Configure(std::filesystem::temp_directory_path()/"diagnostic-layout/RuneSchema/plugins/RuneSchema.Helpy");
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DeltaTime=1.f/60;io.Fonts->Build();
    DiagnosticCenterModel::Store store;
    for(int i=0;i<1000;++i)store.Accept(LiveLog::Parse("[RuneSchema] [warning] Fixture issue "+std::to_string(i)));
    const auto frame=[&](ImVec2 size){
        io.DisplaySize=size;ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize(size);
        ImGui::Begin("Diagnostic resize fixture");HelpyPreviewHooks::DiagnosticRowsDrawn=0;
        DiagnosticsPanel::DrawIssues(store,HelpyImGuiTheme::Current);ImGui::End();ImGui::Render();
    };
    for(const ImVec2 size:{ImVec2{520,600},ImVec2{1100,700},ImVec2{520,420}}){
        for(int i=0;i<3;++i)frame(size);
        if(HelpyPreviewHooks::DiagnosticRowsDrawn>=40)throw std::runtime_error("Resized diagnostic list renders offscreen issues: "+std::to_string(HelpyPreviewHooks::DiagnosticRowsDrawn));
        ImGuiWindow* list=nullptr;
        for(auto* window:ImGui::GetCurrentContext()->Windows)if(window->Active&&(std::strstr(window->Name,"##diagnostic-narrow-list")||std::strstr(window->Name,"##diagnostic-list")))list=window;
        if(!list||list->ScrollMax.y<=0)throw std::runtime_error("Resized diagnostics lost its scrollable list");
        io.AddMousePosEvent(list->Pos.x+list->Size.x*.5f,list->Pos.y+list->Size.y*.5f);frame(size);frame(size);
        const auto before=list->Scroll.y;io.AddMouseWheelEvent(0,-4);frame(size);frame(size);
        if(list->Scroll.y<=before)throw std::runtime_error("Mouse wheel cannot scroll resized diagnostics");
    }
    ImGui::DestroyContext();std::cout<<"Diagnostic virtualization and wheel scrolling survive narrow/wide/narrow resize.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
