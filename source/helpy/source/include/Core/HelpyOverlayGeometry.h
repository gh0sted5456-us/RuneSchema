#pragma once
#include <algorithm>

namespace PS {
// Integer-only layout geometry for HELPY's non-activating notification surface.
// Windows POINT/RECT members are LONG; normalize them at the API boundary
// rather than mixing LONG and int in min/max/clamp expressions.
struct HelpyOverlayGeometry {
    struct Bounds {
        int ClientX,ClientY,ClientWidth;
        int MonitorLeft,MonitorTop,MonitorRight,MonitorBottom;
    };
    struct Position { int X,Y; };

    template<class Point,class Rectangle>
    static Bounds Normalize(const Point& clientOrigin,const Rectangle& client,const Rectangle& monitorWork) noexcept {
        return {
            static_cast<int>(clientOrigin.x),
            static_cast<int>(clientOrigin.y),
            std::max(1,static_cast<int>(client.right-client.left)),
            static_cast<int>(monitorWork.left),
            static_cast<int>(monitorWork.top),
            static_cast<int>(monitorWork.right),
            static_cast<int>(monitorWork.bottom)
        };
    }
    static int AvailableWidth(const Bounds& b,int margin) noexcept {
        return std::min(b.ClientWidth-2*margin,b.MonitorRight-b.MonitorLeft-2*margin);
    }
    static int MaxHeight(const Bounds& b,int margin) noexcept {
        return std::max(1,b.MonitorBottom-std::max(b.MonitorTop,b.ClientY+margin)-margin);
    }
    static Position Place(const Bounds& b,int width,int height,int margin) noexcept {
        return {
            std::clamp(b.ClientX+b.ClientWidth-width-margin,b.MonitorLeft,std::max(b.MonitorLeft,b.MonitorRight-width)),
            std::clamp(b.ClientY+margin,b.MonitorTop,std::max(b.MonitorTop,b.MonitorBottom-height))
        };
    }
};
} // namespace PS
