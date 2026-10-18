#pragma once
#include "Core/CharacterEntryBypassPlan.h"
#include "Zydis/Zydis.h"
#include <vector>

namespace PS::CharacterEntryBypassPlan {
inline bool Nonvolatile(ZydisRegister reg) noexcept {
    return reg==ZYDIS_REGISTER_RBX||reg==ZYDIS_REGISTER_RBP||reg==ZYDIS_REGISTER_RSI||reg==ZYDIS_REGISTER_RDI
        ||reg==ZYDIS_REGISTER_R12||reg==ZYDIS_REGISTER_R13||reg==ZYDIS_REGISTER_R14||reg==ZYDIS_REGISTER_R15;
}
inline std::optional<std::uint32_t> DecodeReturn(std::span<const std::uint8_t> body) {
    if(body.empty()||body.size()>65536)return {};
    ZydisDecoder decoder{};ZydisDecoderInit(&decoder,ZYDIS_MACHINE_MODE_LONG_64,ZYDIS_STACK_WIDTH_64);
    std::vector<Instruction> code;
    for(std::size_t at=0;at<body.size();) {
        ZydisDecodedInstruction ins{};ZydisDecodedOperand ops[ZYDIS_MAX_OPERAND_COUNT]{};
        if(!ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder,body.data()+at,body.size()-at,&ins,ops))||!ins.length)return {};
        Instruction op{static_cast<std::uint32_t>(at),ins.length,Kind::Other};
        const auto reg=[&](unsigned i,ZydisRegister expected){return ops[i].type==ZYDIS_OPERAND_TYPE_REGISTER&&ops[i].reg.value==expected;};
        const auto stackMemory=[&](unsigned i){return ops[i].type==ZYDIS_OPERAND_TYPE_MEMORY
            &&ops[i].mem.index==ZYDIS_REGISTER_NONE&&(ops[i].mem.base==ZYDIS_REGISTER_RSP||ops[i].mem.base==ZYDIS_REGISTER_R11);};
        if(ins.mnemonic==ZYDIS_MNEMONIC_RET)op.Type=ins.operand_count_visible?Kind::Invalid:Kind::Return;
        else if(ins.meta.category==ZYDIS_CATEGORY_RET)op.Type=Kind::Invalid;
        else if(ins.meta.category==ZYDIS_CATEGORY_COND_BR||ins.meta.category==ZYDIS_CATEGORY_UNCOND_BR) {
            if(!ins.operand_count_visible||ops[0].type!=ZYDIS_OPERAND_TYPE_IMMEDIATE||!ops[0].imm.is_relative)op.Type=Kind::Invalid;
            else {op.Type=Kind::Branch;op.Target=static_cast<std::int64_t>(at+ins.length)+ops[0].imm.value.s;}
        }
        else if(ins.mnemonic==ZYDIS_MNEMONIC_POP&&ins.operand_count_visible==1
            &&ops[0].type==ZYDIS_OPERAND_TYPE_REGISTER&&Nonvolatile(ops[0].reg.value))op.Type=Kind::Restore;
        else if(ins.mnemonic==ZYDIS_MNEMONIC_MOV&&ins.operand_count_visible==2) {
            if(ops[0].type==ZYDIS_OPERAND_TYPE_REGISTER&&Nonvolatile(ops[0].reg.value)&&ops[0].size==64
                &&ops[1].size==64&&stackMemory(1))op.Type=Kind::Restore;
            else if(reg(0,ZYDIS_REGISTER_RSP)&&reg(1,ZYDIS_REGISTER_R11))op.Type=Kind::StackRestore;
        }
        else if(ins.mnemonic==ZYDIS_MNEMONIC_LEA&&ins.operand_count_visible==2
            &&reg(0,ZYDIS_REGISTER_R11)&&stackMemory(1)&&ops[1].mem.base==ZYDIS_REGISTER_RSP
            &&ops[1].mem.disp.has_displacement&&ops[1].mem.disp.value>0)op.Type=Kind::Restore;
        else if(ins.mnemonic==ZYDIS_MNEMONIC_ADD&&ins.operand_count_visible==2&&reg(0,ZYDIS_REGISTER_RSP)
            &&ops[1].type==ZYDIS_OPERAND_TYPE_IMMEDIATE&&ops[1].imm.value.s>0)op.Type=Kind::StackRestore;
        code.push_back(op);at+=ins.length;
    }
    return FindReturn(code,static_cast<std::uint32_t>(body.size()));
}
}
