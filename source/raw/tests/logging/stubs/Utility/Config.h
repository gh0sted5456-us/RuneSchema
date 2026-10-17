#pragma once
#include <string_view>
namespace PS {
struct PSConfig {
    bool Advanced=false;
    static PSConfig* Get(){static PSConfig config;return &config;}
    bool IsDebugLoggingEnabled() const{return Advanced;}
    bool IsRoutineNotificationEnabled(std::string_view) const{return Advanced;}
};
}
