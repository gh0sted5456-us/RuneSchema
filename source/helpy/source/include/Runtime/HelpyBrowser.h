#pragma once
// Helpy's web browser ad blocker: uBlock Origin Lite, downloaded from its official GitHub release
// only after the player turns ad blocking on, checked against the release's SHA-256 and unpacked
// into Helpy's state folder. The browser page loads it from there.
#include <filesystem>
#include <string>

namespace PS::HelpyBrowser {
enum class Blocker { Off, Working, Ready, Failed };
struct View {
    Blocker State = Blocker::Off;
    std::string Version, Message;
    std::filesystem::path Folder; // The unpacked extension, when Ready.
    bool PendingActivation = false;
};
// Called each frame with the setting. On: uses the copy already downloaded, or downloads the
// latest release when there is none. Off: never touches the network (downloaded files stay).
void AdBlock(bool on);
// The player's "Check for update" button: downloads a newer release when there is one.
bool CheckForUpdate();
View Current();
// Publish the installed marker only after WebView2 accepts the candidate; recover on failure.
void Activated(const std::filesystem::path& folder) noexcept;
void Rejected(const std::filesystem::path& folder,const std::string& reason) noexcept;
void Shutdown() noexcept;
}
