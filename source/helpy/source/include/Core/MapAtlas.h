#pragma once
#include "Core/MapViewport.h"
#include "ScornedTravel.h"
#include <array>
#include <limits>
#include <span>
#include <string_view>

namespace PS::MapAtlas {
using MapProjection::Point;
using MapProjection::Projection;
struct Bounds {
    Point Min, Max;
    constexpr bool Contains(Point p) const {
        return p.X >= Min.X && p.X <= Max.X && p.Y >= Min.Y && p.Y <= Max.Y;
    }
    constexpr Projection Project(double minZ, double maxZ) const {
        return {{(Min.X + Max.X) / 2, (Min.Y + Max.Y) / 2}, {1, 0}, {0, -1},
            (Max.X - Min.X) / 2, (Max.Y - Min.Y) / 2, minZ, maxZ};
    }
};
inline constexpr Bounds Mainland{{-12000, -118550}, {408000, 301450}};
inline constexpr Bounds Scorned{{-850000, 608000}, {-660000, 798000}};
inline constexpr Bounds Overview{{Scorned.Min.X, Mainland.Min.Y}, {Mainland.Max.X, Scorned.Max.Y}};
inline constexpr Point MainlandPortal{95654.399393, 59613.574157};
inline constexpr Point ScornedPortal=ScornedTravel::Entrance;

struct Area {
    std::string_view Name;
    std::span<const Point> Shape;
    size_t Group;
};
#include "Core/MapAtlasData.inl"
#include "Core/MapAtlasInset.inl"

inline constexpr size_t OverviewTab = 0, ScornedTab = 1, MainlandTab = 2, FirstRegionTab = 3;
inline constexpr size_t TabCount = FirstRegionTab + SurfaceAreas.size();
inline constexpr std::array Groups{"Brynmoor", "Ghornfell", "Fellhollow", "Dowdun Reach", "Umbral Sands"};

inline constexpr size_t PrimaryTab(size_t tab) { return tab >= FirstRegionTab ? MainlandTab : tab; }

inline Bounds Extent(std::span<const Point> shape) {
    Bounds bounds{shape.front(), shape.front()};
    for (const auto p : shape) {
        bounds.Min = {std::min(bounds.Min.X, p.X), std::min(bounds.Min.Y, p.Y)};
        bounds.Max = {std::max(bounds.Max.X, p.X), std::max(bounds.Max.Y, p.Y)};
    }
    return bounds;
}

inline Bounds Frame(size_t tab) {
    if (tab == OverviewTab) return Overview;
    if (tab == ScornedTab) return Scorned;
    if (tab == MainlandTab) return Mainland;
    auto bounds = Extent(SurfaceAreas[tab - FirstRegionTab].Shape);
    const double pad = std::max(4000.0, std::max(bounds.Max.X - bounds.Min.X, bounds.Max.Y - bounds.Min.Y) * .07);
    return {{std::max(Mainland.Min.X, bounds.Min.X - pad), std::max(Mainland.Min.Y, bounds.Min.Y - pad)},
        {std::min(Mainland.Max.X, bounds.Max.X + pad), std::min(Mainland.Max.Y, bounds.Max.Y + pad)}};
}

inline std::string_view Title(size_t tab) {
    if (tab == OverviewTab) return "Overview";
    if (tab == ScornedTab) return "Scorned Wilderness";
    if (tab == MainlandTab) return "Mainland";
    return SurfaceAreas[tab - FirstRegionTab].Name;
}

inline bool Contains(std::span<const Point> shape, Point point) {
    bool inside = false;
    for (size_t i = 0, j = shape.size() - 1; i < shape.size(); j = i++) {
        const auto a = shape[j], b = shape[i];
        const double cross = (point.X - a.X) * (b.Y - a.Y) - (point.Y - a.Y) * (b.X - a.X);
        if (std::abs(cross) < .01 && point.X >= std::min(a.X, b.X) && point.X <= std::max(a.X, b.X) &&
            point.Y >= std::min(a.Y, b.Y) && point.Y <= std::max(a.Y, b.Y)) return true;
        if ((a.Y > point.Y) != (b.Y > point.Y) && point.X < a.X + (point.Y - a.Y) * (b.X - a.X) / (b.Y - a.Y))
            inside = !inside;
    }
    return inside;
}

inline size_t RegionAt(Point point) {
    if (Scorned.Contains(point) && (Contains(ScornedHub, point) || Contains(ScornedRaid, point))) return ScornedTab;
    if (!Mainland.Contains(point)) return OverviewTab;
    // When adjacent sampled splines overlap, prefer the smaller, more specific named area.
    static const auto extents = [] {
        std::array<Bounds, SurfaceAreas.size()> result;
        for (size_t i = 0; i < result.size(); ++i) result[i] = Extent(SurfaceAreas[i].Shape);
        return result;
    }();
    double smallest = std::numeric_limits<double>::infinity();
    size_t tab = MainlandTab;
    for (size_t i = 0; i < SurfaceAreas.size(); ++i) {
        const auto& b = extents[i];
        const double area = (b.Max.X - b.Min.X) * (b.Max.Y - b.Min.Y);
        if (area < smallest && b.Contains(point) && Contains(SurfaceAreas[i].Shape, point)) {
            tab = FirstRegionTab + i;
            smallest = area;
        }
    }
    return tab;
}

inline bool ScornedDisplayOnly(Point point) {
    if (!Mainland.Contains(point)) return false;
    const Point uv{(point.X-Mainland.Min.X)/(Mainland.Max.X-Mainland.Min.X),
        (point.Y-Mainland.Min.Y)/(Mainland.Max.Y-Mainland.Min.Y)};
    if (uv.X>.32 || uv.Y<.18 || uv.Y>.49) return false;
    // Native mainland polygons take precedence where the painted inset overlaps them.
    return (Contains(ScornedDisplayHub,uv)||Contains(ScornedDisplayRaid,uv))&&RegionAt(point)==MainlandTab;
}

inline std::string_view RegionNameAt(Point point) {
    const auto tab = RegionAt(point);
    if (tab == ScornedTab) return Contains(ScornedHub, point) ? "Rasthin's Rest" : "Scorned Wilderness";
    return tab >= FirstRegionTab ? Title(tab) : std::string_view{};
}

inline bool MatchesMainland(const Projection& p) {
    const auto expected = Mainland.Project(p.MinZ, p.MaxZ);
    return p.Valid() && std::abs(p.Center.X - expected.Center.X) < 1 && std::abs(p.Center.Y - expected.Center.Y) < 1 &&
        std::abs(p.HalfWidth - expected.HalfWidth) < 1 && std::abs(p.HalfHeight - expected.HalfHeight) < 1 &&
        std::abs(p.Right.X - 1) < .0001 && std::abs(p.Right.Y) < .0001 &&
        std::abs(p.Up.X) < .0001 && std::abs(p.Up.Y + 1) < .0001;
}

struct RasterClip { Point ScreenMin, ScreenMax, RasterMin, RasterMax; };
inline bool ClipRaster(const Projection& active, const MapProjection::Viewport& view, Bounds raster, RasterClip& clip) {
    const auto visibleMin = view.ToUV({0, 0}), visibleMax = view.ToUV({view.Width, view.Height});
    const auto a = active.ToWorld({std::clamp(visibleMin.X, 0.0, 1.0), std::clamp(visibleMin.Y, 0.0, 1.0)});
    const auto b = active.ToWorld({std::clamp(visibleMax.X, 0.0, 1.0), std::clamp(visibleMax.Y, 0.0, 1.0)});
    const Bounds visible{{std::max(a.X, raster.Min.X), std::max(a.Y, raster.Min.Y)},
        {std::min(b.X, raster.Max.X), std::min(b.Y, raster.Max.Y)}};
    if (visible.Min.X >= visible.Max.X || visible.Min.Y >= visible.Max.Y) return false;
    const auto image = raster.Project(active.MinZ, active.MaxZ);
    clip = {view.ToPixel(active.ToUV(visible.Min)), view.ToPixel(active.ToUV(visible.Max)),
        image.ToUV(visible.Min), image.ToUV(visible.Max)};
    return true;
}

struct TabState {
    size_t Active = OverviewTab, Requested = OverviewTab;
    bool Following = false;

    void Track(Point point) {
        if (Following) Requested = RegionAt(point);
    }
    void Pause() {
        Following = false;
        Requested = Active;
    }
    void Show(size_t visible) {
        if (visible == Active) return;
        if (visible != Requested) Following = false;
        Active = Requested = visible;
    }
};
}
