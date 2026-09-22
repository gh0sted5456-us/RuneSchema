#pragma once
#include <functional>
#include <map>
#include <string>
namespace RC::Unreal {class UObject;class UFunction;class UEngine;class AGameModeBase;namespace Hook {
using GlobalCallbackId=int;inline constexpr int ERROR_ID=-1;
template<class>struct TCallbackIterationData {};
struct FCallbackOptions {const wchar_t* OwnerModName=nullptr;const wchar_t* HookName=nullptr;};
inline bool failTick=false,failWorld=false,failEvents=false;
inline int sequence=0;
inline std::map<int,std::function<void(TCallbackIterationData<void>&,UEngine*,float,bool)>> tickCallbacks;
inline std::map<int,std::function<void(TCallbackIterationData<void>&,AGameModeBase*)>> worldCallbacks;
template<class F>int RegisterEngineTickPostCallback(F fn,const FCallbackOptions&){
    if(failTick)return ERROR_ID;
    tickCallbacks.emplace(++sequence,fn);return sequence;
}
template<class F>int RegisterInitGameStatePreCallback(F fn,const FCallbackOptions&){
    if(failWorld)return ERROR_ID;
    worldCallbacks.emplace(++sequence,fn);return sequence;
}
inline std::map<int,std::function<void(TCallbackIterationData<void>&,UObject*,UFunction*,void*)>> eventCallbacks;
template<class F>int RegisterProcessEventPreCallback(F fn,const FCallbackOptions&){
    if(failEvents)return ERROR_ID;
    eventCallbacks.emplace(++sequence,fn);return sequence;
}
inline void UnregisterCallback(int id){tickCallbacks.erase(id);worldCallbacks.erase(id);eventCallbacks.erase(id);}
}}
