#include "SDK/Structs/Custom/FScriptMapHelper.h"
#include "SDK/Structs/Custom/FScriptArrayHelper.h"
#include <iostream>
using namespace RC::Unreal;
static void Check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main() try {
    // Compile/link the production FScriptMapHelper.cpp against the narrow stubs.
    // Padding deliberately makes old key-size + value-size allocation too small.
    FProperty key{4,4,0x11},value{16,16,0x22};FScriptMap map;
    FScriptMapLayout layout{16,{40}};
    UECustom::FScriptMapHelper helper(&map,layout,&key,&value);
    {
        UECustom::FManagedValue pair;
        helper.InitializePair(pair);
        Check(FMemory::GuardIntact(),"map initialization wrote beyond allocated bytes");
        Check(FMemory::requested>=32 && FMemory::requestedAlignment>=16,"map alignment/size lost");
        Check(static_cast<uint8*>(helper.GetValuePtr(pair.GetData()))[15]==0x22,"value not fully initialized");
        // A live row after three deleted slots must update, not become a duplicate.
        map.slots.resize(3,{false,{}});
        helper.Add(pair);
        Check(map.Num()==1,"initial insert");
        static_cast<uint8*>(helper.GetValuePtr(pair.GetData()))[0]=0x33;
        helper.Add(pair);
        Check(map.Num()==1,"sparse update created duplicate key");
    }
    Check(FMemory::live==0 && key.live==1 && value.live==1,"temporary map properties leaked");
    Check(static_cast<uint8*>(map.GetData(3,layout))[16]==0x33,"stored value did not survive temporary destruction");
    Check(helper.Remove(map.GetData(3,layout)) && map.Num()==0,"sparse tail not removed");
    Check(key.live==0 && value.live==0,"stored map properties leaked");

    UECustom::FManagedValue pair;
    for(auto invalid:{FScriptMapLayout{2,{40}},FScriptMapLayout{16,{20}}}){
        bool rejected=false;try{UECustom::FScriptMapHelper bad(&map,invalid,&key,&value);bad.InitializePair(pair);}catch(...){rejected=true;}
        Check(rejected,"invalid layout accepted");
    }
    for(int fault=0;fault<3;++fault){
        FMemory::failAllocation=fault==0;
        key.failInitialize=fault==1;
        value.failInitialize=fault==2;
        bool rejected=false;
        try{helper.InitializePair(pair);}catch(...){rejected=true;}
        Check(rejected && !pair.GetData() && FMemory::live==0 && key.live==0 && value.live==0,
              "partially initialized pair leaked");
    }
    value.failInitialize=false;
    bool unwound=false;
    try {
        UECustom::FManagedValue scratch;
        helper.InitializePair(scratch);
        throw std::runtime_error("Injected import failure");
    } catch(const std::runtime_error&) {unwound=true;}
    Check(unwound && FMemory::live==0 && key.live==0 && value.live==0,"map scratch leaked during import failure");

    FArrayProperty property{&value};
    FScriptArray rows;
    UECustom::FScriptArrayHelper array(&rows,&property);
    {
        UECustom::FManagedValue element;
        array.InitializeValue(element);
        array.Add(element);
    }
    Check(FMemory::live==0 && value.live==1,"temporary array property leaked");
    Check(static_cast<uint8*>(rows.GetData())[15]==0x22,"stored array element did not survive temporary destruction");
    array.Empty();
    Check(value.live==0,"stored array property leaked");
    std::cout<<"PASS: reflected map and array temporary cleanup, initialization failures and retained copies.\n";
} catch(const std::exception& e) {
    std::cerr<<e.what()<<'\n';return 1;
}
