#include "SDK/Structs/Custom/FArrayCopy.h"
#include <iostream>
#include <stdexcept>
using namespace RC::Unreal;
static void Check(bool value){if(!value)throw std::runtime_error("Reflected array transaction regression");}
int main()try{
    FArrayProperty property;FScriptArray target{{std::string(200,'v'),"retained"}};const auto original=target.Entries;
    {UECustom::FArrayCopy copy(&property,&target);auto& rows=static_cast<FScriptArray*>(copy.Data)->Entries;
     Check(rows==original&&rows.data()!=target.Entries.data());rows[0]="uncommitted";rows.push_back("modded");}
    Check(target.Entries==original&&property.Live==0&&FMemory::Allocations==0);
    try{UECustom::FArrayCopy copy(&property,&target);auto& rows=static_cast<FScriptArray*>(copy.Data)->Entries;rows[0]="changed";rows.push_back("modded");throw std::runtime_error("Injected late import failure");}catch(const std::runtime_error&){}
    Check(target.Entries==original&&property.Live==0&&FMemory::Allocations==0);
    property.FailAfter=0;bool refused=false;try{UECustom::FArrayCopy copy(&property,&target);}catch(...){refused=true;}
    Check(refused&&target.Entries==original&&property.Live==0&&FMemory::Allocations==0);property.FailAfter=-1;
    {UECustom::FArrayCopy copy(&property,&target);auto& rows=static_cast<FScriptArray*>(copy.Data)->Entries;rows[0]="committed";rows.push_back("modded");const auto expected=rows;
     property.FailAfter=0;bool threw=false;try{copy.Commit();}catch(...){threw=true;}property.FailAfter=-1;
     Check(!threw&&target.Entries==expected);copy.Commit();Check(target.Entries==expected);}
    Check(property.Live==0&&FMemory::Allocations==0);
    for(int i=0;i<1000;++i){try{UECustom::FArrayCopy copy(&property,&target);static_cast<FScriptArray*>(copy.Data)->Entries.push_back("temporary");throw std::runtime_error("Injected failure");}catch(const std::runtime_error&){}Check(property.Live==0&&FMemory::Allocations==0);}
    for(int fault=0;fault<4;++fault){property.Dimensions=fault==0?2:1;property.Size=fault==1?1:sizeof(FScriptArray);property.FailInitialize=fault==2;FMemory::FailAllocation=fault==3;
      refused=false;try{UECustom::FArrayCopy copy(&property,&target);}catch(...){refused=true;}Check(refused&&property.Live==0&&FMemory::Allocations==0);}
    std::cout<<"Array staging, allocation/copy/init failures, nonallocating commit and 1000 failed transactions passed.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
