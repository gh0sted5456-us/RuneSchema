#pragma once
#include <atomic>
namespace PS::HelpyActivity {
inline std::atomic<bool> Visible{false};
inline std::atomic<bool> CaptureInput{false};
inline bool WantsInputCapture(){return Visible.load(std::memory_order_relaxed)&&CaptureInput.load(std::memory_order_relaxed);}
struct InputCapture {
    bool Move=false,Look=false;
    bool Applied()const{return Move||Look;}
    void Forget(){Move=Look=false;}
    template<class F> void Update(bool enabled,F apply){
        if(Move!=enabled){apply(true,enabled);Move=enabled;}
        if(Look!=enabled){apply(false,enabled);Look=enabled;}
    }
};
inline bool BackgroundAllowed(bool explicitJob=false){return explicitJob||Visible.load(std::memory_order_relaxed);}
}
