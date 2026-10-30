#include "Runtime/BuildingPolicy/AdapterState.h"
#include <cassert>
#include <stdexcept>
#include <string>

int main(){
    PS::Buildings::AdapterState support,farm,otherWorld;
    unsigned supportCalls=0,farmCalls=0,reports=0,effects=0,otherCalls=0;
    const auto report=[&](const char* message){assert(std::string(message)=="unsupported native contract");++reports;};
    for(unsigned tick=0;tick<10000;++tick){
        support.Update([&]{++supportCalls;throw std::runtime_error("unsupported native contract");},report);
        farm.Update([&]{++farmCalls;throw std::runtime_error("unsupported native contract");},report);
        ++effects;
        otherWorld.Update([&]{++otherCalls;},report);
    }
    assert(support.Unavailable&&farm.Unavailable&&!otherWorld.Unavailable);
    assert(supportCalls==1&&farmCalls==1&&reports==2&&effects==10000&&otherCalls==10000);
    support={};support.Update([&]{++supportCalls;},report);
    assert(supportCalls==2&&!support.Unavailable&&reports==2);
    PS::Buildings::AdapterState freshWorld;
    freshWorld.Update([&]{++farmCalls;},report);
    assert(farmCalls==2&&!freshWorld.Unavailable);
    PS::Buildings::AdapterState pending;bool ready=false;unsigned bindings=0;
    const auto bind=[&]{if(!ready)return;++bindings;};
    pending.Update(bind,report);assert(!pending.Unavailable&&bindings==0);
    ready=true;pending.Update(bind,report);assert(!pending.Unavailable&&bindings==1);
}
