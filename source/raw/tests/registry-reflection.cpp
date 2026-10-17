#include "Core/ReflectedLayout.h"
#include "Core/RegistryObjectIdentity.h"
#include "Generator/LoaderCapabilities.h"
#include <vector>
#include <string_view>
#include <iostream>
struct Field {std::string_view Name;int Offset,Size;};
template<class F> static void Rejects(F action){bool rejected=false;try{action();}catch(...){rejected=true;}if(!rejected)throw std::runtime_error("Invalid reflected layout accepted");}
int main() {
    const auto validate=[](int size,std::initializer_list<Field> values){PS::ReflectedLayout::Validate(size,std::span(values.begin(),values.size()));};
    validate(0,{});validate(64,{{"Input",8,8},{"ReturnValue",48,1}});
    validate(160,{{"ReturnValue",0,1},{"Input",64,8},{"ExtendedEvent",80,72}});
    Rejects([&]{validate(-1,{});});Rejects([&]{validate(65536,{});});
    Rejects([&]{validate(8,{{"Input",4,8}});});Rejects([&]{validate(8,{{"Input",-1,8}});});
    Rejects([&]{validate(16,{{"Input",0,8},{"ReturnValue",7,1}});});
    Rejects([&]{validate(16,{{"Input",0,8},{"Input",8,8}});});
    Rejects([&]{validate(16,{{"Input",8,0}});});
    unsigned checked=0;
    for(unsigned flags=0;flags<64;++flags) {
        const bool same=flags&1,path=flags&2,valid=flags&4,destroying=flags&8,owned=flags&16,rooted=flags&32;
        const bool expected=same&&path&&valid&&!destroying&&(!owned||rooted);
        if(PS::RegistryObjectIdentity::Matches(same,path,valid,destroying,owned,rooted,17,17)!=expected)
            throw std::runtime_error("Registry object retention policy changed");
        if(PS::RegistryObjectIdentity::Matches(same,path,valid,destroying,owned,rooted,17,18)
            ||PS::RegistryObjectIdentity::Matches(same,path,valid,destroying,owned,rooted,0,0))
            throw std::runtime_error("Registry accepted a stale/unassigned generation");
        ++checked;
    }
    if(PS::LoaderCapabilities.size()!=22)throw std::runtime_error("Loader audit catalogue changed; update coverage");
    std::cout<<"Reflected padding/reordering, bounds/alias rejection and "<<checked<<" lifetime combinations passed; loader catalogue: "<<PS::LoaderCapabilities.size()<<".\n";
}
