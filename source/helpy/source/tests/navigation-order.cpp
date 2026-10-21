#include "Core/HelpyNavigationOrder.h"
#include <cassert>
int main(int argc,char** argv){
    assert(argc==2);namespace N=PS::HelpyNavigationOrder;namespace fs=std::filesystem;
    const auto folder=fs::path(argv[1])/("navigation-order-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));const auto file=folder/"navigation.json";
    const std::vector<int> defaults{11,0,4,13,5};assert(N::Load(file));assert(N::Ordered("sidebar",defaults)==defaults);
    assert(N::Move("sidebar",13,11,defaults));assert((N::Ordered("sidebar",defaults)==std::vector<int>{13,11,0,4,5}));
    assert(N::Load(file));assert(N::Ordered("sidebar",defaults).front()==13);
    assert((N::Ordered("sidebar",{11,0,4,13,5,99})==std::vector<int>{13,11,0,4,5,99}));
    assert((N::Ordered("sidebar",{11,4,5})==std::vector<int>{11,4,5})); // Hidden/removed routes cannot dispatch stale IDs.
    assert(!N::Move("sidebar",99,11,defaults));assert(N::Reset("sidebar"));assert(N::Ordered("sidebar",defaults)==defaults);
    assert(N::Move("home",4,11,defaults));PS::ConfigFiles::Write(file,"external change");assert(!N::Reset("home"));assert(PS::ConfigFiles::Read(file)=="external change");
    assert(!N::Load(file));assert(!N::Move("home",4,11,defaults));
    bool duplicate=false;try{(void)N::Validate({{"Format",1},{"Orders",{{"home",{1,1}}}}});}catch(...){duplicate=true;}assert(duplicate);
    bool traversal=false;try{(void)N::Validate({{"Format",1},{"Orders",{{"../home",{1}}}}});}catch(...){traversal=true;}assert(traversal);
}
