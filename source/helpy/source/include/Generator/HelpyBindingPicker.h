#pragma once
#include "InputBindings.h"
#include "Generator/HelpyMenu.h"
#include <functional>

namespace PS::HelpyBindingPicker {
using Keys=InputBindings::Keys;
inline ImGuiID Owner=0;
inline Keys DraftKeys,RecordedKeys;
inline uint32_t DraftPad=0,RecordedPad=0;
inline int Device=0;
inline bool Listening=false,WaitNeutral=true,SawInput=false,JustRecorded=false;
inline std::string Error,OriginalKeys,OriginalPad;
inline HelpyControllerDevice::Reading Reading;
inline uint64_t NextRead=0,GuideUntil=0;
inline auto ReadKey=+[](int key){return (GetAsyncKeyState(key)&0x8000)!=0;};
inline bool Active(){return Owner!=0;}
inline bool Recording(){return Active()&&Listening;}
inline void Cancel(){Owner=0;Listening=false;DraftKeys.clear();RecordedKeys.clear();DraftPad=RecordedPad=0;GuideUntil=0;Error.clear();}
inline bool NativeGuide(){if(!Active())return false;if(Listening&&Device==1)GuideUntil=GetTickCount64()+120;return true;}
inline uint32_t Pad(const HelpyControllerDevice::Reading& value){return uint32_t(value.Pad.wButtons)|(value.Guide?InputBindings::Guide:0)|(value.Pad.bLeftTrigger>100?InputBindings::LeftTrigger:0)|(value.Pad.bRightTrigger>100?InputBindings::RightTrigger:0)|(value.Touch.Clicked?InputBindings::Touchpad:0);}
inline void Tick(bool allowed=true){
    if(!Active())return;
    if(ImGui::GetCurrentContext()->OpenPopupStack.empty()){Cancel();return;}
    const auto now=GetTickCount64();if(now<NextRead)return;NextRead=now+25;
    if(!allowed){WaitNeutral=true;SawInput=false;RecordedKeys.clear();RecordedPad=0;return;}
    Reading=HelpyControllerInput::DeviceRead(false);
    if(!Listening)return;
    if(ReadKey(VK_ESCAPE)||(Pad(Reading)&0x30)==0x30){Listening=false;RecordedKeys.clear();RecordedPad=0;return;}
    if(Device==0&&(ReadKey(VK_LWIN)||ReadKey(VK_RWIN))){Listening=false;Error="Windows-key combinations are reserved by the operating system.";return;}
    Keys keys;uint32_t pad=Pad(Reading)|(now<GuideUntil?InputBindings::Guide:0);
    if(Device==0)for(uint16_t key=1;key<256;++key)if(InputBindings::Normalize(key)==key&&!InputBindings::Name(key).empty()&&ReadKey(key))keys.push_back(key);
    const bool down=Device==0?!keys.empty():pad!=0;
    if(WaitNeutral){if(!down&&!ReadKey(VK_LBUTTON))WaitNeutral=false;return;}
    if(down){SawInput=true;if(Device==0){for(auto key:keys)if(std::find(RecordedKeys.begin(),RecordedKeys.end(),key)==RecordedKeys.end())RecordedKeys.push_back(key);}else RecordedPad|=pad;}
    else if(SawInput){if(Device==0)DraftKeys=RecordedKeys;else DraftPad=RecordedPad;Listening=false;JustRecorded=true;}
}
inline void SelectKey(uint16_t key){const auto found=std::find(DraftKeys.begin(),DraftKeys.end(),key);if(found==DraftKeys.end())DraftKeys.push_back(key);else DraftKeys.erase(found);Error.clear();}
inline bool Key(const char* text,const char* name,float width,float height){
    const auto key=InputBindings::Code(name);if(!key)return false;
    const bool selected=std::find(DraftKeys.begin(),DraftKeys.end(),*key)!=DraftKeys.end();
    const auto& t=HelpyImGuiTheme::Current;
    ImGui::PushStyleColor(ImGuiCol_Button,selected?t.selection:t.control);ImGui::PushStyleColor(ImGuiCol_Text,selected?t.accentHover:t.text);
    const bool pressed=ImGui::Button(text,{width,height});ImGui::PopStyleColor(2);
    if(pressed)SelectKey(*key);
    if(ImGui::IsItemHovered()||ImGui::IsItemFocused())HelpyImGuiTheme::Tooltip(name);
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::BindingKeys[name]=HelpyPreviewHooks::LastRect();
#endif
    return pressed;
}
struct KeyCap{const char* Name;const char* Label;float Units=1;};
inline void Row(std::initializer_list<KeyCap> keys,int row){
    float units=0;for(const auto& key:keys)units+=key.Units;
    const auto gap=ImGui::GetStyle().ItemSpacing.x;const float unit=std::max(1.f,(ImGui::GetContentRegionAvail().x-gap*(keys.size()-1))/units),height=HelpyImGuiTheme::Current.Y(30);
    ImGui::PushID(row);int index=0;for(const auto& key:keys){if(index)ImGui::SameLine();ImGui::PushID(index++);Key(key.Label?key.Label:key.Name,key.Name,unit*key.Units,height);ImGui::PopID();}ImGui::PopID();
}
inline void Keyboard(){
    Row({{"Esc",nullptr},{"F1",nullptr},{"F2",nullptr},{"F3",nullptr},{"F4",nullptr},{"F5",nullptr},{"F6",nullptr},{"F7",nullptr},{"F8",nullptr},{"F9",nullptr},{"F10",nullptr},{"F11",nullptr},{"F12",nullptr}},0);
    Row({{"Backtick","`"},{"1",nullptr},{"2",nullptr},{"3",nullptr},{"4",nullptr},{"5",nullptr},{"6",nullptr},{"7",nullptr},{"8",nullptr},{"9",nullptr},{"0",nullptr},{"Minus","-"},{"Equals","="},{"Backspace","Bksp",2}},1);
    Row({{"Tab",nullptr,1.5f},{"Q",nullptr},{"W",nullptr},{"E",nullptr},{"R",nullptr},{"T",nullptr},{"Y",nullptr},{"U",nullptr},{"I",nullptr},{"O",nullptr},{"P",nullptr},{"LeftBracket","["},{"RightBracket","]"},{"Backslash","\\",1.5f}},2);
    Row({{"CapsLock","Caps",1.75f},{"A",nullptr},{"S",nullptr},{"D",nullptr},{"F",nullptr},{"G",nullptr},{"H",nullptr},{"J",nullptr},{"K",nullptr},{"L",nullptr},{"Semicolon",";"},{"Apostrophe","'"},{"Enter",nullptr,2.25f}},3);
    Row({{"Shift",nullptr,2.25f},{"Z",nullptr},{"X",nullptr},{"C",nullptr},{"V",nullptr},{"B",nullptr},{"N",nullptr},{"M",nullptr},{"Comma",","},{"Period","."},{"Slash","/"},{"Shift",nullptr,2.75f}},4);
    Row({{"Ctrl",nullptr,1.5f},{"Alt",nullptr,1.5f},{"Space",nullptr,6},{"Alt",nullptr,1.5f},{"Ctrl",nullptr,1.5f},{"Left","<"},{"Down","v"},{"Up","^"},{"Right",">"}},5);
    if(ImGui::TreeNode("More keys")){
        Row({{"Insert","Ins"},{"Home",nullptr},{"End",nullptr},{"Delete","Del"},{"PageUp","PgUp"},{"PageDown","PgDn"},{"PrintScreen","PrtSc"},{"ScrollLock","ScrLk"},{"Pause",nullptr}},6);
        Row({{"F13",nullptr},{"F14",nullptr},{"F15",nullptr},{"F16",nullptr},{"F17",nullptr},{"F18",nullptr},{"F19",nullptr},{"F20",nullptr},{"F21",nullptr},{"F22",nullptr},{"F23",nullptr},{"F24",nullptr}},7);
        Row({{"NumLock","Num"},{"Num0","0"},{"Num1","1"},{"Num2","2"},{"Num3","3"},{"Num4","4"},{"Num5","5"},{"Num6","6"},{"Num7","7"},{"Num8","8"},{"Num9","9"},{"NumAdd","+"},{"NumSubtract","-"},{"NumMultiply","*"},{"NumDivide","/"},{"NumDecimal","."}},8);
        ImGui::TreePop();
    }
}
inline void PadKey(const char* name,const char* label,uint32_t bit,ImVec2 at,ImVec2 size,int face=-1){
    const auto& t=HelpyImGuiTheme::Current;const bool selected=(DraftPad&bit)!=0;
    const auto glyphs=HelpyControllerDevice::Resolve(t.controllerGlyphs,Reading.Glyphs);
    ImGui::SetCursorScreenPos(at);ImGui::PushID(name);ImGui::PushStyleColor(ImGuiCol_Button,selected?t.selection:t.control);ImGui::PushStyleColor(ImGuiCol_Text,selected?t.accentHover:t.text);
    if(ImGui::Button(face<0?label:"##face",size)){DraftPad^=bit;Error.clear();}
    if(face>=0)HelpyControllerInput::DrawGlyph({at.x+size.x*.5f,at.y+size.y*.5f},std::min(size.x,size.y)*.32f,size_t(face),glyphs==HelpyControllerDevice::PlayStation?2:glyphs==HelpyControllerDevice::Nintendo?4:glyphs==HelpyControllerDevice::Xbox?1:3,true);
    ImGui::PopStyleColor(2);if(ImGui::IsItemHovered()||ImGui::IsItemFocused())HelpyImGuiTheme::Tooltip(name);
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::BindingPad[name]=HelpyPreviewHooks::LastRect();
#endif
    ImGui::PopID();
}
inline void Controller(){
    const auto& t=HelpyImGuiTheme::Current;const auto glyphs=HelpyControllerDevice::Resolve(t.controllerGlyphs,Reading.Glyphs);const bool ps=glyphs==HelpyControllerDevice::PlayStation,nintendo=glyphs==HelpyControllerDevice::Nintendo;
    ImGui::TextDisabled("%s",Reading.Connected?(ps?"PlayStation layout":nintendo?"Nintendo layout":"Xbox / compatible layout"):"Connect a controller, or select the buttons below.");
    const auto origin=ImGui::GetCursorScreenPos();const float width=ImGui::GetContentRegionAvail().x,height=std::max(80.f,std::min(t.Y(250),ImGui::GetContentRegionAvail().y-ImGui::GetStyle().ItemSpacing.y)),scale=std::min(width/700.f,height/270.f);const ImVec2 center{origin.x+width*.5f,origin.y+height*.53f};
    auto* draw=ImGui::GetWindowDrawList();const auto point=[&](float x,float y){return ImVec2(center.x+x*scale,center.y+y*scale);};
    std::array<ImVec2,12> body{point(-210,-78),point(-110,-104),point(110,-104),point(210,-78),point(242,-5),point(264,100),point(214,118),point(150,70),point(-150,70),point(-214,118),point(-264,100),point(-242,-5)};
    draw->AddConcavePolyFilled(body.data(),int(body.size()),ImGui::GetColorU32(t.panel));draw->AddPolyline(body.data(),int(body.size()),ImGui::GetColorU32(t.border),ImDrawFlags_Closed,2);
    ImGui::Dummy({width,height});
    const auto button=[&](const char* name,const char* label,uint32_t bit,float x,float y,float w=42,float h=34,int face=-1){const ImVec2 size{w*scale,h*scale},pos=point(x,y);PadKey(name,label,bit,{pos.x-size.x*.5f,pos.y-size.y*.5f},size,face);};
    button("LT",ps?"L2":nintendo?"ZL":"LT",InputBindings::LeftTrigger,-190,-112,64);button("RT",ps?"R2":nintendo?"ZR":"RT",InputBindings::RightTrigger,190,-112,64);
    button("LB",ps?"L1":nintendo?"L":"LB",0x100,-190,-70,64);button("RB",ps?"R1":nintendo?"R":"RB",0x200,190,-70,64);
    button("View",ps?"Share":nintendo?"-":"View",0x20,-66,-20,64);button("Menu",ps?"Options":nintendo?"+":"Menu",0x10,66,-20,64);
    button("Guide",ps?"PS":"Home",InputBindings::Guide,0,35,70);
    if(ps)button("Touchpad","Touchpad",InputBindings::Touchpad,0,-70,126,48);
    const float dx=ps?-175:-106,dy=ps?-4:47;
    button("Up","^",1,dx,dy-30,32);button("Down","v",2,dx,dy+30,32);button("Left","<",4,dx-34,dy,32);button("Right",">",8,dx+34,dy,32);
    button("L3","L3",0x40,ps?-95:-177,ps?60:-5,48,42);button("R3","R3",0x80,95,60,48,42);
    button("South","",0x1000,177,30,36,34,0);button("East","",0x2000,217,-5,36,34,1);button("West","",0x4000,137,-5,36,34,2);button("North","",0x8000,177,-40,36,34,3);
    ImGui::SetCursorScreenPos({origin.x,origin.y+height+ImGui::GetStyle().ItemSpacing.y});ImGui::Dummy({0,0});
}
using Check=std::function<std::string(const std::string&,const std::string&)>;
inline bool Edit(const char* id,const char* label,std::string& keyboard,std::string& controller,const Check& check={},const char* defaultKeys="None",const char* defaultPad="None",bool controllerOnly=false){
    const auto& t=HelpyImGuiTheme::Current;ImGui::PushID(id);const auto owner=ImGui::GetID("binding-owner");bool changed=false;
    ImGui::TextUnformatted(label);
    const bool edit=HelpyImGuiTheme::SoftButton("Edit bindings");
#if defined(HELPY_PREVIEW)
    HelpyPreviewHooks::BindingEditors[id]=HelpyPreviewHooks::LastRect();HelpyPreviewHooks::MenuFocus[id]={ImGui::GetItemID(),ImGui::GetCurrentWindow()->ID};
#endif
    if(edit){
        try{DraftKeys=InputBindings::Parse(keyboard);DraftPad=InputBindings::ParsePad(controller);OriginalKeys=keyboard;OriginalPad=controller;Owner=owner;Error.clear();Listening=false;Device=controllerOnly?1:0;NextRead=0;ImGui::OpenPopup("Input bindings");}catch(const std::exception& error){Error=error.what();}
    }
    ImGui::SameLine();if(controllerOnly){const auto at=ImGui::GetCursorScreenPos();HelpyControllerInput::DrawPhysicalGlyph({at.x+t.X(18),at.y+ImGui::GetFrameHeight()*.5f},t.Y(11),InputBindings::ParsePad(controller),t.controllerGlyphs,false);ImGui::Dummy({t.X(36),ImGui::GetFrameHeight()});if(ImGui::IsItemHovered())HelpyImGuiTheme::Tooltip(controller.c_str());}else ImGui::TextWrapped("%s  |  %s",keyboard.c_str(),controller.c_str());
    const auto* viewport=ImGui::GetMainViewport();const ImVec2 maximum{std::max(1.f,viewport->WorkSize.x-24),std::max(1.f,viewport->WorkSize.y-24)};
    ImGui::SetNextWindowSizeConstraints({std::min(520.f,maximum.x),std::min(380.f,maximum.y)},maximum);ImGui::SetNextWindowSize({std::min(t.X(850),maximum.x),std::min(t.Y(600),maximum.y)},ImGuiCond_FirstUseEver);
    if(ImGui::BeginPopupModal("Input bindings",nullptr,ImGuiWindowFlags_NoCollapse)){
        if(Owner!=owner){ImGui::CloseCurrentPopup();ImGui::EndPopup();ImGui::PopID();return false;}
        ImGui::TextUnformatted(label);ImGui::TextWrapped("%s",controllerOnly?"Select or record one controller button, then lock in. Occupied buttons swap actions.":"Select keys or buttons, or record a combination. Lock in saves both shortcuts.");
        if(Listening)ImGui::BeginDisabled();
        constexpr std::array labels{"Keyboard","Controller"};if(controllerOnly){Device=1;ImGui::TextUnformatted("Controller buttons");}else HelpyMenu::Tabs("binding-device",labels,Device,false);
        if(Listening)ImGui::EndDisabled();
        ImGui::TextWrapped("%s",(Device==0?InputBindings::Text(DraftKeys):InputBindings::PadText(DraftPad)).c_str());
        const float footer=ImGui::GetFrameHeightWithSpacing()*5+t.Y(12);
        if(HelpyImGuiTheme::BeginPanel("binding-visual",{0,std::max(t.Y(80),ImGui::GetContentRegionAvail().y-footer)},true)){
            ImGui::BeginDisabled(Listening);if(Device==0)Keyboard();else Controller();ImGui::EndDisabled();
        }HelpyImGuiTheme::EndPanel();
        if(Listening){ImGui::TextDisabled("%s",WaitNeutral?"Release held keys/buttons, then press your combination.":"Recording. Release to review; Esc or View + Menu stops recording.");ImGui::PushItemFlag(ImGuiItemFlags_NoNav,true);if(HelpyImGuiTheme::SoftButton("Stop recording"))Listening=false;ImGui::PopItemFlag();}
        else{if(HelpyImGuiTheme::SoftButton("Record combination")){Listening=true;WaitNeutral=true;SawInput=false;RecordedKeys.clear();RecordedPad=0;ImGui::ClearActiveID();}ImGui::SameLine();
#if defined(HELPY_PREVIEW)
            HelpyPreviewHooks::BindingActions["Record"]=HelpyPreviewHooks::LastRect();
#endif
            if(HelpyImGuiTheme::SoftButton("Defaults")){DraftKeys=InputBindings::Parse(defaultKeys);DraftPad=InputBindings::ParsePad(defaultPad);Error.clear();}ImGui::SameLine();
            if(HelpyImGuiTheme::SoftButton("Clear")){if(Device==0)DraftKeys.clear();else DraftPad=0;Error.clear();}
#if defined(HELPY_PREVIEW)
            HelpyPreviewHooks::BindingActions["Clear"]=HelpyPreviewHooks::LastRect();
#endif
        }
        const float statusAt=ImGui::GetCursorPosY();ImGui::PushStyleColor(ImGuiCol_Text,t.warning);ImGui::TextWrapped("%s",Error.empty()?" ":Error.c_str());ImGui::PopStyleColor();ImGui::Dummy({0,std::max(0.f,statusAt+ImGui::GetTextLineHeightWithSpacing()*4-ImGui::GetCursorPosY())});
        ImGui::BeginDisabled(Listening);if(JustRecorded){HelpyControllerInput::FocusNext();JustRecorded=false;}
        if(HelpyImGuiTheme::SoftButton("Lock in"))try{
            const auto keys=InputBindings::Text(InputBindings::Validate(DraftKeys)),pad=InputBindings::PadText(DraftPad);
            if(keyboard!=OriginalKeys||controller!=OriginalPad)throw std::runtime_error("This binding changed elsewhere. Reopen the editor.");
            const auto issue=check?check(keys,pad):std::string{};if(!issue.empty())throw std::runtime_error(issue);
            keyboard=keys;controller=pad;changed=true;ImGui::CloseCurrentPopup();Cancel();
        }catch(const std::exception& error){Error=error.what();}
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::BindingLock=HelpyPreviewHooks::LastRect();
#endif
        ImGui::SameLine();if(HelpyImGuiTheme::SoftButton("Cancel")||(!Listening&&ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight,false))){ImGui::CloseCurrentPopup();Cancel();}
#if defined(HELPY_PREVIEW)
        HelpyPreviewHooks::BindingActions["Cancel"]=HelpyPreviewHooks::LastRect();
#endif
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }
    ImGui::PopID();return changed;
}
}
