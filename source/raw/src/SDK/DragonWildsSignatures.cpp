#include "SDK/DragonWildsSignatures.h"
#include "Signatures.hpp"
#include "SigScanner/SinglePassSigScanner.hpp"
#include "Utility/Logging.h"
#include "Helpers/String.hpp"
#include "Runtime/Storefront.h"
#include "SDK/Helper/Memory.h"
#include "Unreal/UObject.hpp"
#include "ASMHelper/ASMHelper.hpp"
#include <Windows.h>
#include <utility>

using namespace RC;
using namespace RC::Unreal;

namespace DragonWilds {
    namespace {
        bool Requested(std::initializer_list<const char*> names, const std::string& value)
        {
            if (names.size() == 0) return true;
            for (const auto* name : names) if (name && value == name) return true;
            return false;
        }

        bool IsExecutable(void* address)
        {
            MEMORY_BASIC_INFORMATION memory{};
            if (!address || !VirtualQuery(address, &memory, sizeof(memory))
                || memory.State != MEM_COMMIT || (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
            return (memory.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ
                | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
        }
    }

    void SignatureManager::InitializeOnly(std::initializer_list<const char*> names)
    {
        const std::unordered_map<std::string, std::string>* directPatterns = nullptr;
        const std::unordered_map<std::string, std::string>* callPatterns = nullptr;
        switch (PS::Storefront::CurrentNativeLane()) {
        case PS::Storefront::NativeLane::SteamNative:
            directPatterns = &SteamSignatures;
            callPatterns = &SteamSignaturesCallResolve;
            break;
        case PS::Storefront::NativeLane::GamePassNative:
            directPatterns = &GamePassSignatures;
            callPatterns = &GamePassSignaturesCallResolve;
            break;
        default:
            break;
        }
        if (!directPatterns || !callPatterns) return;
        std::vector<SignatureContainer> SigContainerBox;
        SigContainerBox.reserve(directPatterns->size() + callPatterns->size());
        SinglePassScanner::SignatureContainerMap SigContainerMap;

        for (const auto& [ClassAndName, Signature] : *directPatterns)
        {
            if (!Requested(names, ClassAndName) || SignatureMap.contains(ClassAndName)) continue;
            SignatureContainer SigContainer = [=]() -> SignatureContainer {
                return {
                    {{Signature}},
                    [=](SignatureContainer& self) {
                        void* FunctionPointer = static_cast<void*>(self.get_match_address());

                        if (!IsExecutable(FunctionPointer)) return false;
                        SignatureMap.emplace(ClassAndName, FunctionPointer);
                        SourceMap.insert_or_assign(ClassAndName,
                            PS::Storefront::AllowsGamePassNativeSignatures()
                                ? "gamepass-embedded-aob" : "steam-embedded-aob");
                        PS::Log<LogLevel::Verbose>(STR("Found {}: {}\n"), RC::to_generic_string(ClassAndName), FunctionPointer);

                        self.get_did_succeed() = true;

                        return true;
                    },
                    [=](const SignatureContainer& self) {
                        if (!self.get_did_succeed())
                        {
                            PS::Log<RC::LogLevel::Warning>(STR("[BINDING:{}][UNAVAILABLE] {} executable pattern did not validate; the dependent feature will use a safe provider or remain disabled.\n"),
                                RC::to_generic_string(ClassAndName),
                                PS::ToWideSafe(PS::Storefront::Name(PS::Storefront::Current())));
                        }
                    }
                };
            }();
            SigContainerBox.emplace_back(std::move(SigContainer));
        }

        for (const auto& [ClassAndName, Signature] : *callPatterns)
        {
            if (!Requested(names, ClassAndName) || SignatureMap.contains(ClassAndName)) continue;
            SignatureContainer SigContainer = [=]() -> SignatureContainer {
                return {
                    {{Signature}},
                    [=](SignatureContainer& self) {
                        void* FunctionPointer = static_cast<void*>(self.get_match_address());
                        void* FinalAddress = ASM::resolve_call(FunctionPointer);

                        if (!IsExecutable(FinalAddress)) return false;
                        SignatureMap.emplace(ClassAndName, FinalAddress);
                        SourceMap.insert_or_assign(ClassAndName,
                            PS::Storefront::AllowsGamePassNativeSignatures()
                                ? "gamepass-embedded-call-aob" : "steam-embedded-call-aob");
                        PS::Log<LogLevel::Verbose>(STR("Found {}: {}\n"), RC::to_generic_string(ClassAndName), FinalAddress);

                        self.get_did_succeed() = true;

                        return true;
                    },
                    [=](const SignatureContainer& self) {
                        if (!self.get_did_succeed())
                        {
                            PS::Log<RC::LogLevel::Warning>(STR("[BINDING:{}][UNAVAILABLE] {} call pattern did not validate; the dependent feature will use a safe provider or remain disabled.\n"),
                                RC::to_generic_string(ClassAndName),
                                PS::ToWideSafe(PS::Storefront::Name(PS::Storefront::Current())));
                        }
                    }
                };
            }();
            SigContainerBox.emplace_back(std::move(SigContainer));
        }

        if (!SigContainerBox.empty()) {
            SigContainerMap.emplace(ScanTarget::MainExe, std::move(SigContainerBox));
            SinglePassScanner::start_scan(SigContainerMap);
        }
    }

    void SignatureManager::Initialize()
    {
        InitializeOnly({});
    }

    void* SignatureManager::GetSignature(const std::string& ClassAndFunction)
    {
        auto It = SignatureMap.find(ClassAndFunction);
        if (It != SignatureMap.end())
        {
            return It->second;
        }

        return nullptr;
    }

    void* SignatureManager::ResolveUObjectVirtual(const std::string& binding,
        const RC::StringType& classPath, std::initializer_list<const RC::CharType*> members)
    {
        if (auto* existing = GetSignature(binding)) return existing;
        try {
            const auto& layout = UObject::VTableLayoutMap;
            for (const auto* member : members) {
                if (!member) continue;
                const auto slot = layout.find(member);
                if (slot == layout.end() || slot->second % sizeof(void*) || slot->second > 8192) continue;
                auto** vtable = GetVTablePtrByClassPath(classPath);
                if (!vtable) return nullptr;
                auto* address = GetVirtualFunctionFromVTable(vtable, slot->second / sizeof(void*));
                if (!IsExecutable(address)) continue;
                SignatureMap.insert_or_assign(binding, address);
                SourceMap.insert_or_assign(binding, "live-vtable:" + RC::to_string(member));
                PS::Log<LogLevel::Normal>(STR("Native binding {} resolved through UE4SS vtable metadata ({}).\n"),
                    RC::to_generic_string(binding), member);
                return address;
            }
        } catch (const std::exception& error) {
            PS::Log<LogLevel::Warning>(STR("Native binding {} vtable provider failed: {}.\n"),
                RC::to_generic_string(binding), PS::ToWideSafe(error.what()));
        } catch (...) {
            PS::Log<LogLevel::Warning>(STR("Native binding {} vtable provider failed.\n"),
                RC::to_generic_string(binding));
        }
        return nullptr;
    }

    std::string SignatureManager::GetSource(const std::string& binding)
    {
        const auto found = SourceMap.find(binding);
        return found == SourceMap.end() ? std::string("unresolved") : found->second;
    }
}
