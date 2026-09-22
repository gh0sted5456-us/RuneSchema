#include "FakeHost.h"
#include "Generator/QuickMenuUI.h"
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define private public
#include "Generator/InGameQuickMenu.h"
#undef private
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#include <iostream>
using namespace RC;using namespace RC::Unreal;using namespace DragonWilds;
namespace PS {namespace {
std::map<std::wstring,UFunction*> functions;
UObject* selectedController=nullptr;
bool drawFails=false,sizeFails=false,assetsMissing=false,installFails=false;
int frames=0,diagnosticRects=0,diagnosticTexts=0;
void* ResolvePostRender(){return reinterpret_cast<void*>(std::uintptr_t{1});}
UFunction* CanvasFunction(const TCHAR* path){auto p=functions.find(path);return p==functions.end()?nullptr:p->second;}
std::pair<float,float> ViewportExtent(UObject*,UObject*){if(sizeFails)throw std::runtime_error("no dimensions");return {1920,1080};}
UObject* MenuWhiteTexture(){return assetsMissing?nullptr:selectedController;}
UObject* MenuFont(){return assetsMissing?nullptr:selectedController;}
void DrawRect(UObject*,float,float,float,float,float,float,float,float){if(drawFails)throw std::runtime_error("draw failure");++diagnosticRects;}
void DrawText(UObject*,const std::string&,float,float,float,float,float,float){if(drawFails)throw std::runtime_error("text failure");++diagnosticTexts;}
#include "QuickMenuInputAdapter.inl"
}
bool InstallInlineHook(SafetyHookInline& hook,void*,void*){hook.installed=!installFails;return !installFails;}
void InGameQuickMenu::RemoveInputHook()noexcept{m_inputHook=nullptr;m_window.store(nullptr);}
void InGameQuickMenu::EnsureInputHook(){m_window.store(reinterpret_cast<HWND>(std::uintptr_t{1}));}
void InGameQuickMenu::RenderCanvas(UObject*){if(drawFails)throw std::runtime_error("draw failure");++frames;}
UObject* InGameQuickMenu::FindLocalPlayerController(UObject* v){return selectedController&&selectedController->world==v->world?selectedController:nullptr;}
#include "ProductionLifecycle.inc"
}
using namespace PS;
int checks=0;
#define CHECK(x) do{++checks;if(!(x))throw std::runtime_error(std::string("CHECK failed line ")+std::to_string(__LINE__)+": "+#x);}while(0)
template<class F>void Reject(F f){bool thrown=false;try{f();}catch(const std::exception&){thrown=true;}CHECK(thrown);}
struct Fixture {
    UClass viewportClass,controllerClass,worldClass,canvasClass,libraryClass,widgetClass,pawnClass;
    UObject viewport,controller,world,canvas,library,pawn;
    FBoolProperty lookParam,moveParam,lookResult,moveResult,cursor,flushUI,flushGame;
    FObjectProperty pcUI,focusUI,pcGame;
    FEnumProperty mouseLock;
    UEnum mouseEnum;
    UFunction lookFn,moveFn,lookQuery,moveQuery,uiFn,gameFn,otherFn,drawFn;
    InGameQuickMenu menu;
    Fixture(){
        viewport.type=&viewportClass;controller.type=&controllerClass;world.type=&worldClass;canvas.type=&canvasClass;
        library.type=&libraryClass;library.flags=RF_ClassDefaultObject;pawn.type=&pawnClass;
        viewport.world=&world;controller.world=&world;canvas.world=&world;pawn.world=&world;controller.pawn=&pawn;
        fakeObjects={&viewport,&controller,&world,&canvas,&library,&pawn};
        ActorHelper::classes={{L"/Script/Engine.PlayerController",&controllerClass},{L"/Script/UMG.Widget",&widgetClass},{L"/Script/UMG.WidgetBlueprintLibrary",&libraryClass}};
        UECustom::UObjectGlobals::library=&library;
        cursor.offset=121;cursor.mask=8;controllerClass.fields[L"bShowMouseCursor"]=&cursor;
        lookParam.flags=CPF_Parm;moveParam.flags=CPF_Parm;lookFn.kind=UFunction::Look;moveFn.kind=UFunction::Move;
        lookFn.fields[L"bNewLookInput"]=&lookParam;moveFn.fields[L"bNewMoveInput"]=&moveParam;
        lookResult.flags=CPF_Parm|CPF_OutParm|CPF_ReturnParm;moveResult.flags=lookResult.flags;
        lookQuery.kind=UFunction::LookQuery;moveQuery.kind=UFunction::MoveQuery;
        lookQuery.fields[L"ReturnValue"]=&lookResult;moveQuery.fields[L"ReturnValue"]=&moveResult;
        pcUI.flags=CPF_Parm;pcUI.propertyClass.value=&controllerClass;pcUI.offset=0;
        focusUI.flags=CPF_Parm;focusUI.propertyClass.value=&widgetClass;focusUI.offset=8;
        mouseLock.flags=CPF_Parm;mouseLock.offset=16;mouseLock.enumeration=&mouseEnum;
        // A nonzero value intentionally proves no assumed EMouseLockMode byte.
        mouseEnum.names={{FName(L"EMouseLockMode::DoNotLock"),7},{FName(L"EMouseLockMode::LockAlways"),8}};
        flushUI.flags=CPF_Parm;flushUI.offset=17;
        uiFn.kind=UFunction::UIOnly;uiFn.parms=18;uiFn.fields={{L"PlayerController",&pcUI},{L"InWidgetToFocus",&focusUI},{L"InMouseLockMode",&mouseLock},{L"bFlushInput",&flushUI}};
        pcGame=pcUI;flushGame.flags=CPF_Parm;flushGame.offset=8;
        gameFn.kind=UFunction::GameOnly;gameFn.parms=9;gameFn.fields={{L"PlayerController",&pcGame},{L"bFlushInput",&flushGame}};
        otherFn=uiFn;otherFn.kind=UFunction::GameAndUI;
        functions={{L"/Script/Engine.Controller:SetIgnoreLookInput",&lookFn},{L"/Script/Engine.Controller:SetIgnoreMoveInput",&moveFn},
            {L"/Script/Engine.Controller:IsLookInputIgnored",&lookQuery},{L"/Script/Engine.Controller:IsMoveInputIgnored",&moveQuery},
            {L"/Script/UMG.WidgetBlueprintLibrary:SetInputMode_UIOnlyEx",&uiFn},{L"/Script/UMG.WidgetBlueprintLibrary:SetInputMode_GameOnly",&gameFn},
            {L"/Script/UMG.WidgetBlueprintLibrary:SetInputMode_GameAndUIEx",&otherFn},
            {L"/Script/Engine.Canvas:K2_DrawText",&drawFn},{L"/Script/Engine.Canvas:K2_DrawTexture",&drawFn}};
        fakeObjectVirtualsAvailable=false;fakeObjectVirtualCalls=0;selectedController=&controller;installFails=false;Hook::failTick=false;Hook::failWorld=false;Hook::failEvents=false;
        Hook::tickCallbacks.clear();Hook::worldCallbacks.clear();Hook::eventCallbacks.clear();InGameQuickMenu::s_postRenderHook={};
        fakeProtect=PAGE_READWRITE;fakeQueryFails=false;fakeWindowLive=true;fakeClipFails=false;fakeClipCalls=0;
        fakeWindow=reinterpret_cast<HWND>(std::uintptr_t{1});fakeNow=100;fakeThread=42;
        drawFails=false;sizeFails=false;assetsMissing=false;frames=0;diagnosticRects=0;diagnosticTexts=0;fakeLogThrows=false;fakeLogs.clear();
        menu.m_window.store(fakeWindow);InGameQuickMenu::s_activeMenu.store(&menu);
    }
    ~Fixture(){fakeWindow=reinterpret_cast<HWND>(std::uintptr_t{1});gameFn.failBefore=false;gameFn.failAfter=false;controller.throwMove=false;controller.throwLook=false;menu.Shutdown();}
    bool cursorOn(){return cursor.GetPropertyValue(cursor.ContainerPtrToValuePtr<void>(&controller));}
    void open(){menu.Toggle();InGameQuickMenu::PostRenderThunk(&viewport,&canvas);}
    void close(){menu.Close();menu.TickInputSafety();}
};
void TestNativeModes(){
    Fixture f;auto native=PrepareNativeMenuMode(true);CHECK(native.doNotLock==7);CHECK(native.function==&f.uiFn);
    const auto viewportBefore=f.viewport.storage;
    f.menu.Initialize();f.open();CHECK(f.menu.IsOpen());CHECK(f.menu.m_inputReady.load());CHECK(frames==1);
    CHECK(f.controller.uiCalls==1);CHECK(f.controller.gameCalls==0);CHECK(f.controller.inputMode==1);CHECK(f.controller.flushes==1);
    CHECK(f.controller.lookCount==1&&f.controller.moveCount==1);CHECK(f.cursorOn());CHECK(f.menu.m_modeObserverActive.load());
    CHECK(f.viewport.storage==viewportBefore);CHECK(fakeClipCalls==0);
    InGameQuickMenu::PostRenderThunk(&f.viewport,&f.canvas);CHECK(f.controller.uiCalls==1);CHECK(frames==2);
    f.close();CHECK(!f.menu.IsOpen());CHECK(!f.menu.HasInputLease());CHECK(f.controller.gameCalls==1);CHECK(f.controller.flushes==2);
    CHECK(f.controller.inputMode==0);CHECK(!f.cursorOn());CHECK(f.controller.lookCount==0&&f.controller.moveCount==0);
    CHECK(!f.menu.m_modeObserverActive.load());f.open();CHECK(f.controller.uiCalls==2);f.close();CHECK(f.controller.gameCalls==2);
    CHECK(f.pcUI.initialized==f.pcUI.destroyed);CHECK(f.focusUI.initialized==f.focusUI.destroyed);CHECK(f.flushUI.initialized==f.flushUI.destroyed);
}
void TestContractFailures(){
    {Fixture f;UECustom::UObjectGlobals::library=nullptr;f.open();CHECK(f.menu.m_inputDiagnostic.load());CHECK(!f.menu.HasInputLease());CHECK(frames==0);CHECK(diagnosticRects==1);}
    for(const auto* path:{L"/Script/UMG.WidgetBlueprintLibrary:SetInputMode_UIOnlyEx",L"/Script/UMG.WidgetBlueprintLibrary:SetInputMode_GameOnly"}){
        Fixture f;functions.erase(path);f.open();CHECK(f.menu.m_inputDiagnostic.load());CHECK(!f.menu.m_inputReady.load());CHECK(f.controller.uiCalls==0);CHECK(!f.menu.HasInputLease());}
    {Fixture f;f.library.flags=0;Reject([&]{PrepareNativeMenuMode(true);});CHECK(f.controller.uiCalls==0);}
    {Fixture f;f.uiFn.flags=FUNC_Native;Reject([&]{PrepareNativeMenuMode(true);});}
    {Fixture f;f.uiFn.flags=FUNC_Static;Reject([&]{PrepareNativeMenuMode(true);});}
    {Fixture f;f.pcUI.propertyClass.value=&f.widgetClass;Reject([&]{PrepareNativeMenuMode(true);});}
    {Fixture f;f.pcUI.size=4;Reject([&]{PrepareNativeMenuMode(true);});}
    {Fixture f;f.focusUI.propertyClass.value=&f.controllerClass;Reject([&]{PrepareNativeMenuMode(true);});}
    {Fixture f;f.flushUI.flags|=CPF_OutParm;Reject([&]{PrepareNativeMenuMode(true);});}
    {Fixture f;f.uiFn.fields.erase(L"InWidgetToFocus");Reject([&]{PrepareNativeMenuMode(true);});}
    {Fixture f;f.uiFn.fields[L"Unknown"]=&f.lookParam;Reject([&]{PrepareNativeMenuMode(true);});}
    {Fixture f;f.flushUI.offset=16;Reject([&]{PrepareNativeMenuMode(true);});}
    for(int offset:{-1,18,1000}){Fixture f;f.flushUI.offset=offset;Reject([&]{PrepareNativeMenuMode(true);});}
    for(int size:{0,257}){Fixture f;f.uiFn.parms=size;Reject([&]{PrepareNativeMenuMode(true);});}
    {Fixture f;f.mouseEnum.names.clear();Reject([&]{PrepareNativeMenuMode(true);});}
    {Fixture f;f.mouseEnum.names.push_back({FName(L"DoNotLock"),7});Reject([&]{PrepareNativeMenuMode(true);});}
    {Fixture f;f.mouseEnum.names[0].Value=256;Reject([&]{PrepareNativeMenuMode(true);});}
    {Fixture f;f.mouseLock.number.integer=false;Reject([&]{PrepareNativeMenuMode(true);});}
    {Fixture f;FNumericProperty byte;byte.offset=16;byte.flags=CPF_Parm;byte.enumeration=&f.mouseEnum;f.uiFn.fields[L"InMouseLockMode"]=&byte;
        auto call=PrepareNativeMenuMode(true);InvokeNativeMenuMode(&f.controller,call);CHECK(f.controller.uiCalls==1);CHECK(byte.initialized==byte.destroyed);}
    {Fixture f;f.pcUI.flags|=CPF_UObjectWrapper;Reject([&]{PrepareNativeMenuMode(true);});CHECK(f.controller.uiCalls==0);CHECK(!f.menu.HasInputLease());}
    {Fixture f;f.mouseLock.number.skipWrite=true;auto call=PrepareNativeMenuMode(true);Reject([&]{InvokeNativeMenuMode(&f.controller,call);});CHECK(f.controller.uiCalls==0);}
    {Fixture f;f.flushUI.failInit=true;auto call=PrepareNativeMenuMode(true);Reject([&]{InvokeNativeMenuMode(&f.controller,call);});CHECK(f.pcUI.initialized==f.pcUI.destroyed);CHECK(f.focusUI.initialized==f.focusUI.destroyed);CHECK(f.controller.uiCalls==0);}
}
void TestBusyAndReadiness(){
    {Fixture f;f.cursor.SetPropertyValue(f.cursor.ContainerPtrToValuePtr<void>(&f.controller),true);f.open();CHECK(!f.menu.IsOpen());CHECK(f.cursorOn());CHECK(f.controller.uiCalls==0);}
    {Fixture f;f.controller.lookCount=1;f.open();CHECK(!f.menu.IsOpen());CHECK(f.controller.lookCount==1);CHECK(f.controller.uiCalls==0);}
    {Fixture f;f.controller.moveCount=2;f.open();CHECK(!f.menu.IsOpen());CHECK(f.controller.moveCount==2);}
    {Fixture f;f.controller.pawn=nullptr;f.open();CHECK(!f.menu.IsOpen());CHECK(!f.menu.HasInputLease());}
    {Fixture f;f.pawn.world=nullptr;f.open();CHECK(!f.menu.IsOpen());}
    {Fixture f;selectedController=nullptr;f.open();CHECK(!f.menu.IsOpen());}
    {Fixture f;sizeFails=true;f.open();CHECK(!f.menu.HasInputLease());CHECK(f.controller.uiCalls==0);}
    {Fixture f;assetsMissing=true;f.open();CHECK(!f.menu.HasInputLease());CHECK(f.controller.uiCalls==0);}
    {Fixture f;f.controller.failWeak=true;f.open();CHECK(!f.menu.HasInputLease());CHECK(f.controller.lookCount==0);}
    {Fixture f;functions.erase(L"/Script/Engine.Controller:SetIgnoreMoveInput");f.open();CHECK(!f.menu.HasInputLease());CHECK(f.controller.uiCalls==0);CHECK(!f.cursorOn());}
}
void TestCleanup(){
    {Fixture f;drawFails=true;f.open();CHECK(!f.menu.IsOpen());CHECK(!f.menu.HasInputLease());CHECK(f.controller.uiCalls==1);CHECK(f.controller.gameCalls==1);CHECK(!f.cursorOn());}
    {Fixture f;f.uiFn.failBefore=true;f.open();CHECK(!f.menu.HasInputLease());CHECK(f.controller.uiCalls==0);CHECK(f.controller.gameCalls==1);CHECK(f.controller.lookCount==0);}
    {Fixture f;f.uiFn.failAfter=true;f.open();CHECK(!f.menu.HasInputLease());CHECK(f.controller.uiCalls==1);CHECK(f.controller.gameCalls==1);CHECK(f.controller.inputMode==0);CHECK(s_nativeMenuModeDepth==0);}
    {Fixture f;f.controller.throwMove=true;f.open();CHECK(!f.menu.HasInputLease());CHECK(f.controller.lookCount==0);CHECK(!f.cursorOn());}
    {Fixture f;f.open();f.gameFn.failBefore=true;f.close();CHECK(f.menu.HasInputLease());CHECK(!f.menu.m_inputReady.load());CHECK(f.controller.lookCount==0&&f.controller.moveCount==0);CHECK(!f.cursorOn());
        f.gameFn.failBefore=false;f.menu.TickInputSafety();CHECK(!f.menu.HasInputLease());CHECK(f.controller.gameCalls==1);}
    {Fixture f;f.open();fakeWindow=reinterpret_cast<HWND>(std::uintptr_t{2});f.menu.TickInputSafety();CHECK(!f.menu.IsOpen());CHECK(f.menu.HasInputLease());CHECK(f.controller.gameCalls==0);CHECK(!f.cursorOn());
        CHECK(f.controller.lookCount==0&&f.controller.moveCount==0);fakeWindow=reinterpret_cast<HWND>(std::uintptr_t{1});f.menu.TickInputSafety();CHECK(!f.menu.HasInputLease());CHECK(f.controller.gameCalls==1);}
    {Fixture f;f.open();f.controller.serial++;f.close();CHECK(!f.menu.HasInputLease());CHECK(f.controller.gameCalls==0);}
    {Fixture f;f.open();f.menu.ResetWorld();CHECK(!f.menu.HasInputLease());CHECK(!f.menu.IsOpen());CHECK(SpawnToolRequests::CancelRequested.load());}
    {Fixture f;f.open();fakeNow+=5000;f.menu.TickInputSafety();CHECK(!f.menu.IsOpen());CHECK(!f.menu.HasInputLease());}
    {Fixture f;f.menu.Toggle();fakeNow+=5000;f.menu.TickInputSafety();CHECK(!f.menu.IsOpen());CHECK(f.controller.uiCalls==0);}
}
void TestOwnershipAndMessages(){
    {Fixture f;f.menu.Initialize();f.open();CHECK(f.menu.IsOpen());CHECK(Hook::eventCallbacks.size()==1);
        MenuParameterBuffer args({&f.pcUI,&f.focusUI,&f.mouseLock,&f.flushUI});WriteMenuObjectParameter(&f.uiFn,&f.pcUI,args.Data(),256,&f.controller);
        f.library.ProcessEvent(&f.otherFn,args.Data());CHECK(!f.menu.IsOpen());CHECK(!f.menu.m_nativeModeOwned);CHECK(f.controller.inputMode==2);f.menu.TickInputSafety();
        CHECK(f.controller.gameCalls==0);CHECK(f.cursorOn());CHECK(f.controller.lookCount==0&&f.controller.moveCount==0);CHECK(!f.menu.HasInputLease());}
    {Fixture f;f.menu.Initialize();f.open();UObject other;other.type=&f.controllerClass;
        MenuParameterBuffer args({&f.pcUI,&f.focusUI,&f.mouseLock,&f.flushUI});WriteMenuObjectParameter(&f.uiFn,&f.pcUI,args.Data(),256,&other);
        f.library.ProcessEvent(&f.otherFn,args.Data());CHECK(f.menu.IsOpen());CHECK(f.menu.m_nativeModeOwned);}
    {Fixture f;f.open();MSG text{fakeWindow,WM_CHAR,65,0};InGameQuickMenu::InputThunk(0,PM_REMOVE,reinterpret_cast<LPARAM>(&text));CHECK(text.message==WM_NULL);CHECK(f.menu.m_events.size()==1);
        MSG f2{fakeWindow,WM_KEYDOWN,VK_F2,0};InGameQuickMenu::InputThunk(0,PM_REMOVE,reinterpret_cast<LPARAM>(&f2));CHECK(f2.message==WM_KEYDOWN);
        MSG lose{fakeWindow,WM_KILLFOCUS,0,0};InGameQuickMenu::InputThunk(0,PM_REMOVE,reinterpret_cast<LPARAM>(&lose));
        CHECK(f.menu.IsOpen());CHECK(f.menu.HasInputLease());
        fakeWindow=reinterpret_cast<HWND>(std::uintptr_t{2});f.menu.TickInputSafety();CHECK(!f.menu.IsOpen());
        fakeWindow=reinterpret_cast<HWND>(std::uintptr_t{1});f.menu.TickInputSafety();CHECK(!f.menu.HasInputLease());}
    {Fixture f;functions.erase(L"/Script/UMG.WidgetBlueprintLibrary:SetInputMode_UIOnlyEx");f.open();MSG click{fakeWindow,WM_LBUTTONUP,0,0};InGameQuickMenu::InputThunk(0,PM_REMOVE,reinterpret_cast<LPARAM>(&click));CHECK(click.message==WM_LBUTTONUP);CHECK(f.menu.m_events.empty());CHECK(!f.menu.HasInputLease());}
    for(int failure=0;failure<4;++failure){Fixture f;Hook::failTick=failure==0;Hook::failWorld=failure==1;Hook::failEvents=failure==2;installFails=failure==3;
        f.menu.Initialize();CHECK(Hook::tickCallbacks.empty());CHECK(Hook::worldCallbacks.empty());CHECK(Hook::eventCallbacks.empty());CHECK(!InGameQuickMenu::s_postRenderHook);}
    {Fixture f;f.menu.Initialize();f.menu.Shutdown();CHECK(Hook::tickCallbacks.empty());CHECK(Hook::worldCallbacks.empty());CHECK(Hook::eventCallbacks.empty());}
}

