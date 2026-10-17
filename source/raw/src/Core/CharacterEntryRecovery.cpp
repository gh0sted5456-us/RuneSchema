#include "Core/CharacterEntryRecovery.h"
#include "Core/CharacterEntryReturnGate.h"
#include "SDK/DragonWildsSignatures.h"
#include "Runtime/NativeExecutableDiscovery.h"
#include "Runtime/HostServices.h"
#include "Utility/Logging.h"
#include "Zydis/Zydis.h"
#include <Windows.h>
#include <TlHelp32.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cwctype>
#include <fstream>
#include <string>

using namespace RC;
namespace {
std::atomic_bool Reported=false,PlayerStateReported=false;
bool StandaloneBypassEnabledOrLoaded() {
    // enabled.txt starts after mods.txt, regardless of a mods.txt :0 entry.
    const auto mods=PS::HostServices::WorkingDirectory()/"Mods";
    std::error_code error;
    if(std::filesystem::exists(mods/"CorruptCharacterBypass"/"enabled.txt",error)||error)return true;
    std::ifstream list(mods/"mods.txt");std::string line;std::size_t bytes=0;
    while(std::getline(list,line)) {
        if((bytes+=line.size())>1024*1024)return true;
        if(const auto comment=line.find(';');comment!=line.npos)line.resize(comment);
        std::erase_if(line,[](unsigned char c){return std::isspace(c);});
        std::transform(line.begin(),line.end(),line.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        if(line.starts_with("corruptcharacterbypass:1"))return true;
    }
    const auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,GetCurrentProcessId());
    if(snapshot==INVALID_HANDLE_VALUE)return true;
    MODULEENTRY32W module{};module.dwSize=sizeof(module);bool conflict=false;
    if(!Module32FirstW(snapshot,&module))conflict=true;
    else do {
        std::wstring path{module.szExePath};std::transform(path.begin(),path.end(),path.begin(),[](wchar_t c){return std::towlower(c);});
        if(path.find(L"\\corruptcharacterbypass\\")!=path.npos){conflict=true;break;}
    }while(Module32NextW(snapshot,&module));
    CloseHandle(snapshot);return conflict;
}
// No native loader is called through a guessed ABI. Hook its decoded return
// tail after native work has completed, covering every caller like the original.
void* FindNativeReturnGate(void* native) {
    const auto* image=reinterpret_cast<const std::uint8_t*>(GetModuleHandleW(nullptr));
    if(!image||!PS::NativeExecutableDiscovery::Readable(image,sizeof(IMAGE_DOS_HEADER)))return nullptr;
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<=0||dos->e_lfanew>4096)return nullptr;
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(image+dos->e_lfanew);
    if(!PS::NativeExecutableDiscovery::Readable(nt,sizeof(*nt))||nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||nt->FileHeader.NumberOfSections>96)return nullptr;
    const auto imageSize=nt->OptionalHeader.SizeOfImage;
    const auto address=reinterpret_cast<std::uintptr_t>(image);
    const auto target=reinterpret_cast<std::uintptr_t>(native);
    if(!imageSize||imageSize>512*1024*1024||target<address||target-address>=imageSize)return nullptr;
    DWORD64 owner=0;const auto* function=RtlLookupFunctionEntry(target,&owner,nullptr);
    if(!function||owner!=address||function->BeginAddress!=target-address||function->BeginAddress>=function->EndAddress
        ||function->EndAddress>imageSize||function->EndAddress-function->BeginAddress>65536)return nullptr;
    const auto* sections=IMAGE_FIRST_SECTION(nt);
    if(!PS::NativeExecutableDiscovery::Readable(sections,nt->FileHeader.NumberOfSections*sizeof(*sections)))return nullptr;
    bool executable=false;
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        const auto& section=sections[i];const auto size=section.Misc.VirtualSize;
        if(!(section.Characteristics&IMAGE_SCN_MEM_EXECUTE)||(section.Characteristics&IMAGE_SCN_MEM_WRITE))continue;
        if(section.VirtualAddress>=imageSize||size>imageSize-section.VirtualAddress)return nullptr;
        if(function->BeginAddress>=section.VirtualAddress&&function->EndAddress<=section.VirtualAddress+size)executable=true;
    }
    const auto length=function->EndAddress-function->BeginAddress;
    if(!executable||!PS::NativeExecutableDiscovery::Readable(native,length))return nullptr;
    const auto gate=PS::CharacterEntryBypassPlan::DecodeReturn({static_cast<const std::uint8_t*>(native),length});
    return gate?static_cast<std::uint8_t*>(native)+*gate:nullptr;
}
void* FindPlayerStateLoad() {
    void* result=nullptr;
    for(const auto pattern:PS::CharacterEntryBypassPlan::PlayerStatePatterns) {
        auto* candidate=PS::NativeExecutableDiscovery::Resolve(pattern);
        if(candidate){if(result&&candidate!=result)return nullptr;result=candidate;}
    }
    return result;
}
}
namespace DragonWilds {
void CharacterEntryRecovery::Initialize() {
    if(IsActive())return;
    if(StandaloneBypassEnabledOrLoaded()) {
        State="Blocked: standalone CorruptCharacterBypass is enabled or loaded";
        PS::Log<LogLevel::Warning>(STR("[SAVE-ENTRY][CONFLICT] Standalone CorruptCharacterBypass is enabled or loaded. Disable it before using RuneSchema's optional bypass; no duplicate hook was installed.\n"));return;
    }
    auto* validation=SignatureManager::GetSignature("CharacterSave::Validate");
    if(!validation){State="Unavailable: native validator binding was not found";PS::Log<LogLevel::Warning>(STR("[SAVE-ENTRY][UNAVAILABLE][VALIDATOR] The native validator binding was not found. No acceptance hook was installed.\n"));return;}
    auto* gate=FindNativeReturnGate(validation);
    auto* player=FindPlayerStateLoad();
    auto* playerGate=player?FindNativeReturnGate(player):nullptr;
    if(!gate||!playerGate||gate==playerGate){State="Unavailable: both native return gates must be verified";PS::Log<LogLevel::Warning>(STR("[SAVE-ENTRY][UNAVAILABLE][RETURN-GATES] Character validation and ProcessPlayerStateLoad must both have uniquely verified native return gates. No bypass hook was installed.\n"));return;}
    auto created=safetyhook::MidHook::create(gate,&AcceptCharacter,safetyhook::MidHook::StartDisabled);
    auto playerCreated=safetyhook::MidHook::create(playerGate,&AcceptPlayerState,safetyhook::MidHook::StartDisabled);
    if(!created||!playerCreated){State="Unavailable: paired hook preparation failed";PS::Log<LogLevel::Warning>(STR("[SAVE-ENTRY][UNAVAILABLE] The paired native return hooks could not be prepared. Native acceptance remains unchanged.\n"));return;}
    AcceptanceHook=std::move(*created);
    PlayerStateHook=std::move(*playerCreated);
    if(!AcceptanceHook.enable()||!PlayerStateHook.enable()){
        PlayerStateHook={};AcceptanceHook={};State="Unavailable: paired hook activation failed";
        PS::Log<LogLevel::Warning>(STR("[SAVE-ENTRY][UNAVAILABLE] Paired bypass activation failed; both native return hooks were removed.\n"));return;
    }
    State="Ready: native validation and player-state return bypass active";
    PS::Log<LogLevel::Normal>(STR("[SAVE-ENTRY][READY] Optional character validation and ProcessPlayerStateLoad bypass active. Both native functions still run; rejected return results are accepted without save repair.\n"));
}
void CharacterEntryRecovery::Shutdown(){PlayerStateHook={};AcceptanceHook={};Reported=false;PlayerStateReported=false;State="Disabled for this session";}
bool CharacterEntryRecovery::IsActive() const noexcept{return static_cast<bool>(AcceptanceHook)&&static_cast<bool>(PlayerStateHook);}
void CharacterEntryRecovery::AcceptCharacter(SafetyHookContext& context) {
    if(context.rax&0xffu)return;
    context.rax=PS::CharacterEntryBypassPlan::AcceptRejected(context.rax);
    if(!Reported.exchange(true))PS::Log<LogLevel::Warning>(STR("[SAVE-ENTRY][BYPASSED] Native character validation rejected this load; the opt-in bypass allowed it to continue. This is not a save-data repair; retain your original backup.\n"));
}
void CharacterEntryRecovery::AcceptPlayerState(SafetyHookContext& context) {
    if(context.rax&0xffu)return;
    context.rax=PS::CharacterEntryBypassPlan::AcceptRejected(context.rax);
    if(!PlayerStateReported.exchange(true))PS::Log<LogLevel::Warning>(STR("[SAVE-ENTRY][BYPASSED][PLAYER-STATE] Native ProcessPlayerStateLoad returned rejection; the opt-in bypass accepted its return result. Native loading was not skipped.\n"));
}
}
