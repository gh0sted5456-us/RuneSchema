#pragma once
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
namespace PS::CharacterEntryBypassPlan {
struct Gate {std::uint32_t Call,Test,Success;};
// Discovery only; runtime also certifies decoded boundaries and the caller.
inline std::optional<Gate> Find(std::span<const std::uint8_t> body,std::uint32_t caller,std::uint32_t validation) {
    std::optional<Gate> found;
    for(std::size_t i=0;i+9<=body.size();++i) {
        if(body[i]!=0xe8||body[i+5]!=0x84||body[i+6]!=0xc0)continue;
        std::int32_t relative;std::memcpy(&relative,body.data()+i+1,4);
        if(static_cast<std::int64_t>(caller)+static_cast<std::int64_t>(i)+5+relative!=validation)continue;
        std::int64_t destination=0;
        if(body[i+7]==0x75)destination=static_cast<std::int64_t>(i)+9+static_cast<std::int8_t>(body[i+8]);
        else if(i+13<=body.size()&&body[i+7]==0x0f&&body[i+8]==0x85) {
            std::int32_t jump;std::memcpy(&jump,body.data()+i+9,4);destination=static_cast<std::int64_t>(i)+13+jump;
        }else continue;
        if(destination<=static_cast<std::int64_t>(i+8)||destination>=static_cast<std::int64_t>(body.size()))continue;
        if(found)return {};
        found=Gate{caller+static_cast<std::uint32_t>(i),caller+static_cast<std::uint32_t>(i+5),caller+static_cast<std::uint32_t>(destination)};
    }
    return found;
}
inline std::uintptr_t AcceptRejected(std::uintptr_t result) noexcept {return (result&0xffu)?result:(result|std::uintptr_t{1});}
}