void TestUnavailableObjectVirtuals() {
    Fixture f;
    alignas(16) std::array<std::uint8_t,256> raw{};
    // Prove that the test host models this log's failure, not a working setter.
    Reject([&]{f.pcUI.SetObjectPropertyValue(raw.data(),&f.controller);});
    Reject([&]{f.pcUI.GetObjectPropertyValue(raw.data());});
    CHECK(fakeObjectVirtualCalls==2);
    fakeObjectVirtualCalls=0;
    f.menu.Initialize();f.open();
    CHECK(f.menu.IsOpen());CHECK(f.menu.m_inputReady.load());
    CHECK(f.controller.uiCalls==1);CHECK(fakeObjectVirtualCalls==0);
    // A native menu requesting input must also be read without the virtual getter.
    WriteMenuObjectParameter(&f.otherFn,&f.pcUI,raw.data(),raw.size(),&f.controller);
    f.library.ProcessEvent(&f.otherFn,raw.data());
    CHECK(!f.menu.IsOpen());f.menu.TickInputSafety();
    CHECK(!f.menu.HasInputLease());CHECK(f.controller.inputMode==2);
    CHECK(f.controller.gameCalls==0);CHECK(fakeObjectVirtualCalls==0);
    // Separate fixture exercises normal close and reopen under the same failure.
    {
        Fixture fresh;fresh.open();fresh.close();fresh.open();fresh.close();
        CHECK(fresh.controller.uiCalls==2);CHECK(fresh.controller.gameCalls==2);
        CHECK(!fresh.menu.HasInputLease());CHECK(!fresh.cursorOn());
        CHECK(fresh.controller.lookCount==0&&fresh.controller.moveCount==0);
        CHECK(fakeObjectVirtualCalls==0);
    }
}
void TestScalarArgumentBounds() {
    Fixture f;
    std::array<std::uint8_t,256> raw{};raw.fill(0xa5);
    const auto before=raw;
    WriteMenuObjectParameter(&f.uiFn,&f.pcUI,raw.data(),raw.size(),&f.controller);
    CHECK(ReadMenuObjectParameter(&f.uiFn,&f.pcUI,raw.data(),raw.size())==&f.controller);
    CHECK(std::equal(raw.begin()+sizeof(void*),raw.end(),before.begin()+sizeof(void*)));
    WriteMenuObjectParameter(&f.uiFn,&f.focusUI,raw.data(),raw.size(),nullptr);
    CHECK(ReadMenuObjectParameter(&f.uiFn,&f.focusUI,raw.data(),raw.size())==nullptr);
    CHECK(std::equal(raw.begin()+16,raw.end(),before.begin()+16));
    Reject([&]{WriteMenuObjectParameter(&f.uiFn,&f.pcUI,nullptr,256,&f.controller);});
    Reject([&]{ReadMenuObjectParameter(&f.uiFn,&f.pcUI,nullptr,256);});
    Reject([&]{WriteMenuObjectParameter(&f.uiFn,&f.pcUI,raw.data(),8,&f.controller);});
    Reject([&]{WriteMenuObjectParameter(&f.uiFn,&f.pcUI,raw.data(),256,&f.pawn);});
    f.controller.flags=RF_BeginDestroyed;
    Reject([&]{WriteMenuObjectParameter(&f.uiFn,&f.pcUI,raw.data(),256,&f.controller);});
    f.controller.flags=0;
    for(unsigned flag:{CPF_OutParm,CPF_ReturnParm,CPF_ReferenceParm,CPF_UObjectWrapper}) {
        f.pcUI.flags=CPF_Parm|flag;
        Reject([&]{WriteMenuObjectParameter(&f.uiFn,&f.pcUI,raw.data(),256,&f.controller);});
    }
    f.pcUI.flags=CPF_Parm;
    for(const wchar_t* type:{L"ClassProperty",L"SoftObjectProperty",L"WeakObjectProperty",L"ObjectPtrProperty"}) {
        f.pcUI.fieldClass.name=type;
        Reject([&]{PrepareNativeMenuMode(true);});
    }
    f.pcUI.fieldClass.name=L"ObjectProperty";
    FObjectProperty other=f.pcUI;
    Reject([&]{WriteMenuObjectParameter(&f.uiFn,&other,raw.data(),256,&f.controller);});
    f.pcUI.dim=2;Reject([&]{PrepareNativeMenuMode(true);});f.pcUI.dim=1;
    f.pcUI.flags=0;Reject([&]{PrepareNativeMenuMode(true);});f.pcUI.flags=CPF_Parm;
    for(int offset:{-1,11,18,256}) {
        f.pcUI.offset=offset;
        Reject([&]{WriteMenuObjectParameter(&f.uiFn,&f.pcUI,raw.data(),256,&f.controller);});
    }
    f.pcUI.offset=0;
    // Reordered, non-default offsets: no assumed UFUNCTION C++ struct layout.
    f.pcUI.offset=16;f.focusUI.offset=0;f.mouseLock.offset=8;f.flushUI.offset=9;f.uiFn.parms=24;
    const auto call=PrepareNativeMenuMode(true);InvokeNativeMenuMode(&f.controller,call);
    CHECK(f.controller.uiCalls==1);CHECK(fakeObjectVirtualCalls==0);
}
void TestPreDispatchOwnership() {
    {Fixture f;f.flushUI.failInitOn=2;f.open();CHECK(!f.menu.IsOpen());
        CHECK(!f.menu.HasInputLease());CHECK(!f.menu.m_modeObserverActive.load());
        CHECK(f.controller.uiCalls==0&&f.controller.gameCalls==0);
        CHECK(f.controller.lookCount==0&&f.controller.moveCount==0);CHECK(!f.cursorOn());}
    {Fixture f;f.flushUI.failInit=true;f.open();CHECK(!f.menu.IsOpen());
        CHECK(!f.menu.HasInputLease());CHECK(!f.menu.m_modeObserverActive.load());
        CHECK(f.controller.uiCalls==0&&f.controller.gameCalls==0);
        CHECK(f.controller.lookCount==0&&f.controller.moveCount==0);CHECK(!f.cursorOn());}
    {Fixture f;f.flushGame.failInit=true;f.open();CHECK(!f.menu.IsOpen());
        CHECK(!f.menu.HasInputLease());CHECK(!f.menu.m_modeObserverActive.load());
        CHECK(f.controller.uiCalls==0&&f.controller.gameCalls==0);CHECK(!f.cursorOn());}
    {Fixture f;const auto call=PrepareNativeMenuMode(true);f.flushUI.skipWrite=true;
        bool owned=false;Reject([&]{InvokeNativeMenuMode(&f.controller,call,&owned);});
        CHECK(!owned);CHECK(f.controller.uiCalls==0);}
    {Fixture f;f.mouseLock.number.skipWrite=true;f.open();
        CHECK(!f.menu.HasInputLease());CHECK(!f.menu.m_modeObserverActive.load());
        CHECK(f.controller.uiCalls==0&&f.controller.gameCalls==0);CHECK(!f.cursorOn());}
}

