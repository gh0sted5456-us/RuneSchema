#pragma once
#include <cstdint>
#include <string>
namespace PS::HelpyUpdates {
struct View {
    std::string MainVersion,ReleaseVersion,MainError,ReleaseError,CacheError;
    std::int64_t CheckedAt=0;
    std::string GameUpdateTitle,GameUpdateUrl,GameUpdateError;
    std::int64_t GameUpdateDate=0;
};
const View& Current();
bool Busy();
int Stage();
int CooldownSeconds();
bool Start(const std::string& installed);
// UI thread only. Returns true once when a completed check finds a newer version.
bool Tick(bool automatic,bool networkAllowed,const std::string& installed);
void Cancel() noexcept;
void Shutdown() noexcept;
}
