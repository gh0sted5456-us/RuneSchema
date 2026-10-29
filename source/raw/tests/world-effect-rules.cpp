#include "Core/WorldEffectRules.h"
#include <iostream>
namespace W=PS::WorldEffectRules;
void Need(bool value){if(!value)throw std::runtime_error("world effect check failed");}
template<class F>void Reject(F run){try{run();}catch(const std::runtime_error&){return;}throw std::runtime_error("invalid effect accepted");}
int main(){
    Need(W::Scalar(1,4)==4&&W::Scalar(4,4)==4&&W::Scalar(4,2)==4);
    Need(W::Quantity(3,4)==12&&W::Quantity(3,1)==3&&W::Quantity(3,0)==0);
    Need(W::Quantity(1,1.5)==2&&W::Quantity(std::numeric_limits<int>::max(),1)==std::numeric_limits<int>::max());
    Reject([]{W::Scalar(1,-1);});Reject([]{W::Scalar(1,100001);});
    Reject([]{W::Scalar(1,std::numeric_limits<double>::quiet_NaN());});
    Reject([]{W::Quantity(std::numeric_limits<int>::max(),4);});
    Reject([]{W::Quantity(1,std::numeric_limits<double>::infinity());});
    std::cout<<"world-effect-rules: PASS\n";
}
