#include "Generator/ModMenuControls.h"
#include <iostream>
#include <stdexcept>
int main()try{
 ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={700,600};io.DeltaTime=1.f/60;io.Fonts->Build();io.ConfigFlags|=ImGuiConfigFlags_NavEnableGamepad;io.BackendFlags|=ImGuiBackendFlags_HasGamepad;
 double value=.5;nlohmann::json selected="A",next=selected,options={"A","B","C"};ImGuiID dial=0,radialB=0,red=0;ImVec2 center;ImVec4 radialRect{},colorRect{};std::string color="#33669980";
 const auto frame=[&]{ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({700,600});ImGui::Begin("Controls");PS::ModMenuControls::Dial("dial",value,0,1,.01,2);dial=ImGui::GetItemID();const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();center={(a.x+b.x)*.5f,(a.y+b.y)*.5f};if(PS::ModMenuControls::Radial("choice",options,selected,next))selected=next;
 const auto radialA=ImGui::GetItemRectMin(),radialZ=ImGui::GetItemRectMax();radialRect={radialA.x,radialA.y,radialZ.x,radialZ.y};ImGui::PushID("choice");ImGui::PushID(1);radialB=ImGui::GetID("option");ImGui::PopID();ImGui::PopID();
 PS::ModMenuControls::Color("color",color,true);const auto colorA=ImGui::GetItemRectMin(),colorZ=ImGui::GetItemRectMax();colorRect={colorA.x,colorA.y,colorZ.x,colorZ.y};ImGui::PushID("color");red=ImGui::GetID("##X");ImGui::PopID();ImGui::End();ImGui::Render();};
 frame();frame();auto* window=ImGui::FindWindowByName("Controls");ImGui::FocusWindow(window);ImGui::SetFocusID(dial,window);ImGui::SetNavCursorVisible(true);ImGui::GetCurrentContext()->NavInputSource=ImGuiInputSource_Gamepad;
 io.AddKeyEvent(ImGuiKey_GamepadFaceDown,true);frame();io.AddKeyEvent(ImGuiKey_GamepadFaceDown,false);frame();io.AddKeyEvent(ImGuiKey_GamepadDpadRight,true);for(int i=0;i<12;++i)frame();io.AddKeyEvent(ImGuiKey_GamepadDpadRight,false);frame();
 if(!(value>.5&&value<=1))throw std::runtime_error("Controller could not adjust circular numeric control");
 io.AddKeyEvent(ImGuiKey_GamepadFaceRight,true);frame();io.AddKeyEvent(ImGuiKey_GamepadFaceRight,false);frame();
 const auto before=value;io.AddMousePosEvent(center.x,center.y);frame();io.AddMouseButtonEvent(0,true);frame();io.AddMousePosEvent(center.x-35,center.y);frame();io.AddMouseButtonEvent(0,false);frame();
 if(!(value<before&&value>=0))throw std::runtime_error("Pointer could not adjust circular numeric control");
 ImGui::ClearActiveID();ImGui::FocusWindow(window);ImGui::SetFocusID(radialB,window);ImGui::GetCurrentContext()->NavInputSource=ImGuiInputSource_Gamepad;ImGui::SetNavCursorVisible(true);
 io.AddKeyEvent(ImGuiKey_GamepadFaceDown,true);frame();io.AddKeyEvent(ImGuiKey_GamepadFaceDown,false);frame();if(selected!="B")throw std::runtime_error("Controller could not choose a radial option");
 const float diameter=radialRect.z-radialRect.x,angle=-std::numbers::pi_v<float>*.5f+4*std::numbers::pi_v<float>/3;
 const ImVec2 point{(radialRect.x+radialRect.z)*.5f+std::cos(angle)*diameter*.355f,(radialRect.y+radialRect.w)*.5f+std::sin(angle)*diameter*.355f};
 io.AddMousePosEvent(point.x,point.y);frame();io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();if(selected!="C")throw std::runtime_error("Pointer could not choose a radial option");
 const auto oldColor=color;ImGui::ClearActiveID();ImGui::FocusWindow(window);ImGui::SetFocusID(red,window);ImGui::GetCurrentContext()->NavInputSource=ImGuiInputSource_Gamepad;ImGui::SetNavCursorVisible(true);
 io.AddKeyEvent(ImGuiKey_GamepadFaceDown,true);frame();io.AddKeyEvent(ImGuiKey_GamepadFaceDown,false);frame();io.AddKeyEvent(ImGuiKey_GamepadDpadRight,true);for(int i=0;i<12;++i)frame();io.AddKeyEvent(ImGuiKey_GamepadDpadRight,false);frame();
 if(color==oldColor||!PS::ModMenuColor::Valid(color,true))throw std::runtime_error("Controller could not adjust a color component");
 io.AddKeyEvent(ImGuiKey_GamepadFaceRight,true);frame();io.AddKeyEvent(ImGuiKey_GamepadFaceRight,false);frame();
 const auto beforePointerColor=color;const ImVec2 channel{colorRect.x+15,(colorRect.y+colorRect.w)*.5f};io.AddMousePosEvent(channel.x,channel.y);frame();io.AddMouseButtonEvent(0,true);frame();io.AddMousePosEvent(channel.x+35,channel.y);frame();io.AddMouseButtonEvent(0,false);frame();
 if(color==beforePointerColor||!PS::ModMenuColor::Valid(color,true))throw std::runtime_error("Pointer could not adjust a color component");
 if(!ImGui::GetDrawData()||ImGui::GetDrawData()->TotalVtxCount==0)throw std::runtime_error("Circular controls produced no visible rendering");
 ImGui::DestroyContext();std::cout<<"Dial, radial choice and RGBA editing by pointer/controller and smooth rendering passed.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
