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
    const auto playerAccept = recovery.find("return true;", playerCall);
    Need(validationCall != recovery.npos && validationAccept != recovery.npos
            && validationCall < validationAccept,
        "character validation is not allowed only after native validation runs");
    Need(playerCall != recovery.npos && playerAccept != recovery.npos
            && playerCall < playerAccept,
        "player-state loading is not allowed only after the native loader runs");
    Need(recovery.find("json") == recovery.npos
            && recovery.find("ConfigFiles") == recovery.npos
            && recovery.find("SaveCleanup") == recovery.npos,
        "entry recovery must never mutate or clean save data");
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
    Need(gate != loader.npos
            && loader.find("[SAVE-ENTRY][QUARANTINED]", gate) != loader.npos
            && loader.find("m_characterEntryRecovery.Initialize();") == loader.npos,
        "unsafe native entry hooks must remain quarantined");
}
