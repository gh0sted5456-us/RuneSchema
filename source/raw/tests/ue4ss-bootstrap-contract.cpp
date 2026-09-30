#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("UE4SS bootstrap contract input is unavailable");
    return {std::istreambuf_iterator<char>(file), {}};
}
static void Require(const std::string& source, const char* text) {
    if (source.find(text) == std::string::npos)
        throw std::runtime_error(std::string("UE4SS bootstrap contract is missing: ") + text);
}
int main(int argc, char** argv) {
    if (argc != 3) throw std::runtime_error("build.ps1 and raw CMakeLists.txt are required");
    const auto build = Read(argv[1]);
    const auto cmake = Read(argv[2]);

    Require(cmake, "RUNESCHEMA_UE4SS_TAG");
    Require(build, "Get-UE4SSPinnedCommit");
    Require(build, "Ensure-UE4SSSource");
    Require(build, "Assert-UEPseudoAccess");
    Require(build, "FETCHCONTENT_SOURCE_DIR_UE4SS");
    Require(build, "UE4SS_PROJECTS=UE4SS");
    Require(build, "ENABLE_IDE_SOURCE_VISIBILITY=OFF");
    Require(build, "core UE4SS only (UVTD and IDE source indexing disabled)");
    Require(build, "Discarding incomplete CMake configure state");
    Require(build, "https://github.com/Re-UE4SS/UEPseudo.git");
    Require(build, "https://github.com/settings/organizations");
    Require(build, "GITHUB_ACTIONS");
    Require(build, "Git Credential Manager");
    Require(build, "submodule', 'update', '--init', '--recursive");
    Require(build, "Get-ConfigureFingerprint $SourceDirectory $ConfigureArguments");

    std::cout << "UE4SS bootstrap contract passed.\n";
}
