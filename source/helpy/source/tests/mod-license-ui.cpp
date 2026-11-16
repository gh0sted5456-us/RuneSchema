#include "Generator/ModLicensePanel.h"
#include <iostream>
#include <stdexcept>
namespace {std::filesystem::path TestRoot;}
namespace PS::HostServices {std::filesystem::path ModDirectory(){return TestRoot;}}
namespace PS::HelpyControllerDevice {Reading Read(bool){return {};}void Shutdown(){}}
static void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main()try{
    namespace fs=std::filesystem;using namespace PS;using Json=nlohmann::json;
    const auto base=fs::temp_directory_path()/("mod-license-ui-"+std::to_string(GetCurrentProcessId()));TestRoot=base/"ue4ss/Mods/RuneSchema";
    struct Cleanup{fs::path Path;~Cleanup(){ModLicensePanel::Shutdown();std::error_code error;fs::remove_all(Path,error);}}cleanup{base};
    const auto a=TestRoot/"mods/First",b=TestRoot/"mods/Second";
    ConfigFiles::Write(a/"LICENSE","First mod terms");ConfigFiles::Write(a/"conditions.license","Additional first mod terms");ConfigFiles::Write(b/"LICENSE.txt","Second mod terms");
    const Json first={{"Folder",a.string()},{"Metadata",{{"website","https://www.nexusmods.com/runescapedragonwilds/mods/570"}}}},second={{"Folder",b.string()},{"Metadata",Json::object()}};
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.DisplaySize={760,560};io.DeltaTime=1.f/60;io.IniFilename=nullptr;io.Fonts->Build();
    io.ConfigFlags|=ImGuiConfigFlags_NavEnableGamepad|ImGuiConfigFlags_NavEnableKeyboard;io.BackendFlags|=ImGuiBackendFlags_HasGamepad;
    int tab=0;ImVec4 firstRect{},secondRect{};ImGuiID firstId{},secondId{};ImGuiWindow* titleWindow=nullptr;std::string opened;
    const auto frame=[&]{
        ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize(io.DisplaySize);ImGui::Begin("Mods fixture");ModLicensePanel::Context();
        constexpr std::array categories{"RuneSchema mods","Server comparison","UE4SS mods"};HelpyMenu::Categories("mod-manager-sections",categories,tab,ModLicensePanel::Selection.empty()?nullptr:"License");
        if(HelpyMenu::BeginBody("mod-manager-body",tab)){
            if(tab==3)ModLicensePanel::Render([&](const auto& url){opened=url;});
            else for(int i=0;i<2;++i){ImGui::PushID(i);const auto at=ImGui::GetCursorScreenPos();ModLicensePanel::Title(i?second:first,i?"Second":"First",{250,64});
                auto rect=HelpyPreviewHooks::LastRect();if(i){secondRect=rect;secondId=ImGui::GetItemID();}else{firstRect=rect;firstId=ImGui::GetItemID();}titleWindow=ImGui::GetCurrentWindow();
                if(ModLicensePanel::Selected(i?second:first))ModLicensePanel::Outline(at,{at.x+250,at.y+64});ImGui::PopID();}
        }HelpyMenu::EndBody();ImGui::End();ImGui::Render();
    };
    frame();frame();Check(!HelpyPreviewHooks::MenuTabs.contains("mod-manager-sections/License"),"License footer appeared without a selected mod");
    io.AddMousePosEvent((firstRect.x+firstRect.z)*.5f,(firstRect.y+firstRect.w)*.5f);frame();io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();frame();
    Check(ModLicensePanel::Selected(first)&&!ModLicensePanel::Selected(second),"Mouse activation did not select only the first mod");
    Check(HelpyPreviewHooks::MenuTabs.contains("mod-manager-sections/License"),"License footer did not appear after selection");
    const auto footer=HelpyPreviewHooks::MenuTabs.at("mod-manager-sections/License");Check(footer.w<=560&&footer.y>350,"License footer did not stay at the bottom of the internal rail");
    io.AddMousePosEvent(-100,-100);frame();ImGui::FocusWindow(titleWindow);ImGui::SetFocusID(secondId,titleWindow);ImGui::GetCurrentContext()->NavInputSource=ImGuiInputSource_Gamepad;ImGui::SetNavCursorVisible(true);
    io.AddKeyEvent(ImGuiKey_GamepadFaceDown,true);frame();io.AddKeyEvent(ImGuiKey_GamepadFaceDown,false);frame();frame();
    Check(ModLicensePanel::Selected(second),"Controller activation did not select the second mod");
    ModLicensePanel::Select(first,"First");tab=3;frame();ModLicensePanel::Select(second,"Second");
    for(int i=0;i<1000;++i){frame();if(!ModLicensePanel::Worker.valid()&&!ModLicensePanel::Requested)break;Sleep(1);}
    Check(ModLicensePanel::Data.Notices.size()==1&&ModLicensePanel::Data.Notices[0].Text=="Second mod terms","A stale asynchronous result replaced the selected mod's license");
    ModLicensePanel::Select(first,"First");for(int i=0;i<1000;++i){frame();if(!ModLicensePanel::Worker.valid()&&!ModLicensePanel::Requested)break;Sleep(1);}
    Check(ModLicensePanel::Data.Notices.size()==2,"Multiple notices were not available for the selected mod");
    io.DisplaySize={640,480};frame();frame();const auto small=HelpyPreviewHooks::MenuTabs.at("mod-manager-sections/License");Check(small.x>=0&&small.w<=480,"Resized license footer left the viewport");
    TestRoot=base/"another-runtime/Mods/RuneSchema";ModLicensePanel::Context();Check(ModLicensePanel::Selection.empty()&&ModLicensePanel::Data.Notices.empty(),"Runtime change retained another installation's selected license");
    ModLicensePanel::Shutdown();ImGui::DestroyContext();std::cout<<"Mouse/controller selection, footer layout, asynchronous selection changes and runtime isolation passed.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
