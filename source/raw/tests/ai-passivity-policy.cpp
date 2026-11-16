#include "Core/AIPassivityPolicy.h"
#include <cassert>
using namespace PS::AIPassivityPolicy;
int main(){
 for(int a=0;a<=2;++a)for(int b=0;b<=2;++b){const auto x=Merge(Read(a),Read(b));assert(x==Merge(Read(b),Read(a)));if(a==1||b==1)assert(x==Mode::Friendly);else if(a==2||b==2)assert(x==Mode::Angry);else assert(x==Mode::Inherit);}
 for(bool native:{false,true}){assert(Passive(native,Mode::Inherit)==native);assert(Passive(native,Mode::Friendly));assert(!Passive(native,Mode::Angry));}
 for(unsigned char native:{0,1,2})for(auto mode:{Mode::Inherit,Mode::Friendly,Mode::Angry})assert(Attitude(native,mode,false)==native);
 assert(Attitude(2,Mode::Friendly,true)==0);assert(Attitude(0,Mode::Angry,true)==2);assert(Attitude(1,Mode::Inherit,true)==1);
 // Inherit consumes the current native answer, including world/profile changes during an active zone.
 bool now=false;assert(!Passive(now,Mode::Inherit));now=true;assert(Passive(now,Mode::Inherit));assert(!Passive(now,Mode::Angry));now=false;assert(!Passive(now,Mode::Inherit));
 for(int flags=0;flags<32;++flags)for(int mode=0;mode<=2;++mode){
  const bool applied=flags&1,blocked=flags&2,removed=flags&4,member=flags&8,immediate=flags&16;
  assert(ActiveEffect(mode,applied,blocked,removed,member,immediate)==(applied&&!blocked&&!removed&&member&&immediate?Read(mode):Mode::Inherit));
 }
 assert(ActiveEffect(99,false,false,false,true,true)==Mode::Inherit);
 bool badEffect=false;try{ActiveEffect(3,true,false,false,true,true);}catch(...){badEffect=true;}assert(badEffect);
 for(int invalid:{-1,3,16,17,100000}){bool rejected=false;try{Read(invalid);}catch(...){rejected=true;}assert(rejected);}
}
