#pragma once
#include <Zydis/Zydis.h>
#include <cstdint>
#include <set>
#include <span>
#include <stdexcept>

namespace PS::Buildings {
inline uintptr_t NativeExecTarget(uintptr_t start,std::span<const unsigned char> bytes,unsigned resultBytes){
    if(bytes.empty()||bytes.size()>2048||(resultBytes!=0&&resultBytes!=1&&resultBytes!=4))throw std::runtime_error("Native exec bounds or result size changed");
    std::set<ZydisRegister> results{ZYDIS_REGISTER_R8};uintptr_t lastCall=0,found=0;bool live=false,ended=false;
    const auto largest=[](ZydisRegister value){return ZydisRegisterGetLargestEnclosing(ZYDIS_MACHINE_MODE_LONG_64,value);};
    for(size_t offset=0;offset<bytes.size();){
        const auto at=start+offset;ZydisDisassembledInstruction step{};
        if(!ZYAN_SUCCESS(ZydisDisassembleIntel(ZYDIS_MACHINE_MODE_LONG_64,at,bytes.data()+offset,bytes.size()-offset,&step)))throw std::runtime_error("Native exec decode failed");
        const auto target=[&]{ZyanU64 value=0;if(step.operands[0].type!=ZYDIS_OPERAND_TYPE_IMMEDIATE||!ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(&step.info,&step.operands[0],at,&value)))throw std::runtime_error("Native exec call target changed");return uintptr_t(value);};
        if(step.info.mnemonic==ZYDIS_MNEMONIC_CALL){
            // WinGDK uses indirect FFrame helpers before the direct result-producing call.
            lastCall=step.operands[0].type==ZYDIS_OPERAND_TYPE_IMMEDIATE?target():0;live=lastCall!=0;
            for(auto reg:{ZYDIS_REGISTER_RAX,ZYDIS_REGISTER_RCX,ZYDIS_REGISTER_RDX,ZYDIS_REGISTER_R8,ZYDIS_REGISTER_R9,ZYDIS_REGISTER_R10,ZYDIS_REGISTER_R11})results.erase(reg);
        }else if(!resultBytes&&step.info.mnemonic==ZYDIS_MNEMONIC_JMP){
            const auto value=target();if(value<start||value-start>=bytes.size()){found=value;ended=true;break;}
        }else if(step.info.mnemonic==ZYDIS_MNEMONIC_MOV&&resultBytes&&step.operands[0].type==ZYDIS_OPERAND_TYPE_MEMORY
            &&step.operands[0].size==resultBytes*8&&results.contains(largest(step.operands[0].mem.base))
            &&step.operands[0].mem.index==ZYDIS_REGISTER_NONE&&step.operands[0].mem.disp.value==0
            &&step.operands[1].type==ZYDIS_OPERAND_TYPE_REGISTER
            &&step.operands[1].reg.value==(resultBytes==1?ZYDIS_REGISTER_AL:ZYDIS_REGISTER_EAX)){
            if(!live||!lastCall)throw std::runtime_error("Native result producer is not a direct call");
            if(found&&found!=lastCall)throw std::runtime_error("Native result producer is ambiguous");found=lastCall;
        }
        for(unsigned i=0;i<step.info.operand_count_visible;++i){const auto& operand=step.operands[i];
            if(operand.type!=ZYDIS_OPERAND_TYPE_REGISTER||!(operand.actions&ZYDIS_OPERAND_ACTION_MASK_WRITE))continue;
            const auto reg=largest(operand.reg.value);
            const bool alias=step.info.mnemonic==ZYDIS_MNEMONIC_MOV&&i==0&&operand.size==64&&step.operands[1].type==ZYDIS_OPERAND_TYPE_REGISTER&&results.contains(largest(step.operands[1].reg.value));
            if(alias)results.insert(reg);else results.erase(reg);
            if(reg==ZYDIS_REGISTER_RAX&&step.info.mnemonic!=ZYDIS_MNEMONIC_CALL)live=false;
        }
        if(step.info.mnemonic==ZYDIS_MNEMONIC_RET){ended=true;break;}offset+=step.info.length;
    }
    if(!ended||!found)throw std::runtime_error("Native implementation return contract changed");
    return found;
}
}
