#pragma once
#include "Core/NativeInventoryMarkClick.h"
#include "Core/NativeInventoryMarkInput.h"
#include "Core/RuneSchemaDataRoot.h"
#include "Core/ConfigFiles.h"
#include "Core/LegacyInventoryFavorites.h"
#include "Runtime/HostServices.h"
#include "SDK/Helper/NativeInventoryWidgets.h"
#include "SDK/Helper/InventoryMarkService.h"
#include "SDK/WeakObjectHandle.h"
#include <Unreal/Hooks.hpp>
#include "Utility/NativeFunctionHook.h"
#include <windows.h>
namespace PS {
class NativeInventoryMarks {
    using Object=RC::Unreal::UObject;using Function=RC::Unreal::UFunction;using Json=nlohmann::json;
    using Clock=std::chrono::steady_clock;using UI=NativeInventoryWidgets;
    struct Boundary{Function* FunctionPtr{};int Hook=0;};
    struct Marker{WeakObjectHandle Slot,Button,Text;Json Item=Json::object();uint64_t Revision=UINT64_MAX;std::string Label;};
    std::vector<Boundary> Hooks;
    std::vector<Marker> Markers;
    WeakObjectHandle Panel;
    std::function<std::string(Object*)> Identify;
    NativeInventoryMarkClick Click;
    NativeInventoryMarkPoll Poll;
    NativeInventoryMarkControls Controls;
    std::filesystem::path ControlsFile;
    std::string ControlsBaseline;
    bool ControlsReady=false,ControlsExisted=false;
    std::set<std::string> LegacyFavorites,LegacyChecked;
    void LoadLegacyFavorites(){
        const auto saved=RuneSchemaData::GameSavedDirectory()/"InventoryFavorites-favorites.txt";
        const auto script=std::filesystem::path(HostServices::WorkingDirectory())/"Mods"/"InventoryFavorites"/"Scripts"/"favorites.txt";
        for(const auto& file:{saved,std::filesystem::path(saved.wstring()+L".bak"),script,std::filesystem::path(script.wstring()+L".bak")}){
            try{if(!std::filesystem::exists(file))continue;RuneSchemaData::Safe(file);LegacyFavorites=LegacyInventoryFavorites::Parse(ConfigFiles::Read(file,1024*1024));return;}
            catch(const std::exception& error){Warn(error);}
        }
    }
    void CancelClicks(){Click.Reset();Poll={};InventoryMarkInput.Reset();Controls.CaptureUntil={};}
    void LoadControls(){
        ControlsFile=HostServices::ProfileDirectory()/"inventory-controls.json";RuneSchemaData::Safe(ControlsFile);
        ControlsExisted=std::filesystem::exists(ControlsFile);
        if(ControlsExisted){ControlsBaseline=ConfigFiles::Read(ControlsFile,1024);Controls=NativeInventoryMarkControls::Parse(Json::parse(ControlsBaseline));}
        ControlsReady=true;
    }
    void SaveControls(){
        if(!ControlsReady)throw std::runtime_error("Inventory controls were not loaded; original file retained");
        RuneSchemaData::Safe(ControlsFile);std::filesystem::create_directories(ControlsFile.parent_path());
        const auto path=std::filesystem::path(ControlsFile.wstring()+L".lock");RuneSchemaData::Safe(path);
        const auto handle=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Inventory controls are busy in another game session");
        struct Close{HANDLE Value;~Close(){CloseHandle(Value);}}close{handle};
        const bool existed=std::filesystem::exists(ControlsFile);
        if(existed!=ControlsExisted||(existed&&ConfigFiles::Read(ControlsFile,1024)!=ControlsBaseline))throw std::runtime_error("Inventory controls changed in another session; restart to reload");
        const auto next=Controls.Json().dump(2);ConfigFiles::Write(ControlsFile,next);ControlsBaseline=next;ControlsExisted=true;
    }
    Clock::time_point NextDiscover{},NextRefresh{},NextCapabilities{},NextWarning{},NextRetry{};
    uint64_t Revision=0;bool Available=false,Discover=true,WasActive=false;
    size_t Cursor=0;DWORD Thread=GetCurrentThreadId();
    static bool Foreground(){DWORD process=0;GetWindowThreadProcessId(GetForegroundWindow(),&process);return process==GetCurrentProcessId();}
    static bool Visible(Object* widget){if(!UI::Alive(widget))return false;EquipmentDelivery::Call c(widget,TEXT("/Script/UMG.UserWidget:GetIsVisible"),1);c.Invoke();return c.Bool();}
    void Warn(const std::exception& error){const auto now=Clock::now();if(now<NextWarning)return;NextWarning=now+std::chrono::seconds(10);Log<RC::LogLevel::Warning>(STR("[HELPY][MARKS] {}\n"),RC::to_generic_string(error.what()));}
    void Observe(Object* slot){
        if(!UI::Alive(slot))return;auto* panel=UI::Ref(slot,TEXT("InventoryPanel"));if(!UI::Alive(panel))return;
        if(Panel.Get()!=panel){Reset();Panel=WeakObjectHandle(panel);}
        for(const auto& marker:Markers)if(marker.Slot.Get()==slot)return;
        if(Markers.size()<256){Marker marker;marker.Slot=WeakObjectHandle(slot);Markers.push_back(std::move(marker));}
    }
    Json Item(Object* slot,Object* controller,const std::string& character){
        namespace E=EquipmentDelivery;
        if(!UI::Alive(slot)||UI::Ref(slot,TEXT("InventoryPanel"))!=Panel.Get())return Json::object();
        E::Call owner(slot,TEXT("/Script/UMG.Widget:GetOwningPlayer"),1);owner.Invoke();if(owner.Object()!=controller)return Json::object();
        E::Call contained(slot,TEXT("/Script/Dominion.InventorySlotBase:GetContainedItem"),1);contained.Invoke();auto* item=contained.Object();if(!UI::Alive(item))return Json::object();
        auto* bag=E::Owned(controller,TEXT("InventoryComponent"),TEXT("/Script/Dominion.InventoryComponent"));
        size_t copies=0;for(auto* candidate:E::Slots(bag))if(candidate==item)++copies;if(copies!=1)return Json::object();
        auto* data=E::Data(item);if(!UI::Alive(data))return Json::object();
        return {{"Path",RC::to_string(data->GetPathName())},{"PlayerGuid",character},{"Instance",E::Identity(item)}};
    }
    static void Visibility(Object* widget,bool visible){EquipmentDelivery::AH::FunctionCall(widget,TEXT("/Script/UMG.Widget:SetVisibility")).JsonArg(TEXT("InVisibility"),visible?"Visible":"Collapsed").Invoke();}
    void Attach(Marker& marker){
        using namespace RC::Unreal;namespace E=EquipmentDelivery;
        auto* slot=marker.Slot.Get();auto* tree=UI::Ref(slot,TEXT("WidgetTree"));auto* image=UI::Ref(slot,TEXT("ItemImage"));
        auto* overlayType=E::AH::ResolveClass(TEXT("/Script/UMG.Overlay"));
        if(!tree||!image||!overlayType)throw std::runtime_error("Native inventory marker layout unavailable");
        auto* parent=image;
        for(int i=0;i<12;++i){E::Call get(parent,TEXT("/Script/UMG.Widget:GetParent"),1);get.Invoke();parent=get.Object();if(!UI::Alive(parent)||parent->IsA(overlayType))break;}
        if(!UI::Alive(parent)||!parent->IsA(overlayType))throw std::runtime_error("Native inventory marker overlay changed");
        auto* button=UI::Node(tree,TEXT("/Script/UMG.Button"));button->SetRootSet();marker.Button=WeakObjectHandle(button);
        try{
            auto* focus=CastField<FBoolProperty>(button->GetClassPrivate()->GetPropertyByNameInChain(TEXT("IsFocusable")));
            if(!focus||!E::C::Fits(focus,button->GetClassPrivate()->GetPropertiesSize()))throw std::runtime_error("Native marker focus flag unavailable");focus->SetPropertyValue(focus->ContainerPtrToValuePtr<void>(button),false);
            auto* box=UI::Node(tree,TEXT("/Script/UMG.SizeBox"));auto* text=UI::Node(tree,TEXT("/Script/UMG.TextBlock"));
            const Json padding={{"Left",0},{"Top",0},{"Right",0},{"Bottom",0}};
            const Json brush={{"DrawAs","NoDrawType"}};
            UI::CopyStyle(button,TEXT("/Script/UMG.Button:SetStyle"),TEXT("InStyle"),button,TEXT("WidgetStyle"),TEXT("/Script/SlateCore.ButtonStyle"),{{"Normal",brush},{"Hovered",brush},{"Pressed",brush},{"Disabled",brush},{"NormalPadding",padding},{"PressedPadding",padding}});
            E::AH::FunctionCall(button,TEXT("/Script/UMG.Button:SetBackgroundColor")).JsonArg(TEXT("InBackgroundColor"),{{"R",0},{"G",0},{"B",0},{"A",0}}).Invoke();
            // The game's stack-count font can omit star glyphs; use UMG's default font.
            UI::CopyStyle(text,TEXT("/Script/UMG.TextBlock:SetFont"),TEXT("InFontInfo"),text,TEXT("Font"),TEXT("/Script/SlateCore.SlateFontInfo"),{{"Size",12}});
            E::AH::FunctionCall(text,TEXT("/Script/UMG.TextBlock:SetJustification")).JsonArg(TEXT("InJustification"),"Center").Invoke();
            E::AH::FunctionCall(text,TEXT("/Script/UMG.Widget:SetVisibility")).JsonArg(TEXT("InVisibility"),"HitTestInvisible").Invoke();
            E::AH::FunctionCall(box,TEXT("/Script/UMG.SizeBox:SetWidthOverride")).Arg(TEXT("InWidthOverride"),20.f).Invoke();
            E::AH::FunctionCall(box,TEXT("/Script/UMG.SizeBox:SetHeightOverride")).Arg(TEXT("InHeightOverride"),20.f).Invoke();
            UI::Add(box,text);UI::Add(button,box);auto* added=UI::Add(parent,button);
            E::AH::FunctionCall(added,TEXT("/Script/UMG.OverlaySlot:SetHorizontalAlignment")).JsonArg(TEXT("InHorizontalAlignment"),"HAlign_Right").Invoke();
            E::AH::FunctionCall(added,TEXT("/Script/UMG.OverlaySlot:SetVerticalAlignment")).JsonArg(TEXT("InVerticalAlignment"),"VAlign_Top").Invoke();
            E::AH::FunctionCall(added,TEXT("/Script/UMG.OverlaySlot:SetPadding")).JsonArg(TEXT("InPadding"),{{"Left",0},{"Top",2},{"Right",2},{"Bottom",0}}).Invoke();
            marker.Text=WeakObjectHandle(text);
        }catch(...){UI::Remove(button);marker.Button.Reset();throw;}
    }
    void Refresh(Marker& marker,const Json& item){
        namespace E=EquipmentDelivery;
        auto* button=marker.Button.Get();if(!button)return;
        if(marker.Item==item&&marker.Revision==Revision)return;
        Visibility(button,!item.empty());if(item.empty()){marker.Item=Json::object();return;}
        const auto reply=InventoryMarkService::Call({{"Action","Marks"},{"Item",item}});const auto& marks=reply.at("Marks");
        const bool favorite=marks.value("Favorite",false),junk=reply.at("Junk").get<bool>();
        const std::string label=favorite?"★":junk?"×":"☆";
        if(marker.Label!=label){
            E::AH::FunctionCall(marker.Text.Get(),TEXT("/Script/UMG.TextBlock:SetText")).JsonArg(TEXT("InText"),label).Invoke();
            const Json color={{"R",favorite?1.f:junk?.9f:.65f},{"G",favorite?.78f:junk?.32f:.65f},{"B",favorite?.25f:junk?.22f:.65f},{"A",1.f}};
            E::AH::FunctionCall(marker.Text.Get(),TEXT("/Script/UMG.TextBlock:SetColorAndOpacity")).JsonArg(TEXT("InColorAndOpacity"),{{"SpecifiedColor",color},{"ColorUseRule","UseColor_Specified"}}).Invoke();
            E::AH::FunctionCall(button,TEXT("/Script/UMG.Widget:SetToolTipText")).JsonArg(TEXT("InToolTipText"),std::string(favorite?"Favorite":junk?"Junk":"Unmarked")+" · click marker or use marking key: Favorite → Junk → Unmarked (this stack). Default: F6 / middle-click. Ctrl+F6: mouse binding; Ctrl+Shift+F6: rebind key. Marks survive storage moves.").Invoke();
            marker.Label=label;
        }
        marker.Item=item;marker.Revision=reply.at("Revision").get<uint64_t>();
    }
public:
    explicit NativeInventoryMarks(std::function<std::string(Object*)> identify):Identify(std::move(identify)){}
    void Install()noexcept{try{
        using namespace RC::Unreal;if(!Hooks.empty())return;
        try{LoadControls();}catch(const std::exception& error){Warn(error);}
        try{LoadLegacyFavorites();}catch(const std::exception& error){Warn(error);}
        for(const auto* path:{TEXT("/Script/Dominion.InventoryMainPanel:HandleToggle"),TEXT("/Script/Dominion.InventorySlotBase:OnItemHovered")}){
            auto* fn=UECustom::UObjectGlobals::StaticFindObject<UFunction*>(nullptr,nullptr,path,false);if(!CanHookNative(fn)||fn->HasAnyFunctionFlags(FUNC_Net))continue;
            const bool panel=fn->GetName()==TEXT("HandleToggle");
            const auto hook=RegisterNativePostHook(fn,[this,panel](auto& context,void*){if(GetCurrentThreadId()!=Thread)return;try{
                if(panel){if(Panel.Get()!=context.Context){Reset();Panel=WeakObjectHandle(context.Context);}Discover=true;}
                else {Observe(context.Context);Poll.Next={};}
            }catch(const std::exception& error){Warn(error);}});if(hook)Hooks.push_back({fn,hook});
        }
    }catch(const std::exception& error){Warn(error);}}
    void Reset(bool detach=true)noexcept{for(auto& marker:Markers)UI::Remove(marker.Button.Get(),detach);Markers.clear();Panel.Reset();CancelClicks();WasActive=false;Discover=true;Cursor=0;}
    void Tick()noexcept{
        if(GetCurrentThreadId()!=Thread)return;
        try{
            using namespace RC::Unreal;namespace E=EquipmentDelivery;const auto now=Clock::now();if(now<NextRetry)return;
            auto* panel=Panel.Get();
            if(!UI::Alive(panel)&&now>=NextDiscover){
                NextDiscover=now+std::chrono::seconds(2);auto* type=E::AH::ResolveClass(TEXT("/Script/Dominion.InventoryMainPanel"));TArray<Object*> panels;
                if(type)UECustom::UObjectGlobals::GetObjectsOfClass(type,panels,true);if(panels.Num()>64)throw std::runtime_error("Native inventory panel bound exceeded");
                for(auto* candidate:panels)if(UI::Alive(candidate)&&UI::Bool(candidate,TEXT("bIsActive"))){Panel=WeakObjectHandle(candidate);panel=candidate;Discover=true;break;}
            }
            if(!UI::Alive(panel)||!UI::Bool(panel,TEXT("bIsActive"))||!Foreground()){CancelClicks();WasActive=false;return;}
            const bool down=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
            const auto inputEvents=InventoryMarkInput.Take(now);
            if(!Poll.Due(now,down,!inputEvents.empty(),Discover||!WasActive))return;
            if(!Visible(panel)){CancelClicks();WasActive=false;return;}
            E::Call owner(panel,TEXT("/Script/UMG.Widget:GetOwningPlayer"),1);owner.Invoke();auto* controller=owner.Object();if(!UI::Alive(controller)){CancelClicks();return;}
            E::Call local(controller,TEXT("/Script/Engine.Controller:IsLocalController"),1);local.Invoke();const auto character=local.Bool()?Identify(controller):std::string{};
            if(character.empty()){Reset();return;}
            if(!WasActive){Discover=true;WasActive=true;NextRefresh={};}
            if(now>=NextCapabilities){const auto caps=InventoryMarkService::Call({{"Action","Capabilities"}});Available=caps.value("GameFavorites",false);Revision=caps.at("MarksRevision").get<uint64_t>();NextCapabilities=now+std::chrono::seconds(1);}
            if(!Available){Reset();return;}
            if(Discover){
                std::erase_if(Markers,[](auto& marker){if(UI::Alive(marker.Slot.Get()))return false;UI::Remove(marker.Button.Get());return true;});
                auto* type=E::AH::ResolveClass(TEXT("/Script/Dominion.MainInventorySlotBase"));TArray<Object*> slots;
                if(type)UECustom::UObjectGlobals::GetObjectsOfClass(type,slots,true);if(slots.Num()>4096)throw std::runtime_error("Native inventory slot bound exceeded");
                for(auto* slot:slots)if(UI::Alive(slot)&&UI::Ref(slot,TEXT("InventoryPanel"))==panel)Observe(slot);Discover=false;
            }
            if(now>=NextRefresh){
                NextRefresh=now+std::chrono::milliseconds(100);size_t budget=16,created=0;Json imports=Json::array();std::set<std::string> checked;
                for(size_t n=0;n<Markers.size()&&budget;++n){auto& marker=Markers[Cursor++%Markers.size()];--budget;
                    if(!UI::Alive(marker.Slot.Get())){UI::Remove(marker.Button.Get());marker.Button.Reset();marker.Text.Reset();marker.Item=Json::object();continue;}
                    const auto item=Item(marker.Slot.Get(),controller,character);
                    if(!item.empty()&&!LegacyFavorites.empty()){
                        const auto id=LegacyInventoryFavorites::Key(item.at("Instance").get<EquipmentDelivery::Guid>());
                        const auto identity=character+":"+id;
                        if(LegacyFavorites.contains(id)&&!LegacyChecked.contains(identity)&&checked.insert(identity).second){
                            if(LegacyChecked.size()+checked.size()>16384)throw std::runtime_error("Legacy import session bound exceeded; originals retained");
                            imports.push_back(item);
                        }
                    }
                    if(!marker.Button.Get()){if(item.empty()||created>=8)continue;Attach(marker);++created;}
                    Refresh(marker,item);
                }
                if(!imports.empty()){
                    const auto result=InventoryMarkService::Call({{"Action","ImportLegacyFavorites"},{"Items",imports}});
                    LegacyChecked.insert(checked.begin(),checked.end());Revision=result.at("Revision").get<uint64_t>();
                    for(auto& marker:Markers)marker.Revision=UINT64_MAX;
                    if(result.at("Imported").get<unsigned>())Log<RC::LogLevel::Normal>(STR("[HELPY][MARKS][IMPORTED] {} legacy favorites matched the current character's inventory; original files retained.\n"),result.at("Imported").get<unsigned>());
                }
            }
            Marker* hovered=nullptr;Marker* itemHovered=nullptr;std::string key,itemKey;
            const auto targetKey=[](const Marker& marker){return std::to_string(marker.Button.ObjectIndex)+":"+std::to_string(marker.Button.ObjectSerialNumber)+":"+std::to_string(marker.Revision)+marker.Item.dump();};
            for(auto& marker:Markers)if(UI::Alive(marker.Slot.Get())&&UI::Alive(marker.Button.Get())&&!marker.Item.empty()){
                if(UI::Bool(marker.Slot.Get(),TEXT("bIsMousedOver"))){if(itemHovered){CancelClicks();return;}itemHovered=&marker;itemKey=targetKey(marker);}
                E::Call hover(marker.Button.Get(),TEXT("/Script/UMG.Widget:IsHovered"),1);hover.Invoke();if(!hover.Bool())continue;
                if(down){E::Call pressed(marker.Button.Get(),TEXT("/Script/UMG.Button:IsPressed"),1);pressed.Invoke();if(!pressed.Bool())continue;}
                hovered=&marker;key=targetKey(marker);
            }
            auto clicked=Click.Step(down,key);
            for(const auto& event:inputEvents){
                const auto previous=Controls;const auto action=Controls.Handle(event,now);
                using Action=NativeInventoryMarkControls::Action;
                if(action==Action::Changed){try{SaveControls();}catch(...){Controls=previous;throw;}
                    Log<RC::LogLevel::Normal>(STR("[HELPY][MARKS][CONTROLS] Saved key={} mouse={} in the shared profile. Right-click also retains the game's normal use/equip action.\n"),Controls.Key,Controls.Mouse);}
                else if(action==Action::Capture)Log<RC::LogLevel::Normal>(STR("[HELPY][MARKS][CONTROLS] Press a marking key within 8 seconds; Escape cancels.\n"));
                else if(action==Action::Cancel)Log<RC::LogLevel::Normal>(STR("[HELPY][MARKS][CONTROLS] Rebinding cancelled.\n"));
                else if(action==Action::Cycle&&clicked.empty()&&!down&&!itemKey.empty()&&event.Target==itemKey){clicked=itemKey;hovered=itemHovered;}
            }
            InventoryMarkInput.Publish(itemKey,now);
            if(!clicked.empty()&&hovered){
                const auto item=Item(hovered->Slot.Get(),controller,character);if(item!=hovered->Item){CancelClicks();NextRefresh={};return;}
                const auto result=InventoryMarkService::Call({{"Action","CycleMark"},{"Scope","Stack"},{"Item",item},{"Revision",hovered->Revision}});
                Revision=result.at("Revision").get<uint64_t>();hovered->Revision=UINT64_MAX;Refresh(*hovered,item);
            }
        }catch(const std::exception& error){CancelClicks();NextCapabilities={};NextRetry=Clock::now()+std::chrono::seconds(1);Warn(error);}
    }
    // DLL shutdown must not call UMG after the engine has stopped.
    ~NativeInventoryMarks(){Reset(false);for(const auto& boundary:Hooks)try{boundary.FunctionPtr->UnregisterHook(boundary.Hook);}catch(...){} }
};
}