std::size_t CloseLogCount(){return static_cast<std::size_t>(std::count_if(fakeLogs.begin(),fakeLogs.end(),
    [](const auto& value){return value.find(L": closed [")!=std::wstring::npos;}));}
bool Logged(const std::wstring& text){return std::any_of(fakeLogs.begin(),fakeLogs.end(),
    [&](const auto& value){return value.find(text)!=std::wstring::npos;});}
void ExpectCloseReason(const std::wstring& reason){
    CHECK(CloseLogCount()==1);
    CHECK(Logged(L"[verbose] Helpy closed ["+reason+L"]"));
}
void TestFocusHandoffAndOtherViewports(){
    for(const auto msg:{WM_KILLFOCUS,WM_ACTIVATEAPP}){
        Fixture f;f.open();fakeLogs.clear();
        MSG handoff{fakeWindow,msg,0,0};
        InGameQuickMenu::InputThunk(0,PM_REMOVE,reinterpret_cast<LPARAM>(&handoff));
        CHECK(f.menu.IsOpen());CHECK(f.menu.m_inputReady.load());CHECK(CloseLogCount()==0);
        f.menu.TickInputSafety();CHECK(f.menu.IsOpen());CHECK(f.controller.uiCalls==1);CHECK(f.controller.gameCalls==0);
    }
    for(const auto msg:{WM_KILLFOCUS,WM_ACTIVATEAPP}){
        Fixture f;f.open();fakeLogs.clear();const auto game=fakeWindow;
        fakeWindow=reinterpret_cast<HWND>(std::uintptr_t{2});
        MSG lost{game,msg,0,0};
        InGameQuickMenu::InputThunk(0,PM_REMOVE,reinterpret_cast<LPARAM>(&lost));
        CHECK(!f.menu.IsOpen());CHECK(!f.menu.m_inputReady.load());ExpectCloseReason(L"foreground-lost");
        f.menu.TickInputSafety();CHECK(f.menu.HasInputLease());CHECK(f.controller.gameCalls==0);
        fakeWindow=game;f.menu.TickInputSafety();CHECK(!f.menu.HasInputLease());CHECK(f.controller.gameCalls==1);
        CHECK(CloseLogCount()==1);
    }
    {
        Fixture f;f.open();fakeLogs.clear();const auto generation=f.menu.m_generation.load();
        UObject otherViewport,otherWorld;otherViewport.type=&f.viewportClass;otherWorld.type=&f.worldClass;
        otherViewport.world=&otherWorld;
        InGameQuickMenu::PostRenderThunk(&otherViewport,&f.canvas);
        CHECK(f.menu.IsOpen());CHECK(f.menu.m_inputReady.load());CHECK(f.menu.m_generation.load()==generation);
        CHECK(f.menu.m_world.Get()==&f.world);CHECK(f.menu.m_lockedViewport.Get()==&f.viewport);
        CHECK(f.menu.m_completedFrames.load()==1);CHECK(f.controller.gameCalls==0);CHECK(CloseLogCount()==0);
        // A foreign callback must not fake a heartbeat for the owned viewport.
        fakeNow+=5000;InGameQuickMenu::PostRenderThunk(&otherViewport,&f.canvas);f.menu.TickInputSafety();
        CHECK(!f.menu.IsOpen());ExpectCloseReason(L"frame-watchdog");CHECK(!f.menu.HasInputLease());
    }
    {
        Fixture f;f.open();fakeLogs.clear();UObject nextWorld;nextWorld.type=&f.worldClass;
        f.viewport.world=&nextWorld;
        InGameQuickMenu::PostRenderThunk(&f.viewport,&f.canvas);
        CHECK(!f.menu.IsOpen());ExpectCloseReason(L"owner-world-changed");CHECK(!f.menu.HasInputLease());
    }
}
void TestPersistentVisibleSession(){
    Fixture f;f.open();fakeLogs.clear();
    // Two simulated minutes of frames with NO clicks/typing. No wall-clock sleep.
    for(int i=0;i<1200;++i){fakeNow+=100;InGameQuickMenu::PostRenderThunk(&f.viewport,&f.canvas);f.menu.TickInputSafety();}
    CHECK(f.menu.IsOpen());CHECK(f.menu.m_inputReady.load());CHECK(CloseLogCount()==0);
    CHECK(f.menu.m_completedFrames.load()==1201);CHECK(f.controller.uiCalls==1);CHECK(f.controller.gameCalls==0);
    f.menu.Toggle();ExpectCloseReason(L"F2-toggle");CHECK(Logged(L"after 120000 ms"));
    CHECK(Logged(L"completed frames=1201"));CHECK(Logged(L"last frame age=0 ms"));
    f.menu.TickInputSafety();CHECK(!f.menu.HasInputLease());CHECK(f.controller.gameCalls==1);
    f.menu.Close();f.menu.TickInputSafety();CHECK(CloseLogCount()==1);
    fakeLogs.clear();f.open();CHECK(f.menu.m_completedFrames.load()==1);fakeNow+=200;f.close();
    ExpectCloseReason(L"requested");CHECK(Logged(L"after 200 ms"));CHECK(Logged(L"last frame age=200 ms"));
}
void TestAutomaticCloseDiagnostics(){
    {Fixture f;f.open();fakeLogs.clear();fakeWindowLive=false;f.menu.TickInputSafety();
        CHECK(!f.menu.IsOpen());ExpectCloseReason(L"window-unavailable");}
    {Fixture f;f.open();fakeLogs.clear();fakeWindow=reinterpret_cast<HWND>(std::uintptr_t{2});
        InGameQuickMenu::PostRenderThunk(&f.viewport,&f.canvas);ExpectCloseReason(L"foreground-lost");}
    {Fixture f;f.open();fakeLogs.clear();f.controller.serial++;f.menu.TickInputSafety();
        CHECK(!f.menu.IsOpen());CHECK(!f.menu.HasInputLease());ExpectCloseReason(L"input-owner-expired");}
    {Fixture f;f.open();fakeLogs.clear();f.world.serial++;f.menu.TickInputSafety();
        CHECK(!f.menu.IsOpen());ExpectCloseReason(L"owner-world-changed");}
    {Fixture f;f.open();fakeLogs.clear();f.menu.ResetWorld();ExpectCloseReason(L"world-reset");}
    {Fixture f;f.open();fakeLogs.clear();f.menu.Shutdown();ExpectCloseReason(L"shutdown");}
    {Fixture f;fakeLogs.clear();f.menu.Toggle();fakeNow+=5000;f.menu.TickInputSafety();
        ExpectCloseReason(L"first-frame-timeout");CHECK(Logged(L"completed frames=0"));}
    {Fixture f;f.open();fakeLogs.clear();fakeNow+=5000;f.menu.TickInputSafety();ExpectCloseReason(L"frame-watchdog");
        CHECK(Logged(L"last frame age=5000 ms"));}
    {Fixture f;f.open();fakeLogs.clear();drawFails=true;InGameQuickMenu::PostRenderThunk(&f.viewport,&f.canvas);
        ExpectCloseReason(L"open-or-render-error");CHECK(Logged(L"Canvas drawing"));CHECK(!f.menu.HasInputLease());}
    {Fixture f;f.open();fakeLogs.clear();f.menu.m_events.resize(256);
        MSG c{fakeWindow,WM_CHAR,65,0};InGameQuickMenu::InputThunk(0,PM_REMOVE,reinterpret_cast<LPARAM>(&c));
        ExpectCloseReason(L"input-queue-overflow");CHECK(!f.menu.m_inputReady.load());
        f.menu.TickInputSafety();CHECK(!f.menu.HasInputLease());}
    {Fixture f;functions.erase(L"/Script/UMG.WidgetBlueprintLibrary:SetInputMode_UIOnlyEx");f.open();fakeLogs.clear();
        MSG escape{fakeWindow,WM_KEYDOWN,VK_ESCAPE,0};InGameQuickMenu::InputThunk(0,PM_REMOVE,reinterpret_cast<LPARAM>(&escape));
        ExpectCloseReason(L"Escape");CHECK(!f.menu.IsOpen());}
    // Normal-level summaries identify the requested mode, without falsely naming a calling mod.
    for(int kind=0;kind<3;++kind){
        Fixture f;f.menu.Initialize();f.open();fakeLogs.clear();
        UFunction* fn=kind==0?&f.gameFn:kind==1?&f.uiFn:&f.otherFn;
        alignas(16) std::array<std::uint8_t,256> args{};
        WriteMenuObjectParameter(fn,kind==0?&f.pcGame:&f.pcUI,args.data(),args.size(),&f.controller);
        f.library.ProcessEvent(fn,args.data());
        ExpectCloseReason(L"native-input-mode-change");
        CHECK(Logged(kind==0?L"SetInputMode_GameOnly":kind==1?L"SetInputMode_UIOnlyEx":L"SetInputMode_GameAndUIEx"));
        CHECK(!f.menu.m_nativeModeOwned);const auto gameCalls=f.controller.gameCalls;
        f.menu.TickInputSafety();CHECK(f.controller.gameCalls==gameCalls);CHECK(!f.menu.HasInputLease());CHECK(f.cursorOn());
    }
    {Fixture f;f.menu.Initialize();f.open();fakeLogs.clear();f.pcUI.flags|=CPF_UObjectWrapper;
        alignas(16) std::array<std::uint8_t,256> args{};
        f.menu.ObserveNativeInputMode(&f.uiFn,args.data());ExpectCloseReason(L"input-observer-error");}
    {Fixture f;f.open();fakeLogThrows=true;f.menu.Close();fakeLogThrows=false;
        CHECK(!f.menu.IsOpen());CHECK(!f.menu.m_inputReady.load());f.menu.TickInputSafety();CHECK(!f.menu.HasInputLease());}
}
int main(){try{TestNativeModes();TestContractFailures();TestBusyAndReadiness();TestCleanup();TestOwnershipAndMessages();TestUnavailableObjectVirtuals();TestScalarArgumentBounds();TestPreDispatchOwnership();TestFocusHandoffAndOtherViewports();TestPersistentVisibleSession();TestAutomaticCloseDiagnostics();std::cout<<"PASS: "<<checks<<" native-mode adapter/lifecycle assertions (mock host; not an in-game test).\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
