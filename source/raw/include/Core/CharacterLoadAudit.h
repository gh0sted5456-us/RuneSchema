#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
namespace PS::CharacterLoadAudit {
enum class Gate { Validation,PlayerState };
struct Receipt {std::uint64_t Token=0;std::size_t RegistryPruned=0,ValidationBypasses=0,PlayerStateBypasses=0;bool Ambiguous=false,GameplayPrepared=false;};
inline std::atomic<std::uint64_t> Next{0};
inline thread_local Receipt Current;
inline std::uint64_t Begin()noexcept{
    if(Current.Token)return 0; // never attribute a nested/unobserved load to a new character
    auto token=Next.fetch_add(1,std::memory_order_relaxed)+1;
    if(!token)token=Next.fetch_add(1,std::memory_order_relaxed)+1;
    Current={token};return token;
}
inline std::uint64_t Token()noexcept{return Current.Ambiguous?0:Current.Token;}
inline void Prepared()noexcept{if(Token())Current.GameplayPrepared=true;}
inline bool PreparedForFallback()noexcept{return Token()&&Current.GameplayPrepared;}
inline void Ambiguous()noexcept{if(Current.Token)Current.Ambiguous=true;}
inline void Pruned(std::size_t count)noexcept{if(Token())Current.RegistryPruned+=count;}
inline bool Bypassed(Gate gate)noexcept{
    if(!Token())return false;
    auto& count=gate==Gate::Validation?Current.ValidationBypasses:Current.PlayerStateBypasses;
    return ++count==1;
}
inline std::optional<Receipt> End(std::uint64_t token)noexcept{
    if(!token||Current.Token!=token)return {};
    auto result=Current;Current={};return result;
}
}
