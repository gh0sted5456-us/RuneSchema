#pragma once
#include "Core/RuneSchemaDataRoot.h"
#include "imgui.h"
#include "Core/ConfigFiles.h"
#include "Core/ItemBayLayout.h"
#include "Core/ItemPaging.h"
#include "Runtime/HostServices.h"
#include "Generator/HelpyRuntimePreferences.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <stdexcept>

namespace PS::HelpyImGuiTheme {

inline float RuntimeDpi=1.0f;
inline void SetRuntimeDpi(float dpi){RuntimeDpi=std::clamp(dpi,0.75f,3.0f);}
inline ImVec4 Hex(unsigned value){return {((value>>16)&255)/255.0f,((value>>8)&255)/255.0f,(value&255)/255.0f,1.0f};}

struct Theme {
    // Shared visual language with Dragonwilds Sync: quiet slate, soft rune
    // accent, restrained secondary controls, one contrasting primary action.
    ImVec4 window=Hex(0x181e29),panel=Hex(0x1c2330),panelAlt=Hex(0x212938);
    ImVec4 navigation=Hex(0x141923),navigationHover=Hex(0x19212d),navigationActive=Hex(0x292b3c);
    ImVec4 accent=Hex(0xb8a1ef),accentHover=Hex(0xcbb9f6),accentActive=Hex(0xa18bd7),onAccent=Hex(0x1b142a);
    ImVec4 text=Hex(0xf1f3fb),textDisabled=Hex(0x9aa7ba);
    ImVec4 control=Hex(0x19212d),controlHover=Hex(0x293446),controlActive=Hex(0x303b4e);
    ImVec4 button=Hex(0x293344),buttonHover=Hex(0x354259),buttonActive=Hex(0x414d64);
    ImVec4 selection=Hex(0x3a3554),border=Hex(0x303b4e),separator=Hex(0x303b4e);
    ImVec4 scrollbarTrack=Hex(0x141923),scrollbar=Hex(0x495873),scrollbarHover=Hex(0x657493),scrollbarActive=Hex(0x8d8ac0);
    ImVec4 icon=Hex(0xb8a1ef);
    ImVec4 success{0.35f,0.75f,0.50f,1.0f};
    ImVec4 warning{0.95f,0.70f,0.28f,1.0f};
    ImVec4 error{0.94f,0.34f,0.38f,1.0f};
    ImVec4 info{0.36f,0.67f,0.95f,1.0f};
    ImVec4 scrollFade{24.0f/255,30.0f/255,41.0f/255,0.86f};

    float userScale{1.18f},fontScale{1.34f},density{1.14f};
    float frameRounding{6.0f},childRounding{8.0f},scrollbarSize{11.0f};
    float framePaddingX{8.0f},framePaddingY{4.0f},itemSpacingX{8.0f},itemSpacingY{6.0f};
    float sectionSpacing{10.0f},scrollFadeHeight{18.0f},navigationWidth{184.0f},navigationMinWidth{126.0f},navigationMaxWidth{320.0f};
    float contentGutter{12.0f},contentPaddingX{10.0f},contentPaddingY{6.0f},splitterWidth{6.0f},minWindowWidth{760.0f},minWindowHeight{560.0f};
    float resultListHeight{320.0f},listIconSize{28.0f},gridIconSize{60.0f},gridCardHeight{116.0f},gridMinCardWidth{132.0f};
    float navigationItemHeight{36.0f},navigationIconSize{17.0f},loadingSpinnerRadius{13.0f},loadingSpinnerThickness{2.4f},tooltipDelay{0.35f};
    int overscanRows{4};
    int itemGridColumns{ItemBayLayout::DefaultColumns},itemGridRows{ItemBayLayout::DefaultRows};
    bool itemGridFitRows{true};
    int itemPageSize{ItemPaging::DefaultSize};
    bool dpiAware{true},autoReload{true},textBold{false},verboseTooltips{false},itemGridView{true},aiGridView{false},advancedMode{false};
    float itemResultHeight{320.0f},aiResultHeight{320.0f};
    std::string itemCategory{"All"};
    std::string fontFamily{"Segoe UI"};

