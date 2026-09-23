#pragma once
#include "Unreal/CoreUObject/UObject/Class.hpp"
#include "Unreal/UnrealFlags.hpp"
#include "Utility/Logging.h"

namespace PS {
inline bool CanHookNative(RC::Unreal::UFunction* function) {
    if (!function) return false;
    if (function->GetFunctionFlags() & RC::Unreal::FUNC_Native) return true;
    Log<RC::LogLevel::Warning>(STR("Skipped non-native function-pointer hook: {}\n"),
        function->GetPathName());
    return false;
}
inline auto RegisterNativePreHook(RC::Unreal::UFunction* function,
    const RC::Unreal::UnrealScriptFunctionCallable& callback, void* data = nullptr) {
    return CanHookNative(function) ? function->RegisterPreHook(callback, data) : 0;
}
inline auto RegisterNativePostHook(RC::Unreal::UFunction* function,
    const RC::Unreal::UnrealScriptFunctionCallable& callback, void* data = nullptr) {
    return CanHookNative(function) ? function->RegisterPostHook(callback, data) : 0;
}
}
