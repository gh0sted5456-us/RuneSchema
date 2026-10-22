#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <cctype>
namespace PS::NativePersistencePolicy {
inline constexpr const char* Variable="dom.PersistenceStrictMode";
inline bool SessionRequested(bool dedicated,bool bypass,std::string_view mode)noexcept{return dedicated?bypass:mode!="vanilla";}
inline bool BypassRequested(bool dedicated,bool bypass,std::string_view mode)noexcept{return bypass||(!dedicated&&mode!="vanilla");}
enum class Outcome { Disabled,Unavailable,AlreadyTolerant,Applied,Failed,Restored,RestoreFailed };
struct Lease {bool RestoreNeeded=false;};
inline std::optional<bool> Parse(std::string_view value){
    while(!value.empty()&&std::isspace(static_cast<unsigned char>(value.front())))value.remove_prefix(1);
    while(!value.empty()&&std::isspace(static_cast<unsigned char>(value.back())))value.remove_suffix(1);
    if(value=="0"||value=="false"||value=="False")return false;
    if(value=="1"||value=="true"||value=="True")return true;
    return {};
}
// A lease changes strict=true only. It never changes an unknown/default value,
// suppresses a failure, accepts a character, or edits a save.
template<class Read,class Write> Outcome Release(Lease& lease,Read&& read,Write&& write)noexcept{
    if(!lease.RestoreNeeded)return Outcome::Restored;
    try{
        const auto current=read();
        if(current&&*current){lease.RestoreNeeded=false;return Outcome::Restored;}
        write(true);
        const auto restored=read();
        if(restored&&*restored){lease.RestoreNeeded=false;return Outcome::Restored;}
    }catch(...){}
    return Outcome::RestoreFailed;
}
template<class Read,class Write> Outcome Acquire(Lease& lease,bool enabled,Read&& read,Write&& write)noexcept{
    if(!enabled)return Outcome::Disabled;
    if(lease.RestoreNeeded)return Outcome::Failed;
    try{
        const auto current=read();
        if(!current)return Outcome::Unavailable;
        if(!*current)return Outcome::AlreadyTolerant;
        lease.RestoreNeeded=true; // rollback is required even if a setter throws
        write(false);
        const auto verified=read();
        if(verified&&!*verified)return Outcome::Applied;
    }catch(...){}
    Release(lease,read,write);
    return Outcome::Failed;
}
// Retain the lease across load frames and reapply only if native code resets it.
template<class Read,class Write> Outcome Hold(Lease& lease,bool enabled,Read&& read,Write&& write)noexcept{
    if(!enabled)return Outcome::Disabled;
    if(lease.RestoreNeeded)try{
        const auto current=read();if(!current)return Outcome::Unavailable;
        if(!*current)return Outcome::AlreadyTolerant;
        lease.RestoreNeeded=false;
    }catch(...){return Outcome::Failed;}
    return Acquire(lease,true,read,write);
}
}
