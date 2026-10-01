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
    Need(argc == 4, "recovery, signatures, and loader sources required");
    const auto recovery = Read(argv[1]);
    const auto signatures = Read(argv[2]);
    const auto loader = Read(argv[3]);

    const auto validationCall = recovery.find("ValidationHook.call<bool>");
    const auto validationAccept = recovery.find("return true;", validationCall);
    const auto playerCall = recovery.find("PlayerStateHook.call<bool>");
    const auto playerResult = recovery.find("return accepted;", playerCall);
    Need(validationCall != recovery.npos && validationAccept != recovery.npos
            && validationCall < validationAccept,
        "character validation is not allowed only after native validation runs");
    Need(playerCall != recovery.npos && playerResult != recovery.npos
            && playerCall < playerResult,
        "mandatory pruning boundary must preserve the native loader result");
    Need(recovery.find("std::int32_t result") != recovery.npos
            && recovery.find("m_pruner.PruneCharacterJson(*playerState)")
                != recovery.npos,
        "player-state load ABI or native JSON preflight regressed");
    Need(recovery.find("intro") == recovery.npos
            && recovery.find("video") == recovery.npos,
        "unrelated intro-skip behavior leaked into entry recovery");
    Need(recovery.find("CreateToolhelp32Snapshot") != recovery.npos
            && recovery.find("corruptcharacterbypass") != recovery.npos,
        "standalone bypass conflict detection is missing");
    Need(signatures.find("CharacterSave::Validate") != signatures.npos
            && signatures.find("UPersistenceSubsystem::ProcessPlayerStateLoad")
                != signatures.npos,
        "entry recovery signatures are not embedded for native lanes");
    const auto gate = loader.find(
        "GetSettings().persistence.allowCorruptCharacterEntry");
    const auto earlyBoundary = loader.find(
        "m_characterEntryRecovery.Initialize();");
    Need(earlyBoundary != loader.npos && earlyBoundary < gate
            && loader.find("[SAVE-ENTRY][QUARANTINED]", gate) != loader.npos,
        "native pruning must install early while acceptance override remains quarantined");
}
