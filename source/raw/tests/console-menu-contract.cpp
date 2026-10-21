#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
static std::string Read(const std::filesystem::path& path){
    std::ifstream stream(path);assert(stream);return {std::istreambuf_iterator<char>(stream),{}};
}
int main(int argc,char** argv){
    assert(argc==2);const std::filesystem::path root(argv[1]);
    const auto menu=Read(root/"include/Runtime/ConsoleMenu.h");
    for(const auto* guard:{"IsInGameThread()","IsDedicatedServer()","IsLocalController","GetOwningPlayer",
        "C::Fits","FUNC_Native","FUNC_Static","FUNC_Net","C::Count(function)!=(toggle?4:5)",
        "/Script/Dominion.EWidgetType","GetEnumNames()","MenuWidgetName(command)","C::Input(widget,function)",
        "C::Input(playerField,function)","C::Input(outside,function)","C::Input(pause,function)","C::Output(result,function)",
        "OpenWidgetIfClosed","CloseWidgetIfOpen","ToggleWidget","bShouldCheckPauseBlock\"),true"})assert(menu.contains(guard));
    for(const auto* dangerous:{":QuitGame",":ConsoleCommand",":ExecuteConsoleCommand",":ClientTravel",
        ":OpenLevel",":GoToMainMenu",":ExitToMainMenu","WriteProcessMemory","SetGamePaused"})assert(!menu.contains(dangerous));
    assert(menu.find("IsInGameThread()")<menu.find("const auto alive="));
    assert(menu.contains("/Script/UMG.Widget:GetOwningPlayer")&&!menu.contains("/Script/UMG.UserWidget:GetOwningPlayer"));
    assert(menu.find("C::Count(function)")<menu.find("AH::FunctionCall request("));
    assert(menu.find("ordinal<=0||ordinal>255")<menu.find("AH::FunctionCall request("));
    assert(!menu.contains("SetVisibility")&&!menu.contains("RemoveFromParent")&&!menu.contains("ClearHUDWidget"));
    assert(menu.contains("/Script/Dominion.HUDUISubsystem")&&menu.contains("Multiple HUD subsystems match")&&menu.contains("subsystems.Num()>64"));
    assert(menu.contains("object(subsystem,TEXT(\"InputManagerAPI\"))")&&!menu.contains("Default__InputManagerUIAPI"));
    for(const auto* guard:{"ConsoleKey","FUNC_Exec","ViewportConsole","ConsoleKeys","console->GetOuterPrivate()!=viewport","cycle<2","configured.empty()","\"KeyName\",configured"})assert(menu.contains(guard));
    const auto commands=Read(root/"include/Runtime/ConsoleCommands.h");
    assert(commands.contains("RegisterULocalPlayerExecPreCallback")&&commands.contains("UnregisterCallback(Callback)"));
    assert(commands.find("command==Command::NotOurs")<commands.find("iteration.PreventOriginalFunctionCall()"));
    assert(!commands.contains("helpy.vision.status")&&!commands.contains("SchemaVisionRuntime"));
    assert(commands.contains("MenuWidgetName(command)")&&commands.contains("RequestedMenuAction(std::wstring_view(input,length))"));
    for(const auto* guard:{"MenuTick","WeakObjectHandle Player","PendingMenu.reset()","pending->Player.Get()","RegisterEngineTickPostCallback","MenuQueued","A menu request is already queued"})assert(commands.contains(guard));
    assert(commands.find("result=CloseConsole(player)")<commands.find("PendingMenu=MenuRequest"));
    assert(commands.contains("command==Command::CloseConsole"));
    const auto main=Read(root/"src/dllmain.cpp");
    const auto destructor=main.find("~RuneSchema() override");
    assert(main.find("m_consoleCommands.Shutdown()",destructor)<main.find("m_pluginHost.Shutdown()",destructor));
}
