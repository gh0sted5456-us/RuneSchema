#pragma once
#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <string>
#include <utility>

namespace PS::PlayerCarryWeight {
inline bool Verified(double actual,double requested) {
    return std::isfinite(actual)&&std::isfinite(requested)
        &&std::abs(actual-requested)<=std::max(0.001,std::abs(requested)*0.00001);
}
// Diagnostics only: do not change the established last-applied-rule policy,
// combine independent multipliers, or compound them on each retry.
struct Owners {
    struct Selection {std::string Owner,Setting;};
    std::map<std::string,Selection> ByPlayer;
    std::optional<Selection> Record(const std::string& player,Selection next) {
        const auto prior=ByPlayer.find(player);
        std::optional<Selection> result;
        if(prior!=ByPlayer.end()&&prior->second.Setting!=next.Setting)result=prior->second;
        ByPlayer[player]=std::move(next);return result;
    }
};
}
