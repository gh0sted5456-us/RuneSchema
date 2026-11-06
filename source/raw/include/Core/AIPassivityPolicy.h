#pragma once
#include <stdexcept>
namespace PS::AIPassivityPolicy {
enum class Mode:int {Inherit=0,Friendly=1,Angry=2};
inline Mode Read(int value){if(value<0||value>2)throw std::runtime_error("AI passivity must be 0, 1, or 2");return static_cast<Mode>(value);}
constexpr Mode Merge(Mode a,Mode b){return a==Mode::Friendly||b==Mode::Friendly?Mode::Friendly:a==Mode::Angry||b==Mode::Angry?Mode::Angry:Mode::Inherit;}
inline Mode ActiveEffect(int value,bool applied,bool blocked,bool removed,bool member,bool immediate){return applied&&!blocked&&!removed&&member&&immediate?Read(value):Mode::Inherit;}
constexpr bool Passive(bool native,Mode mode){return mode==Mode::Friendly?true:mode==Mode::Angry?false:native;}
constexpr unsigned char Attitude(unsigned char native,Mode mode,bool player){return !player||mode==Mode::Inherit?native:mode==Mode::Friendly?0:2;}
}
