#pragma once
#include <atomic>
namespace PS::HelpyPopupInput {
inline bool Route(bool visible,bool gameForeground,bool gameMessage,bool removed){return visible&&gameForeground&&gameMessage&&removed;}
class HotkeyLatch {
    std::atomic<bool> m_down=false;
public:
    bool Poll(bool down){const bool previous=m_down.exchange(down);return down&&!previous;}
    // Mark the edge before the UI closes: the game-thread poll cannot reopen it.
    bool Press(){return !m_down.exchange(true);}
    void Release(){m_down=false;}
};
}
