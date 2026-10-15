#include "Core/CharacterEntryRecovery.h"
#include "Core/CharacterEntryBypassPlan.h"
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
std::atomic_bool Reported=false;
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
// Decode the complete caller, rejecting byte-pattern hits inside immediates.
// The native diagnostic marker distinguishes character loading from other
// native uses of this shared validator. No native function is called here.
bool CertifiedCaller(const std::uint8_t* image,std::uint32_t imageSize,const RUNTIME_FUNCTION& function,const PS::CharacterEntryBypassPlan::Gate& gate) {
    if(function.BeginAddress>=function.EndAddress||function.EndAddress>imageSize||function.EndAddress-function.BeginAddress>16384)return false;
    auto* begin=image+function.BeginAddress;const auto size=function.EndAddress-function.BeginAddress;
    if(!PS::NativeExecutableDiscovery::Readable(begin,size))return false;
    ZydisDecoder decoder{};ZydisDecoderInit(&decoder,ZYDIS_MACHINE_MODE_LONG_64,ZYDIS_STACK_WIDTH_64);
    ZydisDecodedInstruction instruction{};ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT]{};
    bool marker=false,call=false,test=false,success=false;std::size_t offset=0;
    while(offset<size) {
        if(!ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder,begin+offset,size-offset,&instruction,operands))||!instruction.length)return false;
        const auto rva=function.BeginAddress+static_cast<std::uint32_t>(offset);
        call|=rva==gate.Call&&instruction.mnemonic==ZYDIS_MNEMONIC_CALL;
        test|=rva==gate.Test&&instruction.mnemonic==ZYDIS_MNEMONIC_TEST;
        success|=rva==gate.Success;
        if(instruction.mnemonic==ZYDIS_MNEMONIC_LEA)for(unsigned i=0;i<instruction.operand_count;++i) {
            const auto& operand=operands[i];if(operand.type!=ZYDIS_OPERAND_TYPE_MEMORY||operand.mem.base!=ZYDIS_REGISTER_RIP)continue;
            const auto target=static_cast<std::int64_t>(rva)+instruction.length+operand.mem.disp.value;
            constexpr wchar_t name[]=L"LoadCharacterInfoFromJson";
            constexpr char narrowName[]="LoadCharacterInfoFromJson";
            if(target>=0&&target<=static_cast<std::int64_t>(imageSize)-static_cast<std::int64_t>(sizeof(name))&&PS::NativeExecutableDiscovery::Readable(image+target,sizeof(name))&&std::memcmp(image+target,name,sizeof(name))==0)marker=true;
            if(target>=0&&target<=static_cast<std::int64_t>(imageSize)-static_cast<std::int64_t>(sizeof(narrowName))&&PS::NativeExecutableDiscovery::Readable(image+target,sizeof(narrowName))&&std::memcmp(image+target,narrowName,sizeof(narrowName))==0)marker=true;
        }
        offset+=instruction.length;
    }
    return marker&&call&&test&&success;
}
void* FindCharacterAcceptanceGate(void* validation) {
    const auto* image=reinterpret_cast<const std::uint8_t*>(GetModuleHandleW(nullptr));
    if(!image||!PS::NativeExecutableDiscovery::Readable(image,sizeof(IMAGE_DOS_HEADER)))return nullptr;
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<=0||dos->e_lfanew>4096)return nullptr;
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(image+dos->e_lfanew);
    if(!PS::NativeExecutableDiscovery::Readable(nt,sizeof(*nt))||nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||nt->FileHeader.NumberOfSections>96)return nullptr;
    const auto imageSize=nt->OptionalHeader.SizeOfImage;
    const auto address=reinterpret_cast<std::uintptr_t>(image);
    const auto validated=reinterpret_cast<std::uintptr_t>(validation);
    if(!imageSize||imageSize>512*1024*1024||validated<address||validated-address>=imageSize)return nullptr;
    const auto target=static_cast<std::uint32_t>(validated-address);const auto* sections=IMAGE_FIRST_SECTION(nt);
    if(!PS::NativeExecutableDiscovery::Readable(sections,nt->FileHeader.NumberOfSections*sizeof(*sections)))return nullptr;
    void* result=nullptr;std::uint32_t previous=UINT32_MAX;
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        const auto& section=sections[i];const auto size=section.Misc.VirtualSize;
        if(!(section.Characteristics&IMAGE_SCN_MEM_EXECUTE)||(section.Characteristics&IMAGE_SCN_MEM_WRITE))continue;
        if(section.VirtualAddress>=imageSize||size>imageSize-section.VirtualAddress||!PS::NativeExecutableDiscovery::Readable(image+section.VirtualAddress,size))return nullptr;
        const auto* code=image+section.VirtualAddress;
        for(std::size_t offset=0;offset+5<size;++offset) {
            if(code[offset]!=0xe8)continue;std::int32_t relative;std::memcpy(&relative,code+offset+1,4);
            const auto call=section.VirtualAddress+static_cast<std::uint32_t>(offset);
            if(static_cast<std::int64_t>(call)+5+relative!=target)continue;
            DWORD64 owner=0;const auto* function=RtlLookupFunctionEntry(reinterpret_cast<DWORD64>(image+call),&owner,nullptr);
            if(!function||owner!=reinterpret_cast<DWORD64>(image)||function->BeginAddress==previous)continue;
            previous=function->BeginAddress;
            if(function->BeginAddress>=function->EndAddress||function->EndAddress>imageSize||function->EndAddress-function->BeginAddress>16384||!PS::NativeExecutableDiscovery::Readable(image+function->BeginAddress,function->EndAddress-function->BeginAddress))continue;
            const auto gate=PS::CharacterEntryBypassPlan::Find({image+function->BeginAddress,function->EndAddress-function->BeginAddress},function->BeginAddress,target);
            if(!gate||!CertifiedCaller(image,imageSize,*function,*gate))continue;
            if(result)return nullptr;result=const_cast<std::uint8_t*>(image+gate->Test);
        }
    }
    return result;
}
}
namespace DragonWilds {
void CharacterEntryRecovery::Initialize() {
    if(IsActive())return;
    if(StandaloneBypassEnabledOrLoaded()) {
        PS::Log<LogLevel::Warning>(STR("[SAVE-ENTRY][CONFLICT] Standalone CorruptCharacterBypass is enabled or loaded. Disable it before using RuneSchema's optional bypass; no duplicate hook was installed.\n"));return;
    }
    auto* validation=SignatureManager::GetSignature("CharacterSave::Validate");
    auto* gate=validation?FindCharacterAcceptanceGate(validation):nullptr;
    if(!gate){PS::Log<LogLevel::Warning>(STR("[SAVE-ENTRY][UNAVAILABLE] The native character acceptance call site could not be uniquely verified. Native loading and all other RuneSchema features remain unchanged.\n"));return;}
    auto created=safetyhook::MidHook::create(gate,&AcceptCharacter,safetyhook::MidHook::StartDisabled);
    if(!created){PS::Log<LogLevel::Warning>(STR("[SAVE-ENTRY][UNAVAILABLE] The optional character acceptance hook could not be prepared. Native acceptance remains unchanged.\n"));return;}
    AcceptanceHook=std::move(*created);
    if(!AcceptanceHook.enable()){AcceptanceHook={};PS::Log<LogLevel::Warning>(STR("[SAVE-ENTRY][UNAVAILABLE] Optional bypass activation failed; native acceptance remains unchanged.\n"));return;}
    PS::Log<LogLevel::Normal>(STR("[SAVE-ENTRY][READY] Optional character validation bypass active. Native validation still runs; this does not repair damaged save data.\n"));
}
void CharacterEntryRecovery::Shutdown(){AcceptanceHook={};Reported=false;}
bool CharacterEntryRecovery::IsActive() const noexcept{return static_cast<bool>(AcceptanceHook);}
void CharacterEntryRecovery::AcceptCharacter(SafetyHookContext& context) {
    if(context.rax&0xffu)return;
    context.rax=PS::CharacterEntryBypassPlan::AcceptRejected(context.rax);
    if(!Reported.exchange(true))PS::Log<LogLevel::Warning>(STR("[SAVE-ENTRY][BYPASSED] Native character validation rejected this load; the opt-in bypass allowed it to continue. This is not a save-data repair; retain your original backup.\n"));
}
}
