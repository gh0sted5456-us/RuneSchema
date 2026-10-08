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
static void Forbid(const std::string& source, const char* text) {
    if (source.find(text) != std::string::npos)
        throw std::runtime_error(std::string("UE4SS bootstrap regressed from known-good e7e8a32 behavior: ") + text);
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
    Require(build, "https://github.com/Re-UE4SS/UEPseudo.git");
    Require(build, "https://github.com/settings/organizations");
    Require(build, "GITHUB_ACTIONS");
    Require(build, "Git Credential Manager");
    Require(build, "submodule', 'update', '--init', '--recursive");
    Require(build, "Get-ConfigureFingerprint $SourceDirectory $ConfigureArguments");

    // The only intentional builder deviation from the user-proven e7e8a32
    // baseline is keeping generated CMake/Ninja state on a short temp path.
    Require(build, "RUNESCHEMA_BUILD_CACHE");
    Require(build, "[IO.Path]::GetTempPath()");
    Require(build, "Short generated build cache:");
    Require(build, "$allowedRoots = @($BuildRoot, $BuildCache)");

    // Do not patch or partially reconfigure the pinned UE4SS source. The exact
    // upstream configure path already completed a full local build at e7e8a32.
    Forbid(build, "Prepare-EmbeddedUE4SSCMake");
    Forbid(build, "UE4SS_PROJECTS=UE4SS");
    Forbid(build, "ENABLE_IDE_SOURCE_VISIBILITY=OFF");
    Forbid(cmake, "UVTD is disabled for embedded builds");

    std::cout << "UE4SS bootstrap contract passed.\n";
}
