#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("source unavailable");
    return {std::istreambuf_iterator<char>(file), {}};
}

static void Need(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char** argv)
{
    Need(argc == 5, "recovery, signatures, loader and return decoder sources required");
    const auto recovery = Read(argv[1]);
    const auto signatures = Read(argv[2]);
    const auto loader = Read(argv[3]);
    const auto decoder = Read(argv[4]);

    Need(recovery.find("MidHook::create")!=recovery.npos&&recovery.find("StartDisabled")!=recovery.npos,
        "entry bypass must publish a disabled register-context hook before enabling it");
    Need(recovery.find("call<bool>")==recovery.npos&&recovery.find("InlineHook::create")==recovery.npos,
        "entry bypass must never invoke native loading through a guessed ABI");
    Need(recovery.find("PlayerStateHook=std::move(*playerCreated)")!=recovery.npos
            &&recovery.find("!created||!playerCreated")!=recovery.npos
            &&recovery.find("!AcceptanceHook.enable()||!PlayerStateHook.enable()")!=recovery.npos
            &&recovery.find("PlayerStateHook={};AcceptanceHook={}")!=recovery.npos,
        "both return hooks must be prepared together with rollback on failure");
    Need(recovery.find("context.rax=PS::CharacterEntryBypassPlan::AcceptRejected(context.rax)")!=recovery.npos
        &&recovery.find("if(context.rax&0xffu)return;")!=recovery.npos,
        "only rejected Boolean acceptance may be changed");
    Need(decoder.find("ZydisDecoderDecodeFull")!=decoder.npos&&decoder.find("FindReturn")!=decoder.npos
        &&recovery.find("RtlLookupFunctionEntry")!=recovery.npos
        &&recovery.find("function->BeginAddress!=target-address")!=recovery.npos
        &&recovery.find("NativeExecutableDiscovery::Resolve(pattern)")!=recovery.npos
        &&recovery.find("candidate!=result")!=recovery.npos,
        "decoded return flow, exact unwind function entries and unique native discovery are mandatory");
    Need(recovery.find("json") == recovery.npos
            && recovery.find("ConfigFiles") == recovery.npos
            && recovery.find("SaveCleanup") == recovery.npos,
        "entry recovery must never mutate or clean save data");
    Need(recovery.find("intro") == recovery.npos
            && recovery.find("video") == recovery.npos,
        "unrelated intro-skip behavior leaked into entry recovery");
    Need(recovery.find("CreateToolhelp32Snapshot") != recovery.npos
            && recovery.find("corruptcharacterbypass") != recovery.npos&&recovery.find("enabled.txt")!=recovery.npos,
        "standalone bypass conflict detection is missing");
    Need(signatures.find("CharacterSave::Validate") != signatures.npos
            && signatures.find("UPersistenceSubsystem::ProcessPlayerStateLoad")==signatures.npos,
        "the return-only bypass must not add a globally callable loader ABI binding");
    const auto gate = loader.find("NativePersistencePolicy::BypassRequested");
    Need(gate != loader.npos
            && loader.find("m_characterEntryRecovery.Initialize();",gate)!=loader.npos
            && loader.find("[SAVE-ENTRY][QUARANTINED]")==loader.npos,
        "paired bypass hooks must arm for client Normal/Quick Launch or the explicit server/default setting");
    Need(recovery.find("NativePersistencePolicy::BypassRequested")!=recovery.npos&&recovery.find("CharacterLoadAudit::PreparedForFallback()")==recovery.npos,
        "automatic rejection fallback must cover direct native calls without requiring a reflected preflight token");
    for(const auto* name:{"void CharacterEntryRecovery::AcceptCharacter(","void CharacterEntryRecovery::AcceptPlayerState("}){
        const auto start=recovery.find(name),allow=recovery.find("if(!MayBypass())return;",start),change=recovery.find("context.rax=",start);
        Need(start!=recovery.npos&&allow<change,"every rejection override must check fallback permission before changing its return");
        const auto body=recovery.substr(start,recovery.find("\n}",start)-start);
        Need(body.find("ConsolidatedLog<LogLevel::Normal>")!=body.npos&&body.find("[info]")!=body.npos&&body.find("Save Bypass initiated")!=body.npos,
            "actual bypass must emit a brief, always-visible informational summary");
        Need(body.find("DiagnosticLog::Formatted")!=body.npos&&body.find("DiagnosticLog::Write")!=body.npos&&body.find("LogLevel::Warning")==body.npos,
            "attributed and unattributed bypass details belong only in RuneSchema.log, without a yellow warning");
        Need(body.find("if(!first)return;")!=body.npos&&body.find("subsequent loads should")==body.npos,
            "preserve bypass throttling and never promise future loads will not need fallback");
    }
}
