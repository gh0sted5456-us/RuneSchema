#pragma once
// Portable key policy. Values are Windows virtual-key codes, as used by UE4SS.
#include <atomic>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace PS::HelpyHotkeys {
struct Key {int code;std::string name;};
inline const std::vector<Key>& Choices() {
    static const auto values=[] {
        std::vector<Key> out;
        for(int i=1;i<=24;++i)out.push_back({111+i,"F"+std::to_string(i)});
        out.push_back({45,"Insert"});out.push_back({36,"Home"});out.push_back({35,"End"});
        out.push_back({19,"Pause"});out.push_back({145,"ScrollLock"});return out;
    }();return values;
}
inline std::optional<Key> Find(int code) {for(const auto& key:Choices())if(key.code==code)return key;return {};}
inline std::optional<Key> Find(std::string_view name) {for(const auto& key:Choices())if(key.name==name)return key;return {};}
inline std::atomic<int> Active{113},SuppressedUntilRelease{0}; // F2 by default
inline std::atomic<bool> Capturing{false};
// Observed by the input-safety tick even while the Helpy panel is closed.
// A settings-panel rebind must not require opening Helpy to unlock its new key.
inline void ObserveRelease(int code,bool down) {
    if(!code||down)return;
    int expected=code;
    SuppressedUntilRelease.compare_exchange_strong(expected,0);
}
inline bool Matches(int code) {return code==Active.load()&&!Capturing.load()&&code!=SuppressedUntilRelease.load();}
inline std::string Name(){const auto key=Find(Active.load());return key?key->name:"F2";}
}