    float Dpi() const {
        if(!dpiAware)return 1.0f;float dpi=1.0f;
#if defined(IMGUI_HAS_VIEWPORT)
        if(const auto* vp=ImGui::GetMainViewport();vp&&vp->DpiScale>0.0f)dpi=vp->DpiScale;
#endif
        const auto fb=ImGui::GetIO().DisplayFramebufferScale;dpi=std::max({dpi,std::max(fb.x,fb.y),RuntimeDpi});
        return std::clamp(dpi,0.75f,3.0f);
    }
    float Scale() const{return std::clamp(Dpi()*userScale,0.65f,3.0f);}
    float X(float value) const{return value*Scale();}
    float Y(float value) const{return value*Scale()*density;}
};

inline Theme Current{};
inline std::atomic<bool> ForceReload{true};
inline bool Loaded=false,HaveStamp=false;
inline std::filesystem::file_time_type Stamp{};
inline std::chrono::steady_clock::time_point NextPoll{};
inline std::string Status="Theme not loaded yet.";

inline bool UseUserAppData = true;
inline std::filesystem::path RootThemePath() {
    return HostServices::WorkingDirectory()/"Mods"/"RuneSchema"/"plugins"/"RuneSchema.Helpy"/"settings"/"ui.jsonc";
}
inline std::filesystem::path UserThemePath() {
    return RuneSchemaData::SharedDirectory("HELPY")/"ui.jsonc";
}
inline std::filesystem::path FilePath(){return UseUserAppData?UserThemePath():RootThemePath();}
inline void SelectSettingsSource(bool appData){
    if(UseUserAppData==appData)return;
    UseUserAppData=appData;
    Loaded=false;HaveStamp=false;ForceReload=true;
}

inline nlohmann::json LegacyDefaultDocument() {
    return {
        {"version",17},{"autoReload",true},
        {"display",{{"dpiAware",true},{"userScale",1.18},{"fontScale",1.34},{"density",1.14},{"fontFamily","Segoe UI"},{"textBold",false},{"verboseTooltips",false},{"tooltipDelay",0.35}}},
        {"views",{{"advancedMode",false},{"itemGrid",true},{"itemPageSize",ItemPaging::DefaultSize},{"itemGridColumns",ItemBayLayout::DefaultColumns},{"itemGridRows",ItemBayLayout::DefaultRows},{"itemGridFitRows",true},{"aiGrid",false},{"itemResultHeight",320.0},{"aiResultHeight",320.0},{"itemCategory","All"}}},
        {"cache",{{"iconCacheMB",200}}},
        {"colors",{
            {"window",{0.050,0.052,0.060,1.0}},{"panel",{0.075,0.078,0.090,1.0}},{"panelAlt",{0.100,0.104,0.120,1.0}},
            {"navigation",{0.062,0.064,0.074,1.0}},{"navigationHover",{0.125,0.115,0.155,1.0}},{"navigationActive",{0.155,0.115,0.225,1.0}},
            {"accent",{0.48,0.28,0.78,1.0}},{"accentHover",{0.60,0.39,0.92,1.0}},{"accentActive",{0.39,0.20,0.68,1.0}},
            {"text",{0.94,0.94,0.96,1.0}},{"textDisabled",{0.56,0.57,0.62,1.0}},
            {"control",{0.125,0.128,0.150,1.0}},{"controlHover",{0.175,0.178,0.205,1.0}},{"controlActive",{0.22,0.20,0.29,1.0}},
            {"button",{0.145,0.140,0.170,1.0}},{"buttonHover",{0.48,0.28,0.78,1.0}},{"buttonActive",{0.39,0.20,0.68,1.0}},
            {"selection",{0.31,0.20,0.50,1.0}},{"border",{0.24,0.22,0.31,1.0}},{"separator",{0.20,0.20,0.24,1.0}},
            {"scrollbarTrack",{0.055,0.058,0.067,1.0}},{"scrollbar",{0.26,0.24,0.32,1.0}},
            {"scrollbarHover",{0.42,0.31,0.58,1.0}},{"scrollbarActive",{0.56,0.37,0.76,1.0}},
            {"icon",{0.78,0.74,0.86,1.0}},{"success",{0.35,0.75,0.50,1.0}},{"warning",{0.95,0.70,0.28,1.0}},
            {"error",{0.94,0.34,0.38,1.0}},{"info",{0.36,0.67,0.95,1.0}},{"scrollFade",{0.050,0.052,0.060,0.86}}
        }},
        {"layout",{
            {"frameRounding",3.0},{"childRounding",4.0},{"scrollbarSize",11.0},{"framePadding",{8.0,4.0}},
            {"itemSpacing",{8.0,6.0}},{"sectionSpacing",10.0},{"scrollFadeHeight",18.0},{"navigationWidth",184.0},{"navigationMinWidth",126.0},{"navigationMaxWidth",320.0},{"contentGutter",12.0},{"contentPaddingX",10.0},{"contentPaddingY",6.0},{"splitterWidth",6.0},{"minWindowWidth",760.0},{"minWindowHeight",560.0},{"resultListHeight",320.0},{"listIconSize",28.0},{"gridIconSize",60.0},{"gridCardHeight",116.0},{"gridMinCardWidth",132.0},{"navigationItemHeight",36.0},{"navigationIconSize",17.0},{"loadingSpinnerRadius",13.0},{"loadingSpinnerThickness",2.4},{"overscanRows",4}
        }}
    };
}

inline ImVec4 ReadColor(const nlohmann::json& colors,const char* name,const ImVec4& fallback) {
    const auto it=colors.find(name);if(it==colors.end()||!it->is_array()||(it->size()!=3&&it->size()!=4))return fallback;
    try {const auto f=[&](size_t i,float d){return std::clamp((*it)[i].get<float>(),0.0f,1.0f);};
        return {f(0,fallback.x),f(1,fallback.y),f(2,fallback.z),it->size()==4?f(3,fallback.w):fallback.w};}
    catch(...){return fallback;}
}
inline float ReadFloat(const nlohmann::json& obj,const char* name,float fallback,float lo,float hi) {
    try {const auto it=obj.find(name);return it!=obj.end()&&it->is_number()?std::clamp(it->get<float>(),lo,hi):fallback;}catch(...){return fallback;}
}
inline int ReadInt(const nlohmann::json& obj,const char* name,int fallback,int lo,int hi) {
    try {const auto it=obj.find(name);return it!=obj.end()&&it->is_number_integer()?std::clamp(it->get<int>(),lo,hi):fallback;}catch(...){return fallback;}
}
inline void ReadPair(const nlohmann::json& obj,const char* name,float& x,float& y,float lo,float hi) {
    try {const auto it=obj.find(name);if(it!=obj.end()&&it->is_array()&&it->size()==2){x=std::clamp((*it)[0].get<float>(),lo,hi);y=std::clamp((*it)[1].get<float>(),lo,hi);}}catch(...){}
}

inline nlohmann::json ColorJson(const ImVec4& v){return nlohmann::json::array({v.x,v.y,v.z,v.w});}
inline nlohmann::json DocumentFrom(const Theme& t,int iconCacheMB) {
    return {
        {"version",18},{"autoReload",t.autoReload},
        {"display",{{"dpiAware",t.dpiAware},{"userScale",t.userScale},{"fontScale",t.fontScale},{"density",t.density},{"fontFamily",t.fontFamily},{"textBold",t.textBold},{"verboseTooltips",t.verboseTooltips},{"tooltipDelay",t.tooltipDelay}}},
        {"views",{{"advancedMode",t.advancedMode},{"itemGrid",t.itemGridView},{"itemPageSize",t.itemPageSize},{"itemGridColumns",t.itemGridColumns},{"itemGridRows",t.itemGridRows},{"itemGridFitRows",t.itemGridFitRows},{"aiGrid",t.aiGridView},{"itemResultHeight",t.itemResultHeight},{"aiResultHeight",t.aiResultHeight},{"itemCategory",t.itemCategory}}},
        {"cache",{{"iconCacheMB",std::clamp(iconCacheMB,0,16384)}}},
        {"colors",{
            {"window",ColorJson(t.window)},{"panel",ColorJson(t.panel)},{"panelAlt",ColorJson(t.panelAlt)},
            {"navigation",ColorJson(t.navigation)},{"navigationHover",ColorJson(t.navigationHover)},{"navigationActive",ColorJson(t.navigationActive)},
            {"accent",ColorJson(t.accent)},{"accentHover",ColorJson(t.accentHover)},{"accentActive",ColorJson(t.accentActive)},{"onAccent",ColorJson(t.onAccent)},
            {"text",ColorJson(t.text)},{"textDisabled",ColorJson(t.textDisabled)},
            {"control",ColorJson(t.control)},{"controlHover",ColorJson(t.controlHover)},{"controlActive",ColorJson(t.controlActive)},
            {"button",ColorJson(t.button)},{"buttonHover",ColorJson(t.buttonHover)},{"buttonActive",ColorJson(t.buttonActive)},
            {"selection",ColorJson(t.selection)},{"border",ColorJson(t.border)},{"separator",ColorJson(t.separator)},
            {"scrollbarTrack",ColorJson(t.scrollbarTrack)},{"scrollbar",ColorJson(t.scrollbar)},{"scrollbarHover",ColorJson(t.scrollbarHover)},{"scrollbarActive",ColorJson(t.scrollbarActive)},
            {"icon",ColorJson(t.icon)},{"success",ColorJson(t.success)},{"warning",ColorJson(t.warning)},{"error",ColorJson(t.error)},{"info",ColorJson(t.info)},{"scrollFade",ColorJson(t.scrollFade)}
        }},
        {"layout",{
            {"frameRounding",t.frameRounding},{"childRounding",t.childRounding},{"scrollbarSize",t.scrollbarSize},
            {"framePadding",{t.framePaddingX,t.framePaddingY}},{"itemSpacing",{t.itemSpacingX,t.itemSpacingY}},
            {"sectionSpacing",t.sectionSpacing},{"scrollFadeHeight",t.scrollFadeHeight},{"navigationWidth",t.navigationWidth},
            {"navigationMinWidth",t.navigationMinWidth},{"navigationMaxWidth",t.navigationMaxWidth},{"contentGutter",t.contentGutter},{"contentPaddingX",t.contentPaddingX},{"contentPaddingY",t.contentPaddingY},
            {"splitterWidth",t.splitterWidth},{"minWindowWidth",t.minWindowWidth},{"minWindowHeight",t.minWindowHeight},
            {"resultListHeight",t.resultListHeight},{"listIconSize",t.listIconSize},{"gridIconSize",t.gridIconSize},
            {"gridCardHeight",t.gridCardHeight},{"gridMinCardWidth",t.gridMinCardWidth},{"navigationItemHeight",t.navigationItemHeight},{"navigationIconSize",t.navigationIconSize},{"loadingSpinnerRadius",t.loadingSpinnerRadius},{"loadingSpinnerThickness",t.loadingSpinnerThickness},{"overscanRows",t.overscanRows}
        }}
    };
}
inline nlohmann::json DefaultDocument(){return DocumentFrom(Theme{},200);}
inline void UseSyncColors(){
    const auto source=DocumentFrom(Theme{},200).at("colors");
#define SYNC_COLOR(name) Current.name=ReadColor(source,#name,Current.name)
    SYNC_COLOR(window);SYNC_COLOR(panel);SYNC_COLOR(panelAlt);SYNC_COLOR(navigation);SYNC_COLOR(navigationHover);SYNC_COLOR(navigationActive);
    SYNC_COLOR(accent);SYNC_COLOR(accentHover);SYNC_COLOR(accentActive);SYNC_COLOR(onAccent);SYNC_COLOR(text);SYNC_COLOR(textDisabled);
    SYNC_COLOR(control);SYNC_COLOR(controlHover);SYNC_COLOR(controlActive);SYNC_COLOR(button);SYNC_COLOR(buttonHover);SYNC_COLOR(buttonActive);
    SYNC_COLOR(selection);SYNC_COLOR(border);SYNC_COLOR(separator);SYNC_COLOR(scrollbarTrack);SYNC_COLOR(scrollbar);SYNC_COLOR(scrollbarHover);SYNC_COLOR(scrollbarActive);SYNC_COLOR(icon);SYNC_COLOR(scrollFade);
#undef SYNC_COLOR
}
inline void RequestReload(){ForceReload=true;}
inline void SaveCurrent() {
    const auto file=FilePath();
    try {
        ConfigFiles::Write(file,"// Helpy ImGui skin. Settings UI and this file share the same live values.\n// Set cache.iconCacheMB to 0 for session-only live icon caching.\n"+DocumentFrom(Current,HelpyRuntimePreferences::IconCacheLimitMB.load()).dump(2)+"\n");
        std::error_code ec;const auto stamp=std::filesystem::last_write_time(file,ec);if(!ec){Stamp=stamp;HaveStamp=true;}Loaded=true;Status="Saved "+file.string();
    } catch(const std::exception& e) {Status=std::string("Theme save failed: ")+e.what();}
}
inline void ResetDefaults() {Current=Theme{};HelpyRuntimePreferences::IconCacheLimitMB.store(200);SaveCurrent();}
inline void WriteDefault(const std::filesystem::path& file) {
    ConfigFiles::Write(file,"// Helpy ImGui skin. Edit while Dragonwilds is running.\n// Searches execute on Enter; style saves auto-reload. F5 forces refresh.\n"+DefaultDocument().dump(2)+"\n");
}
inline void Reload(bool createIfMissing=true) {
    const auto file=FilePath();
    try {
        if(!std::filesystem::is_regular_file(file)){
            if(!createIfMissing)throw std::runtime_error("ui.jsonc does not exist");
            if(UseUserAppData&&std::filesystem::is_regular_file(RootThemePath())){
                std::filesystem::create_directories(file.parent_path());
                std::filesystem::copy_file(RootThemePath(),file,std::filesystem::copy_options::skip_existing);
            }else WriteDefault(file);
        }
        const auto doc=nlohmann::json::parse(ConfigFiles::Read(file),nullptr,true,true);Theme next{};next.autoReload=doc.value("autoReload",true);const int version=doc.value("version",0);
        const auto display=doc.value("display",nlohmann::json::object());
        const auto cache=doc.value("cache",nlohmann::json::object());HelpyRuntimePreferences::IconCacheLimitMB.store(ReadInt(cache,"iconCacheMB",200,0,16384));
        next.dpiAware=display.value("dpiAware",true);next.userScale=ReadFloat(display,"userScale",1.18f,0.60f,2.50f);
        next.fontScale=ReadFloat(display,"fontScale",1.34f,0.70f,1.75f);next.density=ReadFloat(display,"density",1.14f,0.65f,1.50f);try{next.fontFamily=display.value("fontFamily",std::string("Segoe UI"));}catch(...){next.fontFamily="Segoe UI";}next.textBold=display.value("textBold",false);next.verboseTooltips=display.value("verboseTooltips",false);next.tooltipDelay=ReadFloat(display,"tooltipDelay",next.tooltipDelay,0.0f,2.0f);
        const auto views=doc.value("views",nlohmann::json::object());next.advancedMode=views.value("advancedMode",false);next.itemGridView=views.value("itemGrid",true);next.aiGridView=views.value("aiGrid",false);next.itemResultHeight=ReadFloat(views,"itemResultHeight",320.0f,180.0f,900.0f);next.aiResultHeight=ReadFloat(views,"aiResultHeight",320.0f,180.0f,900.0f);try{const auto value=views.value("itemCategory",std::string("All"));if(value=="All"||value=="Weapons"||value=="Armor"||value=="Consumables"||value=="Ammo"||value=="Resources"||value=="Tools"||value=="Building"||value=="Quest"||value=="Misc")next.itemCategory=value;}catch(...){}
        next.itemGridColumns=ReadInt(views,"itemGridColumns",ItemBayLayout::DefaultColumns,1,ItemBayLayout::MaxColumns);
        next.itemPageSize=ReadInt(views,"itemPageSize",ItemPaging::DefaultSize,1,ItemPaging::MaximumSize);
        next.itemGridRows=ReadInt(views,"itemGridRows",ItemBayLayout::DefaultRows,1,ItemBayLayout::MaxRows);
        next.itemGridFitRows=views.value("itemGridFitRows",true);
        if(version<4){if(std::abs(next.userScale-1.10f)<0.001f)next.userScale=1.18f;if(std::abs(next.fontScale-1.04f)<0.001f)next.fontScale=1.18f;if(std::abs(next.density-1.06f)<0.001f)next.density=1.14f;}
        if(version<5&&std::abs(next.fontScale-1.14f)<0.001f)next.fontScale=1.18f;
        if(version<6&&std::abs(next.fontScale-1.18f)<0.001f)next.fontScale=1.34f;
        auto c=doc.value("colors",nlohmann::json::object());
        if(version<18){
            const auto legacy=LegacyDefaultDocument().at("colors");bool unchanged=c.size()==legacy.size();
            for(const auto& [key,value]:legacy.items()){
                if(!unchanged)break;const auto old=ReadColor(legacy,key.c_str(),{}),saved=ReadColor(c,key.c_str(),{});
                unchanged=std::abs(old.x-saved.x)<0.0001f&&std::abs(old.y-saved.y)<0.0001f&&std::abs(old.z-saved.z)<0.0001f&&std::abs(old.w-saved.w)<0.0001f;
            }
            if(unchanged)c=DefaultDocument().at("colors");
        }
        if(version<18&&!c.contains("onAccent"))next.onAccent=ReadColor(c,"text",next.text);
#define HCOL(name) next.name=ReadColor(c,#name,next.name)
        HCOL(window);HCOL(panel);HCOL(panelAlt);HCOL(navigation);HCOL(navigationHover);HCOL(navigationActive);HCOL(accent);HCOL(accentHover);HCOL(accentActive);HCOL(onAccent);HCOL(text);HCOL(textDisabled);
        HCOL(control);HCOL(controlHover);HCOL(controlActive);HCOL(button);HCOL(buttonHover);HCOL(buttonActive);HCOL(selection);HCOL(border);HCOL(separator);HCOL(scrollbarTrack);HCOL(scrollbar);
        HCOL(scrollbarHover);HCOL(scrollbarActive);HCOL(icon);HCOL(success);HCOL(warning);HCOL(error);HCOL(info);HCOL(scrollFade);
#undef HCOL
        const auto l=doc.value("layout",nlohmann::json::object());
        next.frameRounding=ReadFloat(l,"frameRounding",next.frameRounding,0.0f,20.0f);next.childRounding=ReadFloat(l,"childRounding",next.childRounding,0.0f,20.0f);
        next.scrollbarSize=ReadFloat(l,"scrollbarSize",next.scrollbarSize,6.0f,28.0f);next.sectionSpacing=ReadFloat(l,"sectionSpacing",next.sectionSpacing,0.0f,40.0f);
        next.scrollFadeHeight=ReadFloat(l,"scrollFadeHeight",next.scrollFadeHeight,0.0f,64.0f);next.navigationWidth=ReadFloat(l,"navigationWidth",next.navigationWidth,112.0f,360.0f);if(version<4&&std::abs(next.navigationWidth-164.0f)<0.001f)next.navigationWidth=184.0f;
        next.navigationMinWidth=ReadFloat(l,"navigationMinWidth",next.navigationMinWidth,96.0f,280.0f);next.navigationMaxWidth=ReadFloat(l,"navigationMaxWidth",next.navigationMaxWidth,next.navigationMinWidth,480.0f);
        next.contentGutter=ReadFloat(l,"contentGutter",next.contentGutter,0.0f,40.0f);next.contentPaddingX=ReadFloat(l,"contentPaddingX",next.contentPaddingX,0.0f,40.0f);next.contentPaddingY=ReadFloat(l,"contentPaddingY",next.contentPaddingY,0.0f,24.0f);if(version<12&&std::abs(next.contentPaddingX-18.0f)<0.001f)next.contentPaddingX=10.0f;if(version<12&&std::abs(next.contentPaddingY-8.0f)<0.001f)next.contentPaddingY=6.0f;next.splitterWidth=ReadFloat(l,"splitterWidth",next.splitterWidth,3.0f,16.0f);
        next.minWindowWidth=ReadFloat(l,"minWindowWidth",next.minWindowWidth,560.0f,1600.0f);next.minWindowHeight=ReadFloat(l,"minWindowHeight",next.minWindowHeight,420.0f,1200.0f);next.resultListHeight=ReadFloat(l,"resultListHeight",next.resultListHeight,180.0f,900.0f);next.listIconSize=ReadFloat(l,"listIconSize",next.listIconSize,16.0f,96.0f);next.gridIconSize=ReadFloat(l,"gridIconSize",next.gridIconSize,28.0f,160.0f);next.gridCardHeight=ReadFloat(l,"gridCardHeight",next.gridCardHeight,72.0f,260.0f);next.gridMinCardWidth=ReadFloat(l,"gridMinCardWidth",next.gridMinCardWidth,92.0f,320.0f);next.navigationItemHeight=ReadFloat(l,"navigationItemHeight",next.navigationItemHeight,24.0f,72.0f);next.navigationIconSize=ReadFloat(l,"navigationIconSize",next.navigationIconSize,10.0f,40.0f);next.loadingSpinnerRadius=ReadFloat(l,"loadingSpinnerRadius",next.loadingSpinnerRadius,6.0f,40.0f);next.loadingSpinnerThickness=ReadFloat(l,"loadingSpinnerThickness",next.loadingSpinnerThickness,1.0f,8.0f);next.overscanRows=ReadInt(l,"overscanRows",next.overscanRows,0,16);
        ReadPair(l,"framePadding",next.framePaddingX,next.framePaddingY,0.0f,24.0f);ReadPair(l,"itemSpacing",next.itemSpacingX,next.itemSpacingY,0.0f,30.0f);
        Current=next;Stamp=std::filesystem::last_write_time(file);HaveStamp=true;Loaded=true;Status="Loaded "+file.string();
    } catch(const std::exception& e) {
        std::error_code ec;const auto stamp=std::filesystem::last_write_time(file,ec);if(!ec){Stamp=stamp;HaveStamp=true;}
        Status=std::string("Theme reload failed; keeping last valid skin: ")+e.what();
    }
}
inline void Poll() {
    const bool forced=ForceReload.exchange(false);const auto now=std::chrono::steady_clock::now();
    if(!forced&&now<NextPoll)return;NextPoll=now+std::chrono::milliseconds(250);
    if(!Loaded){Reload(true);return;}if(!forced&&!Current.autoReload)return;
    const auto file=FilePath();std::error_code ec;const auto stamp=std::filesystem::last_write_time(file,ec);
    if(forced||ec||!HaveStamp||stamp!=Stamp)Reload(true);
}

struct Scope {
    int colors=0,vars=0;
    explicit Scope(const Theme& t) {
        const auto pc=[&](ImGuiCol col,const ImVec4& v){ImGui::PushStyleColor(col,v);++colors;};
        const auto pv=[&](ImGuiStyleVar var,float v){ImGui::PushStyleVar(var,v);++vars;};
        const auto pv2=[&](ImGuiStyleVar var,float x,float y){ImGui::PushStyleVar(var,ImVec2(x,y));++vars;};
        pc(ImGuiCol_Text,t.text);pc(ImGuiCol_TextDisabled,t.textDisabled);pc(ImGuiCol_WindowBg,t.window);pc(ImGuiCol_ChildBg,t.panel);pc(ImGuiCol_PopupBg,t.panelAlt);
        pc(ImGuiCol_Border,t.border);pc(ImGuiCol_FrameBg,t.control);pc(ImGuiCol_FrameBgHovered,t.controlHover);pc(ImGuiCol_FrameBgActive,t.controlActive);
        pc(ImGuiCol_Button,t.button);pc(ImGuiCol_ButtonHovered,t.buttonHover);pc(ImGuiCol_ButtonActive,t.buttonActive);
        pc(ImGuiCol_Header,t.control);pc(ImGuiCol_HeaderHovered,t.accentHover);pc(ImGuiCol_HeaderActive,t.accentActive);
        pc(ImGuiCol_CheckMark,t.accentHover);pc(ImGuiCol_SliderGrab,t.accent);pc(ImGuiCol_SliderGrabActive,t.accentHover);
        pc(ImGuiCol_ScrollbarBg,t.scrollbarTrack);pc(ImGuiCol_ScrollbarGrab,t.scrollbar);pc(ImGuiCol_ScrollbarGrabHovered,t.scrollbarHover);pc(ImGuiCol_ScrollbarGrabActive,t.scrollbarActive);
        pc(ImGuiCol_Separator,t.separator);pc(ImGuiCol_TextSelectedBg,t.selection);
        pv2(ImGuiStyleVar_FramePadding,t.X(t.framePaddingX),t.Y(t.framePaddingY));pv2(ImGuiStyleVar_ItemSpacing,t.X(t.itemSpacingX),t.Y(t.itemSpacingY));
        pv(ImGuiStyleVar_FrameRounding,t.X(t.frameRounding));pv(ImGuiStyleVar_ChildRounding,t.X(t.childRounding));pv(ImGuiStyleVar_ScrollbarSize,t.X(t.scrollbarSize));
        pv(ImGuiStyleVar_ScrollbarRounding,t.X(t.frameRounding));pv(ImGuiStyleVar_GrabRounding,t.X(t.frameRounding));
    }
    ~Scope(){if(vars)ImGui::PopStyleVar(vars);if(colors)ImGui::PopStyleColor(colors);}
    Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
};

inline bool TooltipReady(std::string_view key,bool hovered) {
    static std::string activeKey;static double hoverStart=0.0;
    if(!hovered){if(activeKey==key)activeKey.clear();return false;}
    if(activeKey!=key){activeKey.assign(key.data(),key.size());hoverStart=ImGui::GetTime();return Current.tooltipDelay<=0.0f;}
    return (ImGui::GetTime()-hoverStart)>=static_cast<double>(Current.tooltipDelay);
}

inline bool SearchIconButton(const char* id,float logicalSize=0.0f) {
    const auto& t=Current;const float side=logicalSize>0.0f?t.Y(logicalSize):ImGui::GetFrameHeight();
    ImGui::PushID(id);const bool clicked=ImGui::InvisibleButton("##search",ImVec2(side,side));
    const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();auto* draw=ImGui::GetWindowDrawList();
    const ImU32 bg=ImGui::GetColorU32(ImGui::IsItemActive()?t.buttonActive:ImGui::IsItemHovered()?t.buttonHover:t.button);
    draw->AddRectFilled(a,b,bg,t.X(t.frameRounding));
    const float r=side*0.20f;const ImVec2 c(a.x+side*0.45f,a.y+side*0.43f);const ImU32 fg=ImGui::GetColorU32(t.text);const float w=std::max(1.0f,t.X(1.5f));
    draw->AddCircle(c,r,fg,16,w);draw->AddLine(ImVec2(c.x+r*0.72f,c.y+r*0.72f),ImVec2(a.x+side*0.76f,a.y+side*0.76f),fg,w);
    ImGui::PopID();return clicked;
}

inline bool BeginScroll(const char* id,const ImVec2& size=ImVec2(0,0),bool border=false) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(Current.X(Current.contentPaddingX),Current.Y(Current.contentPaddingY)));
    return ImGui::BeginChild(id,size,border,ImGuiWindowFlags_AlwaysVerticalScrollbar|ImGuiWindowFlags_AlwaysUseWindowPadding);
}
inline void EndScroll(const Theme& t) {
    const float y=ImGui::GetScrollY(),max=ImGui::GetScrollMaxY(),h=t.Y(t.scrollFadeHeight);
    if(h>0.5f&&max>0.5f) {
        auto* draw=ImGui::GetWindowDrawList();const auto p=ImGui::GetWindowPos();const auto s=ImGui::GetWindowSize();const float right=p.x+s.x-t.X(t.scrollbarSize);
        const ImU32 solid=ImGui::GetColorU32(t.scrollFade),clear=ImGui::GetColorU32(ImVec4(t.scrollFade.x,t.scrollFade.y,t.scrollFade.z,0.0f));
        if(y>1.0f)draw->AddRectFilledMultiColor(p,ImVec2(right,p.y+h),solid,solid,clear,clear);
        if(y<max-1.0f)draw->AddRectFilledMultiColor(ImVec2(p.x,p.y+s.y-h),ImVec2(right,p.y+s.y),clear,clear,solid,solid);
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
}
enum class Icon : unsigned char { Ai, Resource, Item, Lab, Building, Cleanup, Appearance, Info };
inline void DrawIcon(Icon icon,const ImVec2& c,float size,const ImVec4& tint=Current.icon) {
    auto* d=ImGui::GetWindowDrawList();const ImU32 col=ImGui::GetColorU32(tint);const float r=size*0.5f,w=std::max(1.0f,size*0.09f);
    switch(icon) {
    case Icon::Ai:d->AddCircle(ImVec2(c.x,c.y-r*.28f),r*.25f,col,10,w);d->AddLine(ImVec2(c.x,c.y),ImVec2(c.x,c.y+r*.42f),col,w);d->AddLine(ImVec2(c.x-r*.32f,c.y+r*.15f),ImVec2(c.x+r*.32f,c.y+r*.15f),col,w);break;
    case Icon::Resource:d->AddQuad(ImVec2(c.x,c.y-r*.55f),ImVec2(c.x+r*.48f,c.y),ImVec2(c.x,c.y+r*.55f),ImVec2(c.x-r*.48f,c.y),col,w);break;
    case Icon::Item:d->AddRect(ImVec2(c.x-r*.46f,c.y-r*.34f),ImVec2(c.x+r*.46f,c.y+r*.38f),col,0,0,w);d->AddLine(ImVec2(c.x-r*.46f,c.y-r*.34f),ImVec2(c.x,c.y-r*.58f),col,w);d->AddLine(ImVec2(c.x+r*.46f,c.y-r*.34f),ImVec2(c.x,c.y-r*.58f),col,w);break;
    case Icon::Lab:d->AddLine(ImVec2(c.x-r*.16f,c.y-r*.55f),ImVec2(c.x-r*.16f,c.y-r*.10f),col,w);d->AddLine(ImVec2(c.x+r*.16f,c.y-r*.55f),ImVec2(c.x+r*.16f,c.y-r*.10f),col,w);d->AddLine(ImVec2(c.x-r*.16f,c.y-r*.10f),ImVec2(c.x-r*.42f,c.y+r*.48f),col,w);d->AddLine(ImVec2(c.x+r*.16f,c.y-r*.10f),ImVec2(c.x+r*.42f,c.y+r*.48f),col,w);d->AddLine(ImVec2(c.x-r*.42f,c.y+r*.48f),ImVec2(c.x+r*.42f,c.y+r*.48f),col,w);d->AddLine(ImVec2(c.x-r*.22f,c.y+r*.18f),ImVec2(c.x+r*.24f,c.y+r*.18f),col,w);break;
    case Icon::Building:d->AddLine(ImVec2(c.x-r*.52f,c.y-r*.05f),ImVec2(c.x,c.y-r*.55f),col,w);d->AddLine(ImVec2(c.x,c.y-r*.55f),ImVec2(c.x+r*.52f,c.y-r*.05f),col,w);d->AddRect(ImVec2(c.x-r*.38f,c.y-r*.05f),ImVec2(c.x+r*.38f,c.y+r*.48f),col,0,0,w);break;
    case Icon::Cleanup:d->AddLine(ImVec2(c.x-r*.30f,c.y-r*.55f),ImVec2(c.x+r*.05f,c.y+r*.22f),col,w);d->AddTriangle(ImVec2(c.x-r*.05f,c.y+r*.12f),ImVec2(c.x+r*.52f,c.y+r*.20f),ImVec2(c.x+r*.30f,c.y+r*.55f),col,w);break;
    case Icon::Appearance:d->AddCircle(c,r*.25f,col,10,w);for(int i=0;i<4;++i){const float a=i*1.5707963f;d->AddLine(ImVec2(c.x+std::cos(a)*r*.38f,c.y+std::sin(a)*r*.38f),ImVec2(c.x+std::cos(a)*r*.58f,c.y+std::sin(a)*r*.58f),col,w);}break;
    case Icon::Info:d->AddCircle(c,r*.52f,col,12,w);d->AddCircleFilled(ImVec2(c.x,c.y-r*.22f),w,col);d->AddLine(ImVec2(c.x,c.y-r*.02f),ImVec2(c.x,c.y+r*.30f),col,w);break;
    }
}
inline bool SectionHeader(const char* label,Icon icon,ImGuiTreeNodeFlags flags=0) {
    const bool open=ImGui::CollapsingHeader(label,flags);const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();const float size=std::min(Current.Y(15.0f),b.y-a.y-Current.Y(4.0f));
    if(size>2.0f)DrawIcon(icon,ImVec2(b.x-Current.X(12.0f),(.5f*(a.y+b.y))),size);return open;
}
inline void SectionGap(const Theme& t){ImGui::Dummy(ImVec2(1.0f,t.Y(t.sectionSpacing)));}

}
