#include "PakLoaderApi.h"
#include "Runtime/PakLoader.h"
#include "Utility/InlineHook.h"
#include "UE4SSProgram.hpp"
#include <Unreal/Core/Containers/FString.hpp>
#include <Unreal/FMemory.hpp>
#include <Unreal/UnrealInitializer.hpp>
#include <Unreal/UnrealVersion.hpp>
#include <DynamicOutput/DynamicOutput.hpp>
#include <SigScanner/SinglePassSigScanner.hpp>
#include <Zydis/Zydis.h>
#include <windows.h>
#include <psapi.h>

namespace PS::PakLoader {
namespace fs = std::filesystem;
using namespace RC;
using namespace RC::Unreal;

namespace {
// The process owns the startup hook for the lifetime of Unreal's PAK platform file.
SafetyHookInline* Hook{};

void AddFolders(const TCHAR* commandLine, TArray<FString>* folders)
{
    Hook->call(commandLine, folders);
    if (!folders || folders->Num() < 0 || folders->Num() > 4096) return;
    try {
        if (!GMalloc || !*GMalloc) return;
        std::vector<fs::path> existing;
        for (const auto& folder : *folders) existing.emplace_back(folder.GetCharArray().GetData());
        for (const auto& paks : Roots()) {
            if (AlreadyRegistered(paks, existing)) continue;
            folders->Add(FString((paks.generic_wstring() + L"/").c_str()));
        }
    } catch (const std::exception&) {
        Output::send<LogLevel::Warning>(STR("[PAK-LOADER] Could not prepare startup PAK directories.\n"));
    }
}

// Resolve the allocator from the RIP-relative load after FMemory::Free's initializer call.
FMalloc** Allocator(const uint8_t* instruction)
{
    if (!instruction) return nullptr;
    ZydisDecoder decoder{};
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
    bool afterCall = false;
    for (size_t offset = 0; offset < 40;) {
        ZydisDecodedInstruction decoded{};
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT]{};
        if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, instruction + offset, 40 - offset,
            &decoded, operands))) return nullptr;
        offset += decoded.length;
        if (afterCall) {
            if (decoded.mnemonic != ZYDIS_MNEMONIC_MOV || decoded.operand_count_visible != 2
                || operands[0].type != ZYDIS_OPERAND_TYPE_REGISTER || operands[1].type != ZYDIS_OPERAND_TYPE_MEMORY
                || operands[1].mem.base != ZYDIS_REGISTER_RIP) return nullptr;
            const auto address = instruction + offset + operands[1].mem.disp.value;
            MEMORY_BASIC_INFORMATION page{};
            if (!VirtualQuery(address, &page, sizeof(page)) || page.State != MEM_COMMIT
                || !(page.Protect & (PAGE_READWRITE | PAGE_WRITECOPY))
                || page.Protect & PAGE_GUARD) return nullptr;
            return reinterpret_cast<FMalloc**>(const_cast<uint8_t*>(address));
        }
        afterCall = decoded.mnemonic == ZYDIS_MNEMONIC_CALL;
    }
    return nullptr;
}
}

extern "C" __declspec(dllexport) bool RuneSchemaPakLoader_JoinV1(const wchar_t* paks) noexcept
{
    return Hook && Register(paks);
}

bool Enroll(const fs::path& paks)
{
    if (UnrealInitializer::StaticStorage::bIsInitialized) return false;
    using Program = UE4SSProgram;
    const auto* core = Program::find_mod_by_name<CppMod>(STR("RuneSchema"),
        Program::IsInstalled::Yes, Program::IsStarted::Yes);
    if (core) {
        auto dll = core->get_path() / "dlls" / "main.dll";
        if (!fs::exists(dll)) dll.replace_filename("RuneSchema.dll");
        const auto module = GetModuleHandleW(dll.c_str());
        const auto enroll = module ? reinterpret_cast<CoreRegister>(GetProcAddress(module, CoreRegisterExport)) : nullptr;
        if (enroll) return enroll(paks.c_str());
        Output::send<LogLevel::Warning>(STR("[PAK-LOADER] The active RuneSchema Core needs the companion mount API.\n"));
        return false;
    }

    // UE4SS calls native startup sequentially. Share the first copy's hook with later mods.
    HMODULE modules[1024]{};
    DWORD used{};
    if (!K32EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &used) || used > sizeof(modules))
        return false;
    for (const auto module : std::span(modules, used / sizeof(HMODULE))) {
        const auto join = reinterpret_cast<CoreRegister>(GetProcAddress(module, "RuneSchemaPakLoader_JoinV1"));
        if (join && join(paks.c_str())) return true;
    }

    constexpr auto pakPattern = "48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 4C 89 74 24 20 55 48 8B EC 48 83 EC 40 48 8D 4D F0 48 8B DA";
    constexpr auto freePattern = "48 85 C9 74 ?? 53 48 83 EC 20 48 8B D9 48 8B ?? ?? ?? ?? ?? 48 85 C9 75 0C E8 ?? ?? ?? ?? 48 8B";
    const auto collect = [](SignatureContainer&) { return false; };
    uint8_t* folders{};
    uint8_t* free{};
    const auto uniqueMatch = [](uint8_t*& address) {
        return [&address](SignatureContainer& result) {
            const auto& matches = result.get_result_store();
            if (matches.size() == 1) address = matches.front().match_address;
        };
    };
    SinglePassScanner::SignatureContainerMap scan;
    auto& patterns = scan[ScanTarget::MainExe];
    patterns.emplace_back(std::vector<SignatureData>{{pakPattern}}, collect, uniqueMatch(folders), true);
    patterns.emplace_back(std::vector<SignatureData>{{freePattern}}, collect, uniqueMatch(free), true);
    SinglePassScanner::start_scan(scan);
    if (!folders || !free) {
        Output::send<LogLevel::Warning>(STR("[PAK-LOADER] Startup bindings missing or ambiguous.\n"));
        return false;
    }
    auto** allocator = GMalloc ? GMalloc : Allocator(free);
    if (!allocator) {
        Output::send<LogLevel::Warning>(STR("[PAK-LOADER] Unreal allocator binding could not be validated.\n"));
        return false;
    }

    if (!Register(paks.c_str())) return false;
    HMODULE self{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&RuneSchemaPakLoader_JoinV1), &self)) return false;
    Version::Major = 5;
    Version::Minor = 6;
    UnrealInitializer::InitializeVersionedContainer();
    GMalloc = allocator;
    auto hook = std::make_unique<SafetyHookInline>();
    Hook = hook.get();
    if (!InstallInlineHook(*hook, folders, reinterpret_cast<void*>(AddFolders))) {
        Hook = nullptr;
        Output::send<LogLevel::Warning>(STR("[PAK-LOADER] Could not install startup PAK hook.\n"));
        return false;
    }
    (void)hook.release();
    return true;
}
}
