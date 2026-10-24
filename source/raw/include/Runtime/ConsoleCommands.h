#pragma once
#include "Core/ConsoleCommand.h"
#include "Runtime/PluginHost.h"
#include "Runtime/ConsoleMenu.h"
#include "Unreal/FOutputDevice.hpp"
#include "Unreal/Hooks.hpp"
#include "Utility/Logging.h"
#include "SDK/WeakObjectHandle.h"
#include <mutex>
#include <optional>
#include <nlohmann/json.hpp>

namespace PS::Console {
class Commands {
    RC::Unreal::Hook::GlobalCallbackId Callback=RC::Unreal::Hook::ERROR_ID;
    RC::Unreal::Hook::GlobalCallbackId MenuTick=RC::Unreal::Hook::ERROR_ID;
    struct MenuRequest {WeakObjectHandle Player;Command Target;MenuAction Action;};
    std::mutex PendingGate;std::optional<MenuRequest> PendingMenu;
public:
    ~Commands(){Shutdown();}
    void Shutdown(){
        if(MenuTick!=RC::Unreal::Hook::ERROR_ID){RC::Unreal::Hook::UnregisterCallback(MenuTick);MenuTick=RC::Unreal::Hook::ERROR_ID;}
        {std::lock_guard lock(PendingGate);PendingMenu.reset();}
        if(Callback!=RC::Unreal::Hook::ERROR_ID){
            RC::Unreal::Hook::UnregisterCallback(Callback);Callback=RC::Unreal::Hook::ERROR_ID;
        }
    }
    void Start(const PluginHost& host)noexcept{try{
        if(Callback!=RC::Unreal::Hook::ERROR_ID)return;
        RC::Unreal::Hook::FCallbackOptions options{};
        options.OwnerModName=TEXT("RuneSchema");options.HookName=TEXT("RuneSchemaConsole");
        MenuTick=RC::Unreal::Hook::RegisterEngineTickPostCallback([this](auto&,auto*,float,bool){
            std::optional<MenuRequest> pending;{std::lock_guard lock(PendingGate);pending=std::move(PendingMenu);PendingMenu.reset();}
            if(!pending)return;
            try{
                auto* player=pending->Player.Get();if(!player||!player->IsA<RC::Unreal::ULocalPlayer>())throw std::runtime_error("Menu player is no longer available");
                const auto result=RequestMenu(static_cast<RC::Unreal::ULocalPlayer*>(player),pending->Target,pending->Action);
                Log<RC::LogLevel::Normal>(TEXT("[CONSOLE][MENU] {}\n"),ToWideSafe(result.dump()));
            }catch(const std::exception& error){Log<RC::LogLevel::Warning>(TEXT("[CONSOLE][MENU][UNAVAILABLE] {}\n"),ToWideSafe(error.what()));}
            catch(...){Log<RC::LogLevel::Warning>(TEXT("[CONSOLE][MENU][UNAVAILABLE] Native menu request failed; no retry.\n"));}
        },options);
        Callback=RC::Unreal::Hook::RegisterULocalPlayerExecPreCallback(
            [this,&host](auto& iteration,RC::Unreal::ULocalPlayer* player,RC::Unreal::UWorld*,const TCHAR* input,RC::Unreal::FOutputDevice& output){
                if(!input||iteration.OriginalFunctionCallPrevented()||iteration.IsReturnValueOverrode())return;
                // Bound inspection; commands outside our exact namespace pass through untouched.
                const auto length=wcsnlen_s(input,512);
                const auto command=Parse(std::wstring_view(input,length));
                if(command==Command::NotOurs)return;
                if(!iteration.TrySetReturnValue(true))return;
                iteration.PreventOriginalFunctionCall();
                RC::StringType reply;
                try{
                    if(command==Command::Help)reply=Help;
                    else if(command==Command::Invalid)reply=TEXT("Invalid RuneSchema command. Use: rs help");
                    else{
                        nlohmann::json result;
                        if(MenuWidgetName(command)){
                            if(!RC::Unreal::IsInGameThread()||MenuTick==RC::Unreal::Hook::ERROR_ID)throw std::runtime_error("Native menu dispatch is unavailable");
                            {std::lock_guard lock(PendingGate);if(PendingMenu)throw std::runtime_error("A menu request is already queued");}
                            const auto pending=WeakObject(player);if(pending.Get()!=player)throw std::runtime_error("Cannot retain the menu player safely");
                            result=CloseConsole(player);
                            {std::lock_guard lock(PendingGate);PendingMenu=MenuRequest{pending,command,RequestedMenuAction(std::wstring_view(input,length))};}
                            result["MenuQueued"]=true;result["Status"]="Console closure requested; native menu action queued after command return.";
                        }
                        else if(command==Command::CloseConsole)result=CloseConsole(player);
                        else if(command==Command::Status||command==Command::Network)
                            result=nlohmann::json::parse(host.Call("RuneSchema.Core","runeschema.tools",
                                command==Command::Status?R"({"Action":"Environment"})":R"({"Action":"NetworkStatus"})"));
                        else{
                            result=nlohmann::json::parse(host.Call("RuneSchema.Core","runeschema.discovery","{}"));
                            if(command==Command::Mappings)result=result.at("MappingBackbone");
                            else result.erase("MappingBackbone");
                        }
                        const auto text=result.dump(2);
                        reply=text.size()<=16384?ToWideSafe(text):TEXT("Diagnostic response exceeds console limit; use Helpy for full details.");
                    }
                }catch(const std::exception& error){reply=ToWideSafe(std::string("RuneSchema command unavailable: ")+error.what());}
                catch(...){reply=TEXT("RuneSchema command unavailable. See RuneSchema.log.");}
                output.Log(reply.c_str());
                Log<RC::LogLevel::Normal>(TEXT("[CONSOLE] {}\n"),reply);
            },options);
        if(Callback==RC::Unreal::Hook::ERROR_ID){
            Shutdown();
            Log<RC::LogLevel::Warning>(TEXT("[CONSOLE][UNAVAILABLE] UE4SS local-player console hook unavailable. Startup continues without RuneSchema commands.\n"));
        }
        else Log<RC::LogLevel::Normal>(TEXT("[CONSOLE][READY] Commands registered: rs help (alias: runeschema). Native menus support open/close/toggle through the game's guarded input router. No Exit action is selected.\n"));
    }catch(...){Shutdown();Log<RC::LogLevel::Warning>(TEXT("[CONSOLE][UNAVAILABLE] Console registration failed; RuneSchema startup continues.\n"));}}
};
}
