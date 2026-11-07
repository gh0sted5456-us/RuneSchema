#include "Generator/HelpyControllerInput.h"
#include "Generator/HelpyMenu.h"
#include "Core/HelpyControllerShortcut.h"
#include <stdexcept>
#include <source_location>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_events.h>
static void Check(bool value,const std::source_location at=std::source_location::current()){if(!value){const auto message="Controller mode regression at line "+std::to_string(at.line());std::cerr<<message<<std::endl;throw std::runtime_error(message);}}
int main()try{using namespace PS::HelpyInputMode;Selection selected;selected.Update(Mode::Automatic,true,false);Check(selected.Controller);selected.Update(Mode::Automatic,false,true);Check(!selected.Controller);selected.Update(Mode::Controller,false,true);Check(selected.Controller);selected.Update(Mode::MouseKeyboard,true,false);Check(!selected.Controller);Check(Confirm(Glyphs::PlayStation)=="Cross"&&Cancel(Glyphs::Xbox)=="B"&&Confirm(Glyphs::Generic)=="South");
    using namespace PS::HelpyControllerDevice;Check(Resolve(4,Xbox)==Nintendo&&Resolve(2,Nintendo)==PlayStation&&Resolve(0,Nintendo)==Nintendo);
    Check(Resolve(0,PlayStation)==PlayStation&&Resolve(1,PlayStation)==Xbox);
    XINPUT_GAMEPAD idle{};idle.sThumbLX=1000;Check(!Active(idle));idle.sThumbLX=12000;Check(Active(idle));
    PS::HelpyControllerInput::DeviceRead=[](bool){return Reading{};};
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={800,600};io.Fonts->Build();XINPUT_GAMEPAD pad{};pad.wButtons=XINPUT_GAMEPAD_A;pad.sThumbLX=20000;
    PS::HelpyControllerInput::Poll(0,&pad);ImGui::NewFrame();Check(ImGui::IsKeyDown(ImGuiKey_GamepadFaceDown)&&ImGui::IsKeyDown(ImGuiKey_GamepadDpadRight)&&!ImGui::IsKeyDown(ImGuiKey_GamepadLStickRight));PS::HelpyControllerInput::Observe(0);Check(PS::HelpyControllerInput::Selection.Controller);
    ImGui::Begin("Controller glyph feedback");const auto gold=PS::HelpyControllerInput::HintColor(true),neutral=PS::HelpyControllerInput::HintColor(false);Check(gold!=neutral);
    auto* draw=ImGui::GetWindowDrawList();const auto before=draw->VtxBuffer.Size;PS::HelpyControllerInput::DrawGlyph({100,100},12,0,2);bool highlighted=false;for(int i=before;i<draw->VtxBuffer.Size;++i)highlighted|=draw->VtxBuffer[i].col==gold;Check(highlighted);
    Check(PS::HelpyControllerInput::HintsHeight(320)>PS::HelpyControllerInput::HintsHeight(1800));ImGui::End();ImGui::EndFrame();
    pad={};PS::HelpyControllerInput::Poll(0,&pad);ImGui::NewFrame();Check(!ImGui::IsKeyDown(ImGuiKey_GamepadFaceDown)&&!ImGui::IsKeyDown(ImGuiKey_GamepadDpadRight));ImGui::EndFrame();PS::HelpyControllerInput::Poll(1,&pad);Check(!(io.ConfigFlags&ImGuiConfigFlags_NavEnableGamepad));Check(!PS::HelpyControllerInput::Initialized);ImGui::DestroyContext();
    {
        PS::HelpyTouchpadTap tap;Touchpad touch;Check(!tap.Poll(true,1,touch,0));touch.Down=true;touch.Fingers=1;touch.X=.5f;touch.Y=.5f;Check(!tap.Poll(true,1,touch,.1));touch.Down=false;touch.Fingers=0;Check(tap.Poll(true,1,touch,.2));
        touch.Down=true;touch.Fingers=1;Check(!tap.Poll(true,1,touch,.3));touch.Down=false;touch.Fingers=0;Check(!tap.Poll(true,1,touch,.7));
        touch.Down=true;touch.Fingers=1;Check(!tap.Poll(true,1,touch,.8));touch.Clicked=true;Check(!tap.Poll(true,1,touch,.85));touch.Down=false;touch.Fingers=0;touch.Clicked=false;Check(!tap.Poll(true,1,touch,.9));
        touch.Down=true;touch.Fingers=1;Check(!tap.Poll(true,1,touch,1));touch.Fingers=2;Check(!tap.Poll(true,1,touch,1.02));touch.Down=false;touch.Fingers=0;Check(!tap.Poll(true,1,touch,1.1));
        touch.Down=true;touch.Fingers=1;Check(!tap.Poll(false,1,touch,1.2));Check(!tap.Poll(true,1,touch,1.3));touch.Down=false;touch.Fingers=0;Check(!tap.Poll(true,1,touch,1.4));
    }
    {
        using PS::HelpyControllerPointer::Motion;
        Check(Motion(1000,-1000,1.f/60,900).X==0);
        const auto full=Motion(32767,0,1.f/60,900),half=Motion(32767,0,1.f/120,900),diagonal=Motion(32767,32767,1.f/60,900);
        Check(std::abs(full.X-2*half.X)<.001f&&std::abs(full.X-15)<.001f);
        Check(std::sqrt(diagonal.X*diagonal.X+diagonal.Y*diagonal.Y)<=full.X+.001f);
        Check(Motion(32767,0,-1,900).X==0&&Motion(32767,0,10,900).X<=45.001f);
        ImGui::CreateContext();auto& pointerIo=ImGui::GetIO();pointerIo.IniFilename=nullptr;pointerIo.DisplaySize={800,600};pointerIo.DeltaTime=1.f/60;pointerIo.Fonts->Build();pointerIo.ConfigInputTrickleEventQueue=false;
        PS::HelpyControllerInput::DeviceRead=[](bool){return Reading{};};PS::HelpyControllerInput::Selection.Controller=false;
        XINPUT_GAMEPAD cursorPad{};pointerIo.AddMousePosEvent(100,100);
        const auto frame=[&](bool enabled=true){PS::HelpyControllerInput::Poll(0,&cursorPad);const auto moved=PS::HelpyControllerInput::UpdatePointer(false,enabled);ImGui::NewFrame();PS::HelpyControllerInput::Observe(0);ImGui::EndFrame();return moved;};
        frame();cursorPad.sThumbRX=32767;const auto moved=frame();Check(moved&&moved->x>100&&PS::HelpyControllerInput::Selection.Controller);
        Check(!ImGui::IsKeyDown(ImGuiKey_GamepadRStickRight));
        cursorPad={};frame();cursorPad.wButtons=XINPUT_GAMEPAD_RIGHT_THUMB;frame();Check(pointerIo.MouseDown[0]);
        frame(false);Check(!pointerIo.MouseDown[0]);frame();Check(!pointerIo.MouseDown[0]);
        cursorPad={};frame();cursorPad.wButtons=XINPUT_GAMEPAD_RIGHT_THUMB;frame();Check(pointerIo.MouseDown[0]);
        PS::HelpyControllerInput::Suspend();PS::HelpyControllerInput::UpdatePointer(false,false);ImGui::NewFrame();Check(!pointerIo.MouseDown[0]);ImGui::EndFrame();
        PS::HelpyControllerInput::Selection.Controller=false;PS::HelpyControllerInput::NavigationCanvas=123456;GImGui->NavId=123456;
        cursorPad={};cursorPad.sThumbLX=20000;PS::HelpyControllerInput::Feed(cursorPad);ImGui::NewFrame();GImGui->NavId=123456;
        Check(!ImGui::IsKeyDown(ImGuiKey_GamepadDpadRight));PS::HelpyControllerInput::Observe(0);Check(PS::HelpyControllerInput::Selection.Controller);
        GImGui->NavId=0;PS::HelpyControllerInput::NavigationCanvas=0;ImGui::EndFrame();
        const auto saved=PS::HelpyImGuiTheme::DocumentFrom(PS::HelpyImGuiTheme::Current,200);
        Check(saved["input"].contains("cursor")&&saved["input"]["cursorSpeed"]==900.f);
        ImGui::DestroyContext();
    }
    (void)Read(true);Check(SDL_WasInit(SDL_INIT_GAMEPAD)!=0);
    const std::array<std::pair<const char*,Labels>,4> layouts{{{"xboxone",Xbox},{"ps4",PlayStation},{"ps5",PlayStation},{"switchpro",Nintendo}}};
    for(const auto& [type,expected]:layouts){
        SDL_VirtualJoystickDesc desc{};SDL_INIT_INTERFACE(&desc);desc.type=SDL_JOYSTICK_TYPE_GAMEPAD;desc.naxes=6;desc.nbuttons=16;desc.button_mask=0xffff;desc.axis_mask=0x3f;desc.name="RuneSchema private controller fixture";
        SDL_VirtualJoystickTouchpadDesc touchpad{2};if(expected==PlayStation){desc.ntouchpads=1;desc.touchpads=&touchpad;}
        const auto id=SDL_AttachVirtualJoystick(&desc);Check(id!=0);const auto mapping=std::string("*,RuneSchema fixture,a:b0,b:b1,x:b2,y:b3,guide:b4,touchpad:b5,back:b6,start:b7,leftstick:b8,rightstick:b9,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,type:")+type+",platform:Windows,";
        Check(SDL_SetGamepadMapping(id,mapping.c_str()));auto* joystick=SDL_OpenJoystick(id);Check(joystick!=nullptr);Check(SDL_SetJoystickVirtualButton(joystick,0,true));Check(SDL_SetJoystickVirtualAxis(joystick,0,20000));
        const auto reading=Read(true);Check(reading.Connected&&(reading.Pad.wButtons&XINPUT_GAMEPAD_A)&&reading.Pad.sThumbLX==20000&&reading.Glyphs==expected);
        // Drivers can post fresh events asynchronously; verify that prior unused events are drained.
        SDL_Event event{};event.type=SDL_EVENT_USER;for(unsigned i=0;i<32;++i)Check(SDL_PushEvent(&event));
        (void)Read(false);Check(SDL_PeepEvents(&event,1,SDL_PEEKEVENT,SDL_EVENT_USER,SDL_EVENT_USER)==0);
        {
            ImGui::CreateContext();auto& controllerIo=ImGui::GetIO();controllerIo.IniFilename=nullptr;controllerIo.DisplaySize={800,600};controllerIo.DeltaTime=1.f/60;controllerIo.Fonts->Build();
            PS::HelpyControllerInput::DeviceRead=&Read;PS::HelpyControllerInput::Selection.Controller=false;int activated=0;ImGuiID tile=0;
            const auto frame=[&]{PS::HelpyControllerInput::Poll(0);ImGui::NewFrame();PS::HelpyControllerInput::Observe(0);
                ImGui::Begin("controller-only startup",nullptr,ImGuiWindowFlags_NoFocusOnAppearing);if(ImGui::Button("Enter"))activated=1;PS::HelpyControllerInput::DefaultFocus();ImGui::SetItemDefaultFocus();
                ImGui::SameLine();if(ImGui::Button("Tile"))activated=2;tile=ImGui::GetItemID();ImGui::End();ImGui::EndFrame();};
            Check(SDL_SetJoystickVirtualButton(joystick,0,false));Check(SDL_SetJoystickVirtualAxis(joystick,0,0));Check(SDL_SetJoystickVirtualAxis(joystick,4,-32768));Check(SDL_SetJoystickVirtualAxis(joystick,5,-32768));frame();frame();Check(!ImGui::GetCurrentContext()->NavId);
            Check(SDL_SetJoystickVirtualButton(joystick,0,true));frame();frame();Check(SDL_SetJoystickVirtualButton(joystick,0,false));frame();
            Check(PS::HelpyControllerInput::ReportedGlyphs==expected&&PS::HelpyControllerInput::Selection.Controller);
            Check(ImGui::GetCurrentContext()->NavWindow&&ImGui::GetCurrentContext()->NavId);
            Check(SDL_SetJoystickVirtualButton(joystick,0,true));frame();Check(SDL_SetJoystickVirtualButton(joystick,0,false));frame();Check(activated==1);
            Check(SDL_SetJoystickVirtualAxis(joystick,0,25000));frame();frame();Check(SDL_SetJoystickVirtualAxis(joystick,0,0));frame();frame();Check(ImGui::GetCurrentContext()->NavId==tile);
            Check(SDL_SetJoystickVirtualButton(joystick,0,true));frame();Check(SDL_SetJoystickVirtualButton(joystick,0,false));frame();Check(activated==2);
            constexpr std::array labels{"First","Second","Third"};int category=0,tab=0;ImGuiID content=0;XINPUT_GAMEPAD menuPad{};ImGui::FocusWindow(nullptr);
            const auto menuFrame=[&]{PS::HelpyControllerInput::Feed(menuPad);ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({800,600});ImGui::Begin("controller two-pane menu");
                PS::HelpyMenu::Categories("controller-categories",labels,category);if(PS::HelpyMenu::BeginBody("controller-menu-body",category)){
                    PS::HelpyMenu::Tabs("controller-tabs",labels,tab);
                    if(PS::HelpyImGuiTheme::BeginScroll("controller-content",{0,300})){
                        ImGui::Button("Content action");content=ImGui::GetItemID();
                    }PS::HelpyImGuiTheme::EndScroll(PS::HelpyImGuiTheme::Current);
                }
                PS::HelpyMenu::EndBody();ImGui::End();PS::HelpyControllerInput::ResolvePaneNavigation();ImGui::EndFrame();};
            menuFrame();menuFrame();Check(GImGui->NavId&&GImGui->NavIdIsAlive);
            menuPad.wButtons=XINPUT_GAMEPAD_DPAD_DOWN;menuFrame();menuPad={};menuFrame();menuPad.wButtons=XINPUT_GAMEPAD_A;menuFrame();menuPad={};menuFrame();Check(category==1);
            menuPad.wButtons=XINPUT_GAMEPAD_DPAD_RIGHT;menuFrame();menuPad={};menuFrame();
            for(int step=0;step<3&&GImGui->NavId!=content;++step){menuPad.wButtons=XINPUT_GAMEPAD_DPAD_DOWN;menuFrame();menuPad={};menuFrame();}
            if(GImGui->NavId!=content)std::cerr<<"Nested content unreachable: "<<(GImGui->NavWindow?GImGui->NavWindow->Name:"none")<<" id="<<GImGui->NavId<<" expected="<<content<<'\n';
            Check(GImGui->NavId==content);
            menuPad.wButtons=XINPUT_GAMEPAD_DPAD_UP;menuFrame();menuPad={};menuFrame();Check(GImGui->NavId!=content);
            category=0;ImGui::FocusWindow(nullptr);menuFrame();menuFrame();menuPad.wButtons=XINPUT_GAMEPAD_DPAD_DOWN;menuFrame();menuPad={};menuFrame();
            menuPad.wButtons=XINPUT_GAMEPAD_DPAD_DOWN;menuFrame();menuPad={};menuFrame();
            menuPad.wButtons=XINPUT_GAMEPAD_A;menuFrame();menuPad={};menuFrame();
            for(int step=0;step<3&&GImGui->NavId!=content;++step){menuPad.wButtons=XINPUT_GAMEPAD_DPAD_DOWN;menuFrame();menuPad={};menuFrame();}Check(GImGui->NavId==content);
            ImGui::FocusWindow(nullptr);int section=0;ImGuiID nestedControl=0;
            const auto nestedFrame=[&]{PS::HelpyControllerInput::Feed(menuPad);ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({800,600});ImGui::Begin("nested section headers");
                PS::HelpyMenu::Tabs("outer-tabs",labels,tab);ImGui::Button("Cart");
                if(PS::HelpyImGuiTheme::BeginPanel("pinned page",{0,0},0,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse)){
                    PS::HelpyMenu::Tabs("inner-tabs",labels,section);
                    if(PS::HelpyMenu::BeginBody("inner-body",section)){ImGui::Button("Nested action");nestedControl=ImGui::GetItemID();}PS::HelpyMenu::EndBody();
                }PS::HelpyImGuiTheme::EndPanel();ImGui::End();PS::HelpyControllerInput::ResolvePaneNavigation();ImGui::EndFrame();};
            nestedFrame();nestedFrame();
            for(int step=0;step<6&&GImGui->NavId!=nestedControl;++step){menuPad.wButtons=XINPUT_GAMEPAD_DPAD_DOWN;nestedFrame();menuPad={};nestedFrame();menuPad.wButtons=XINPUT_GAMEPAD_A;nestedFrame();menuPad={};nestedFrame();}Check(GImGui->NavId==nestedControl);
            const int outerBefore=tab,innerBefore=section;menuPad.wButtons=XINPUT_GAMEPAD_RIGHT_SHOULDER;nestedFrame();menuPad={};nestedFrame();Check(tab==outerBefore&&section==innerBefore);
            ImGui::FocusWindow(nullptr);ImGuiID card=0;
            const auto bayFrame=[&]{PS::HelpyControllerInput::Feed(menuPad);ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({800,600});ImGui::Begin("nested Item Bay route");
                if(PS::HelpyImGuiTheme::BeginPanel("nearby sibling sidebar",{100,300})){ImGui::SetCursorPosY(60);ImGui::Button("Sibling");}PS::HelpyImGuiTheme::EndPanel();ImGui::SameLine();
                PS::HelpyImGuiTheme::BeginPanel("bay body",{0,0});
                ImGui::Button("Toolbar");PS::HelpyControllerInput::DefaultFocus();
                if(PS::HelpyImGuiTheme::BeginPanel("browse and details",{0,300})){
                    if(PS::HelpyImGuiTheme::BeginPanel("item rows",{0,180})){ImGui::Button("Card");card=ImGui::GetItemID();}PS::HelpyImGuiTheme::EndPanel();
                }PS::HelpyImGuiTheme::EndPanel();ImGui::Button("Details");PS::HelpyImGuiTheme::EndPanel();ImGui::End();PS::HelpyControllerInput::ResolvePaneNavigation();ImGui::EndFrame();};
            bayFrame();bayFrame();menuPad.wButtons=XINPUT_GAMEPAD_DPAD_DOWN;bayFrame();menuPad={};bayFrame();Check(GImGui->NavId==card);
            ImGui::DestroyContext();
            std::cout<<type<<": cold-start focus, confirm, stick navigation and two-pane categories passed.\n";
        }
        {
            PS::HelpyControllerInput::KeepShortcuts=true;PS::HelpyControllerInput::Suspend();
            for(int button=0;button<16;++button)Check(SDL_SetJoystickVirtualButton(joystick,button,false));
            for(int axis=0;axis<4;++axis)Check(SDL_SetJoystickVirtualAxis(joystick,axis,0));
            Check(SDL_SetJoystickVirtualAxis(joystick,4,-32768));Check(SDL_SetJoystickVirtualAxis(joystick,5,-32768));
            ImGui::CreateContext();auto& pointerIo=ImGui::GetIO();pointerIo.IniFilename=nullptr;pointerIo.DisplaySize={800,600};pointerIo.DeltaTime=1.f/60;pointerIo.Fonts->Build();pointerIo.ConfigInputTrickleEventQueue=false;
            PS::HelpyControllerInput::DeviceRead=&Read;PS::HelpyControllerInput::Selection.Controller=false;
            int selected=0,wrong=0,dropped=0;bool contextMenu=false;float quantity=0;
            const auto pointerFrame=[&](bool interactive=true){
                PS::HelpyControllerInput::Poll(0,nullptr,interactive);PS::HelpyControllerInput::UpdatePointer(false,interactive);
                ImGui::NewFrame();PS::HelpyControllerInput::Observe(0);
                ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({800,600});ImGui::Begin("controller pointer",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize);
                ImGui::SetCursorScreenPos({20,100});if(ImGui::Button("Navigation target",{120,40}))++wrong;
                if(!GImGui->NavId){ImGui::FocusWindow(ImGui::GetCurrentWindow());ImGui::SetFocusID(ImGui::GetItemID(),ImGui::GetCurrentWindow());}
                ImGui::SetCursorScreenPos({180,100});if(ImGui::Button("Pointer target",{140,40}))++selected;
                if(ImGui::BeginPopupContextItem("pointer actions")){contextMenu=true;ImGui::CloseCurrentPopup();ImGui::EndPopup();}
                ImGui::SetCursorScreenPos({180,180});ImGui::SetNextItemWidth(200);ImGui::SliderFloat("Quantity",&quantity,0,100);
                ImGui::SetCursorScreenPos({180,250});ImGui::Button("Drag source",{120,40});
                if(ImGui::BeginDragDropSource()){const int value=42;ImGui::SetDragDropPayload("controller-grab",&value,sizeof(value));ImGui::TextUnformatted("Grabbed");ImGui::EndDragDropSource();}
                ImGui::SetCursorScreenPos({400,250});ImGui::Button("Drop target",{140,40});
                if(ImGui::BeginDragDropTarget()){if(ImGui::AcceptDragDropPayload("controller-grab"))++dropped;ImGui::EndDragDropTarget();}
                ImGui::End();PS::HelpyControllerInput::DrawPointer();ImGui::Render();
            };
            pointerIo.AddMousePosEvent(230,120);pointerFrame();pointerFrame();
            Check(SDL_SetJoystickVirtualAxis(joystick,2,16000));pointerFrame();Check(PS::HelpyControllerInput::PointerMode&&PS::HelpyControllerInput::Selection.Controller);
            Check(SDL_SetJoystickVirtualAxis(joystick,2,0));
            for(int idle=0;idle<80;++idle)pointerFrame(); // Pointer clicks must survive a pause after moving the stick.
            Check(ImGui::GetMouseCursor()==ImGuiMouseCursor_None&&!pointerIo.MouseDrawCursor);
            if(const char* output=std::getenv("RUNESCHEMA_POINTER_RENDER");output&&std::string_view(type)=="ps5"){
                nlohmann::json snapshots=nlohmann::json::array();
                for(float scale:{1.f,2.f}){
                    pointerIo.FontGlobalScale=scale;pointerFrame();const auto* draw=ImGui::GetForegroundDrawList();
                    nlohmann::json vertices=nlohmann::json::array(),indices=nlohmann::json::array();
                    for(const auto& vertex:draw->VtxBuffer)vertices.push_back({vertex.pos.x,vertex.pos.y,vertex.col});
                    for(auto index:draw->IdxBuffer)indices.push_back(index);
                    snapshots.push_back({{"Scale",scale},{"Center",{pointerIo.MousePos.x,pointerIo.MousePos.y}},{"Vertices",vertices},{"Indices",indices}});
                }
                std::ofstream file(output);file<<snapshots.dump();file.flush();Check(file.good());pointerIo.FontGlobalScale=1.f;
            }
            Check(PS::HelpyControllerInput::PointerMode&&PS::HelpyControllerInput::ReportedGlyphs==expected);
            Check(SDL_SetJoystickVirtualButton(joystick,0,true));pointerFrame();Check(pointerIo.MouseDown[0]&&!ImGui::IsKeyDown(ImGuiKey_GamepadFaceDown));
            Check(SDL_SetJoystickVirtualButton(joystick,0,false));pointerFrame();Check(selected==1&&wrong==0&&!pointerIo.MouseDown[0]);
            Check(SDL_SetJoystickVirtualButton(joystick,0,true));pointerFrame();Check(SDL_SetJoystickVirtualButton(joystick,0,false));pointerFrame();Check(selected==2&&wrong==0);
            Check(SDL_SetJoystickVirtualButton(joystick,2,true));pointerFrame();Check(pointerIo.MouseDown[1]&&!ImGui::IsKeyDown(ImGuiKey_GamepadFaceLeft));
            Check(SDL_SetJoystickVirtualButton(joystick,2,false));pointerFrame();Check(contextMenu&&!pointerIo.MouseDown[1]&&wrong==0);
            pointerIo.AddMousePosEvent(195,190);pointerFrame(); // A physical mouse takes over cleanly.
            Check(!PS::HelpyControllerInput::PointerMode);
            Check(SDL_SetJoystickVirtualAxis(joystick,2,16000));pointerFrame();Check(SDL_SetJoystickVirtualAxis(joystick,2,0));pointerFrame();
            Check(SDL_SetJoystickVirtualButton(joystick,0,true));pointerFrame();const auto initial=quantity;
            Check(SDL_SetJoystickVirtualAxis(joystick,2,32767));for(int drag=0;drag<6;++drag)pointerFrame();
            Check(quantity>initial&&wrong==0);Check(SDL_SetJoystickVirtualAxis(joystick,2,0));Check(SDL_SetJoystickVirtualButton(joystick,0,false));pointerFrame();
            Check(SDL_SetJoystickVirtualButton(joystick,0,true));pointerFrame();pointerFrame(false);Check(!pointerIo.MouseDown[0]);pointerFrame();Check(!pointerIo.MouseDown[0]);
            Check(SDL_SetJoystickVirtualButton(joystick,0,false));pointerFrame();
            Check(SDL_SetJoystickVirtualAxis(joystick,2,16000));pointerFrame();Check(SDL_SetJoystickVirtualAxis(joystick,2,0));pointerFrame();
            Check(SDL_SetJoystickVirtualButton(joystick,2,true));pointerFrame();Check(pointerIo.MouseDown[1]);
            PS::HelpyControllerInput::Suspend();pointerFrame(false);Check(!pointerIo.MouseDown[1]);
            Check(SDL_SetJoystickVirtualButton(joystick,2,false));pointerFrame();
            Check(SDL_SetJoystickVirtualAxis(joystick,2,16000));pointerFrame();Check(SDL_SetJoystickVirtualAxis(joystick,2,0));pointerFrame();
            Check(SDL_SetJoystickVirtualButton(joystick,8,true));pointerFrame();Check(ImGui::IsKeyPressed(ImGuiKey_GamepadL3,false)&&PS::HelpyControllerInput::PointerMode);Check(SDL_SetJoystickVirtualButton(joystick,8,false));pointerFrame();
            pointerIo.AddMousePosEvent(230,270);pointerFrame();
            Check(SDL_SetJoystickVirtualAxis(joystick,2,16000));pointerFrame();Check(SDL_SetJoystickVirtualAxis(joystick,2,0));pointerFrame();
            Check(SDL_SetJoystickVirtualButton(joystick,9,true));pointerFrame();Check(SDL_SetJoystickVirtualButton(joystick,9,false));pointerFrame();
            Check(PS::HelpyControllerInput::DragLatched&&pointerIo.MouseDown[0]);
            Check(SDL_SetJoystickVirtualAxis(joystick,2,32767));for(int move=0;move<15;++move)pointerFrame();Check(SDL_SetJoystickVirtualAxis(joystick,2,0));pointerFrame();
            Check(GImGui->DragDropActive&&dropped==0);
            Check(SDL_SetJoystickVirtualButton(joystick,9,true));pointerFrame();Check(!PS::HelpyControllerInput::DragLatched&&!pointerIo.MouseDown[0]);Check(SDL_SetJoystickVirtualButton(joystick,9,false));pointerFrame();Check(dropped==1);
            Check(SDL_SetJoystickVirtualButton(joystick,9,true));pointerFrame();Check(SDL_SetJoystickVirtualButton(joystick,9,false));pointerFrame();Check(PS::HelpyControllerInput::DragLatched);
            PS::HelpyControllerInput::Suspend();pointerFrame(false);Check(!PS::HelpyControllerInput::DragLatched&&!pointerIo.MouseDown[0]&&dropped==1);
            Check(SDL_SetJoystickVirtualAxis(joystick,2,16000));pointerFrame();Check(SDL_SetJoystickVirtualAxis(joystick,2,0));pointerFrame();
            Check(SDL_SetJoystickVirtualAxis(joystick,0,20000));pointerFrame();Check(!PS::HelpyControllerInput::PointerMode);Check(SDL_SetJoystickVirtualAxis(joystick,0,0));pointerFrame();
            ImGui::DestroyContext();PS::HelpyControllerInput::KeepShortcuts=false;
            std::cout<<type<<": pointer clicks, latched grab/drop, thumb buttons and safe handoff passed.\n";
        }
        if(expected==PlayStation){
            PS::HelpyControllerInput::KeepShortcuts=true;
            ImGui::CreateContext();auto& pointerIo=ImGui::GetIO();pointerIo.IniFilename=nullptr;pointerIo.DisplaySize={800,600};pointerIo.Fonts->Build();pointerIo.ConfigInputTrickleEventQueue=false;
            const auto pointerFrame=[&](bool enabled=true){PS::HelpyControllerInput::PointerReading=Read(false);const auto moved=PS::HelpyControllerInput::UpdatePointer(enabled);ImGui::NewFrame();PS::HelpyControllerInput::Observe(0);ImGui::EndFrame();return moved;};
            Check(SDL_SetJoystickVirtualButton(joystick,5,false));pointerIo.AddMousePosEvent(100,100);pointerFrame();
            Check(SDL_SetJoystickVirtualTouchpad(joystick,0,0,true,.2f,.2f,1.f));Check(!pointerFrame());Check(pointerIo.MousePos.x==100&&pointerIo.MousePos.y==100);
            Check(SDL_SetJoystickVirtualTouchpad(joystick,0,0,true,.3f,.3f,1.f));const auto moved=pointerFrame();Check(moved&&std::abs(moved->x-180)<.01f&&std::abs(moved->y-160)<.01f);Check(PS::HelpyControllerInput::Selection.Controller);
            Check(SDL_SetJoystickVirtualButton(joystick,5,true));pointerFrame();Check(pointerIo.MouseDown[0]);
            pointerFrame(false);Check(!pointerIo.MouseDown[0]);pointerFrame();Check(!pointerIo.MouseDown[0]);
            Check(SDL_SetJoystickVirtualButton(joystick,5,false));pointerFrame();Check(SDL_SetJoystickVirtualButton(joystick,5,true));pointerFrame();Check(pointerIo.MouseDown[0]);
            PS::HelpyControllerInput::Suspend();PS::HelpyControllerInput::UpdatePointer(false);ImGui::NewFrame();Check(!pointerIo.MouseDown[0]);ImGui::EndFrame();
            Check(SDL_SetJoystickVirtualButton(joystick,5,false));Check(SDL_SetJoystickVirtualTouchpad(joystick,0,0,false,.3f,.3f,0));pointerFrame();
            Check(SDL_SetJoystickVirtualTouchpad(joystick,0,0,true,.95f,.95f,1.f));Check(!pointerFrame());
            Check(SDL_SetJoystickVirtualTouchpad(joystick,0,0,true,1.f,1.f,1.f));Check(pointerFrame().has_value());
            Check(SDL_SetJoystickVirtualTouchpad(joystick,0,1,true,.6f,.6f,1.f));pointerFrame();Check(pointerIo.MouseDown[1]&&!pointerIo.MouseDown[0]);
            Check(PS::HelpyControllerInput::Selection.Controller&&PS::HelpyControllerInput::ReportedGlyphs==PlayStation);
            Check(SDL_SetJoystickVirtualTouchpad(joystick,0,1,false,.6f,.6f,0));pointerFrame();Check(!pointerIo.MouseDown[1]);
            Check(SDL_SetJoystickVirtualTouchpad(joystick,0,1,true,.6f,.6f,1.f));pointerFrame();Check(pointerIo.MouseDown[1]);
            pointerFrame(false);Check(!pointerIo.MouseDown[1]);pointerFrame();Check(!pointerIo.MouseDown[1]);
            Check(SDL_SetJoystickVirtualTouchpad(joystick,0,1,false,.6f,.6f,0));pointerFrame();
            Check(SDL_SetJoystickVirtualTouchpad(joystick,0,0,false,1.f,1.f,0));pointerFrame();pointerFrame();
            Check(SDL_SetJoystickVirtualTouchpad(joystick,0,0,true,.4f,.4f,1));pointerFrame();Check(!pointerIo.MouseDown[0]);
            Check(SDL_SetJoystickVirtualTouchpad(joystick,0,0,false,.4f,.4f,0));pointerFrame();Check(pointerIo.MouseDown[0]);Check(PS::HelpyControllerInput::Selection.Controller&&PS::HelpyControllerInput::ReportedGlyphs==PlayStation);pointerFrame();Check(!pointerIo.MouseDown[0]);
            Check(SDL_SetJoystickVirtualTouchpad(joystick,0,0,true,.4f,.4f,1));pointerFrame();Check(SDL_SetJoystickVirtualTouchpad(joystick,0,0,true,.6f,.4f,1));pointerFrame();Check(SDL_SetJoystickVirtualTouchpad(joystick,0,0,false,.6f,.4f,0));pointerFrame();Check(!pointerIo.MouseDown[0]);
            PS::HelpyControllerInput::UpdatePointer(false);ImGui::DestroyContext();PS::HelpyControllerInput::KeepShortcuts=false;
            std::cout<<type<<": relative touchpad pointer, click, focus/disable release and retouch passed.\n";
        }
        {PS::HelpyControllerShortcut::Gesture shortcut;
            Check(!shortcut.Poll(1,false,0,true,0));Check(SDL_SetJoystickVirtualButton(joystick,4,true));const auto guide=Read(false);Check(guide.Connected&&guide.Guide&&guide.Glyphs==expected);
            Check(shortcut.Poll(1,guide.Guide,guide.Pad.wButtons,true,1000));Check(!shortcut.Poll(1,guide.Guide,guide.Pad.wButtons,true,1300));
            Check(SDL_SetJoystickVirtualButton(joystick,4,false));Check(!Read(false).Guide);Check(!shortcut.Poll(1,false,0,true,1400));
            Check(SDL_SetJoystickVirtualButton(joystick,6,true));Check(SDL_SetJoystickVirtualButton(joystick,7,true));const auto chord=Read(false);Check((chord.Pad.wButtons&(XINPUT_GAMEPAD_BACK|XINPUT_GAMEPAD_START))==(XINPUT_GAMEPAD_BACK|XINPUT_GAMEPAD_START));
            Check(!shortcut.Poll(2,false,chord.Pad.wButtons,true,1500));Check(!shortcut.Poll(2,false,0,true,1600));Check(shortcut.Poll(2,false,chord.Pad.wButtons,true,1700));
            std::cout<<type<<": Guide and View + Menu shortcuts passed.\n";
        }
        SDL_CloseJoystick(joystick);Check(SDL_DetachVirtualJoystick(id));(void)Read(true);
    }
    Shutdown();Check(SDL_WasInit(SDL_INIT_GAMEPAD)==0);
}catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}
