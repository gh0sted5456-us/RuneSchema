#include "Generator/HelpyMenu.h"
#include <iostream>
#include <stdexcept>
namespace PS::HelpyControllerDevice {Reading Read(bool){return {};}void Shutdown(){}}
int main()try{
    using namespace PS::HelpyNavigation;
    static_assert(Visible(Page::Items)&&!Visible(Page::Creation)&&!Visible(Page::Recipes));
    static_assert(Visible(Page::Creation,Workspaces{.Clone=true})&&!Visible(Page::Recipes,Workspaces{.Clone=true}));
    static_assert(Visible(Page::Recipes,Workspaces{.Recipes=true})&&!Visible(Page::Creation,Workspaces{.Recipes=true}));
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.DisplaySize={700,450};io.DeltaTime=1.f/60;io.IniFilename=nullptr;io.Fonts->Build();
    io.ConfigFlags|=ImGuiConfigFlags_NavEnableGamepad|ImGuiConfigFlags_NavEnableKeyboard;io.BackendFlags|=ImGuiBackendFlags_HasGamepad;PS::HelpyControllerInput::Selection.Controller=true;
    PS::HelpyImGuiTheme::ControllerNavigation=true;
    int mode=0,selected=0;bool toggled=false;constexpr std::array labels{"First","Second"};ImGuiID first=0,second=0;ImGuiWindow* body=nullptr;
    const auto frame=[&]{
        ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({700,450});ImGui::Begin("Fixture");
        if(mode==0)PS::HelpyMenu::Categories("source",labels,selected);else PS::HelpyMenu::Tabs("source",labels,selected,mode!=2);
        if(mode==2||PS::HelpyMenu::BeginBody("destination",selected)){body=ImGui::GetCurrentWindow();ImGui::Checkbox("First control",&toggled);first=ImGui::GetItemID();ImGui::Button("Second control");second=ImGui::GetItemID();}
        if(mode!=2)PS::HelpyMenu::EndBody();ImGui::End();PS::HelpyControllerInput::ResolvePaneNavigation();ImGui::Render();
    };
    const auto press=[&](ImGuiKey key){io.AddKeyEvent(key,true);frame();io.AddKeyEvent(key,false);frame();frame();};
    const auto focusSource=[&]{const auto focus=PS::HelpyPreviewHooks::MenuFocus.at("source/Second");auto* window=ImGui::FindWindowByID(focus.second);const auto rect=PS::HelpyPreviewHooks::MenuTabs.at("source/Second");ImGui::FocusWindow(window);ImGui::SetFocusID(focus.first,window);window->NavRectRel[0]=ImRect({rect.x-window->Pos.x,rect.y-window->Pos.y},{rect.z-window->Pos.x,rect.w-window->Pos.y});ImGui::GetCurrentContext()->NavInputSource=ImGuiInputSource_Gamepad;ImGui::SetNavCursorVisible(true);};
    for(mode=0;mode<2;++mode){
        selected=0;ImGui::ClearActiveID();frame();frame();focusSource();
        press(mode==0?ImGuiKey_GamepadDpadRight:ImGuiKey_GamepadDpadDown);
        if(selected!=0||PS::HelpyControllerInput::InSection()&&ImGui::GetCurrentContext()->NavWindow->RootWindowForNav==body)throw std::runtime_error("D-pad entered another section without Confirm");
        focusSource();press(ImGuiKey_GamepadFaceDown);
        auto& nav=*ImGui::GetCurrentContext();if(selected!=1||!nav.NavId||nav.NavWindow->RootWindowForNav!=body||toggled)throw std::runtime_error("Confirm failed to enter the selected section without editing it");
        for(int i=0;i<3;++i)press(ImGuiKey_GamepadDpadUp);
        if(nav.NavWindow->RootWindowForNav!=body||nav.NavId!=first)throw std::runtime_error("D-pad escaped the contained content at its edge");
        press(ImGuiKey_GamepadDpadDown);if(nav.NavId!=second)throw std::runtime_error("D-pad could not navigate inside the section");
        press(ImGuiKey_GamepadFaceRight);if(nav.NavWindow->RootWindowForNav==body)throw std::runtime_error("Back did not return to the parent section");
        press(ImGuiKey_GamepadFaceDown);if(nav.NavWindow->RootWindowForNav!=body||nav.NavId!=second)throw std::runtime_error("Re-entering did not restore the last focused control");
        if(mode==1){press(ImGuiKey_GamepadR1);if(selected!=1)throw std::runtime_error("Bumper unexpectedly changed a page tab");}
    }
    mode=2;ImGui::ClearActiveID();frame();frame();focusSource();press(ImGuiKey_GamepadFaceDown);
    if(GImGui->NavId!=first||toggled||PS::HelpyImGuiTheme::EnterNextSection)throw std::runtime_error("Inline tabs did not enter their controls without leaking a child-section request");
    mode=1;
    PS::HelpyImGuiTheme::ControllerNavigation=false;frame();
    if(!(body->ChildFlags&ImGuiChildFlags_NavFlattened))throw std::runtime_error("Pointer mode retained controller containment");
    PS::HelpyImGuiTheme::ControllerNavigation=true;
    ImGuiWindow* panes[3]{};ImGuiID controls[3]{},quantityId=0;int quantity=1;bool hideResults=false;
    const auto moduleFrame=[&]{
        ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({700,450});ImGui::Begin("Controller modules");
        for(int i=0;i<3;++i){if(i==1&&hideResults)continue;if(i)ImGui::SameLine();ImGui::PushID(i);
            PS::HelpyImGuiTheme::BeginPanel("panel",{190,300},true,0,true);panes[i]=ImGui::GetCurrentWindow();
            ImGui::Button(i==0?"Browse":i==1?"Results":"Cart");controls[i]=ImGui::GetItemID();
            if(i==2){ImGui::DragInt("Quantity",&quantity,1,1,10000,"%d",ImGuiSliderFlags_AlwaysClamp);quantityId=ImGui::GetItemID();}
            PS::HelpyImGuiTheme::EndPanel();ImGui::PopID();
        }
        ImGui::End();PS::HelpyControllerInput::ResolvePaneNavigation();ImGui::Render();
    };
    moduleFrame();moduleFrame();ImGui::FocusWindow(panes[0]);ImGui::SetFocusID(controls[0],panes[0]);
    const auto modulePress=[&](ImGuiKey key){io.AddKeyEvent(key,true);moduleFrame();io.AddKeyEvent(key,false);moduleFrame();moduleFrame();};
    modulePress(ImGuiKey_GamepadR1);if(GImGui->NavWindow!=panes[1])throw std::runtime_error("Bumper did not enter results");
    for(int i=0;i<3;++i)modulePress(ImGuiKey_GamepadDpadDown);
    if(GImGui->NavWindow!=panes[1])throw std::runtime_error("D-pad escaped results");
    modulePress(ImGuiKey_GamepadR1);if(GImGui->NavWindow!=panes[2])throw std::runtime_error("Bumper did not enter cart");
    modulePress(ImGuiKey_GamepadDpadDown);modulePress(ImGuiKey_GamepadFaceDown);modulePress(ImGuiKey_GamepadDpadRight);
    if(quantity<=1){std::cerr<<"quantity nav="<<GImGui->NavId<<" expected="<<quantityId<<" active="<<GImGui->ActiveId<<" window="<<(GImGui->NavWindow?GImGui->NavWindow->Name:"none")<<"\n";throw std::runtime_error("Controller could not increase cart quantity");}
    const int before=quantity;modulePress(ImGuiKey_GamepadDpadLeft);if(quantity>=before||quantity<1)throw std::runtime_error("Controller could not decrease cart quantity");
    modulePress(ImGuiKey_GamepadR1);if(GImGui->NavWindow!=panes[2])throw std::runtime_error("Bumper stole an active quantity editor");
    modulePress(ImGuiKey_GamepadFaceDown);modulePress(ImGuiKey_GamepadR1);if(GImGui->NavWindow!=panes[0])throw std::runtime_error("Panel cycle did not wrap after editing");
    modulePress(ImGuiKey_GamepadL1);if(GImGui->NavWindow!=panes[2])throw std::runtime_error("Reverse panel cycle failed");
    modulePress(ImGuiKey_GamepadR1);io.AddKeyEvent(ImGuiKey_GamepadR1,true);moduleFrame();hideResults=true;
    io.AddKeyEvent(ImGuiKey_GamepadR1,false);moduleFrame();moduleFrame();hideResults=false;moduleFrame();
    if(GImGui->NavWindow==panes[1])throw std::runtime_error("A hidden destination retained a stale controller focus request");
    PS::HelpyImGuiTheme::ControllerNavigation=false;
    const auto modalFrame=[&](ImVec2 size,bool open=false){
        io.DisplaySize=size;ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize(size);ImGui::Begin("Modal resize parent");
        if(open)ImGui::OpenPopup("Resize modal");ImGui::SetNextWindowSize({560,360},ImGuiCond_Appearing);
        if(PS::HelpyImGuiTheme::BeginCenteredModal("Resize modal",nullptr,ImGuiWindowFlags_None)){ImGui::TextUnformatted("A modal must remain reachable after resizing");ImGui::Button("Close fixture");ImGui::EndPopup();}
        ImGui::End();ImGui::Render();
    };
    modalFrame({1600,900},true);modalFrame({1600,900});auto* popup=ImGui::FindWindowByName("Resize modal");
    if(!popup)throw std::runtime_error("Resize modal fixture did not open");ImGui::SetWindowPos(popup,{940,480},ImGuiCond_Always);modalFrame({1600,900});
    for(int i=0;i<3;++i)modalFrame({640,480});
    if(popup->Pos.x<0||popup->Pos.y<0||popup->Pos.x+popup->Size.x>640||popup->Pos.y+popup->Size.y>480)throw std::runtime_error("Modal left its controls outside the resized viewport");
    ImGui::ClosePopupToLevel(0,true);
    const auto autoModalFrame=[&](ImVec2 display,bool open,bool constrained){
        io.DisplaySize=display;ImGui::NewFrame();ImGui::Begin("Auto modal parent");if(open)ImGui::OpenPopup("Auto modal");
        ImGui::SetNextWindowSize({390,0},ImGuiCond_Appearing);
        if(constrained)ImGui::SetNextWindowSizeConstraints({320,0},{320,FLT_MAX});
        if(PS::HelpyImGuiTheme::BeginCenteredModal("Auto modal",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
            ImGui::Dummy({ImGui::GetContentRegionAvail().x,100});ImGui::SetNextItemWidth(-1);ImGui::InputInt("##quantity",&quantity);ImGui::EndPopup();}
        ImGui::End();ImGui::Render();
    };
    autoModalFrame({1600,900},true,false);for(int i=0;i<90;++i)autoModalFrame({1600,900},false,false);
    auto* automatic=ImGui::FindWindowByName("Auto modal");if(!automatic||std::abs(automatic->Size.x-390)>1)throw std::runtime_error("Auto modal width grew across frames");
    for(int i=0;i<10;++i)autoModalFrame({300,480},false,false);
    if(automatic->Size.x>300)throw std::runtime_error("Auto modal did not fit a small viewport");
    for(int i=0;i<10;++i)autoModalFrame({1600,900},false,true);
    if(std::abs(automatic->Size.x-320)>1)throw std::runtime_error("Centered modal overwrote caller width constraints");
    ImGui::ClosePopupToLevel(0,true);
    ImGui::DestroyContext();std::cout<<"Contained D-pad navigation, Confirm entry, hierarchical Back, focus restoration, pointer mode and modal resizing passed.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
