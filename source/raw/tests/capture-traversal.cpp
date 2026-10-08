#include "Generator/CaptureTraversal.h"
#include <array>
#include <vector>
#include <cassert>
#include <iostream>
using namespace PS::InspectionTools;
int main() {
    const std::array<int,20> sparse{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2};
    std::vector<int> found; unsigned reads=0;
    auto valid=[&](int i){++reads;return sparse[i]!=0;};
    auto visit=[&](int i){found.push_back(i);return true;};
    assert(VisitCaptureSlots(2,20,4096,64,valid,visit)==2);
    assert((found==std::vector<int>{0,19}) && reads==20);
    found.clear(); reads=0;
    assert(VisitCaptureSlots(2,20,4096,1,valid,visit)==1 && reads==1);
    found.clear(); reads=0;
    assert(VisitCaptureSlots(2,20,4096,64,valid,[](int){return false;})==0 && reads==1);
    reads=0; bool rejected=false;
    try {VisitCaptureSlots(2,20,10,64,valid,visit);}catch(...){rejected=true;}
    assert(rejected && reads==0);
    rejected=false; try {VisitCaptureSlots(4,2,4096,64,valid,visit);}catch(...){rejected=true;}
    assert(rejected && reads==0);
    assert(VisitCaptureSlots(0,0,4096,64,valid,visit)==0 && reads==0);
    std::cout<<"PASS: sparse tail entries, entry/slot budgets, early cancellation and invalid headers before access.\n";
}
