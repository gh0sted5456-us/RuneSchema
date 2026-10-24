#pragma once
#include <charconv>
#include <cstdint>
#include <string>
namespace PS::RuntimeDuration {
enum class Unit {Automatic,Milliseconds,Seconds,Minutes};
inline constexpr const char* Labels[]{"Automatic","Milliseconds","Seconds","Minutes"};
inline std::string Format(std::int64_t milliseconds,Unit unit=Unit::Automatic){
    if(milliseconds<0)return "Not available";
    const bool automatic=unit==Unit::Automatic;
    if(automatic)unit=milliseconds<1000?Unit::Milliseconds:milliseconds<60000?Unit::Seconds:Unit::Minutes;
    if(unit==Unit::Milliseconds)return std::to_string(milliseconds)+" ms";
    char buffer[64];const double divisor=unit==Unit::Minutes?60000.:1000.;
    const auto converted=std::to_chars(buffer,buffer+sizeof(buffer),milliseconds/divisor,std::chars_format::fixed,automatic?1:3);
    if(converted.ec!=std::errc{})return "Not available";
    std::string text(buffer,converted.ptr);while(text.ends_with('0'))text.pop_back();if(text.ends_with('.'))text.pop_back();
    return text+(unit==Unit::Minutes?" min":" s");
}
}
