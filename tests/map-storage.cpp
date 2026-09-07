#include "SDK/Structs/Custom/FScriptMapHelper.h"
#include <iostream>
using namespace RC::Unreal;
namespace UECustom {
FManagedValue::~FManagedValue() {FMemory::Free(Data);}
void FManagedValue::Copy(void* value){Data=value;}
void* FManagedValue::GetData(){return Data;}
}
static void Check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main() try {
    // Compile/link the production FScriptMapHelper.cpp against the narrow stubs.
    // Padding deliberately makes old key-size + value-size allocation too small.
    FProperty key{4,4,0x11},value{16,16,0x22};FScriptMap map;
    FScriptMapLayout layout{16,{40}};
    UECustom::FScriptMapHelper helper(&map,layout,&key,&value);
    UECustom::FManagedValue pair;helper.InitializePair(pair);
    Check(FMemory::GuardIntact(),"map initialization wrote beyond allocated bytes");
    Check(FMemory::requested>=32 && FMemory::requestedAlignment>=16,"map alignment/size lost");
    Check(static_cast<uint8*>(helper.GetValuePtr(pair.GetData()))[15]==0x22,"value not fully initialized");
    // A live row after three deleted slots must update, not become a duplicate.
    map.slots.resize(3,{false,{}});helper.Add(pair);Check(map.Num()==1,"initial insert");
    static_cast<uint8*>(helper.GetValuePtr(pair.GetData()))[0]=0x33;
    helper.Add(pair);Check(map.Num()==1,"sparse update created duplicate key");
    Check(static_cast<uint8*>(map.GetData(3,layout))[16]==0x33,"sparse tail not updated");
    Check(helper.Remove(helper.GetKeyPtr(pair.GetData())) && map.Num()==0,"sparse tail not removed");
    for(auto invalid:{FScriptMapLayout{2,{40}},FScriptMapLayout{16,{20}}}){
        bool rejected=false;try{UECustom::FScriptMapHelper bad(&map,invalid,&key,&value);bad.InitializePair(pair);}catch(...){rejected=true;}
        Check(rejected,"invalid layout accepted");
    }
    std::cout<<"PASS: production map helper padded allocation, guard bytes, alignment, sparse update/remove and invalid layouts.\n";
} catch(const std::exception& e) {
    std::cerr<<e.what()<<'\n';return 1;
}
