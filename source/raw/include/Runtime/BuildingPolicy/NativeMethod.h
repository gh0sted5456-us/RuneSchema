#pragma once
#include "Reflection.h"
#include "BindingTargets.h"
#include "NativeExecTarget.h"
#include <Windows.h>
#include <cstring>

namespace PS::Buildings {
// Resolve the implementation that feeds the exec thunk's result, before array cleanup calls.
inline uintptr_t NativeMethod(UFunction* function,unsigned resultBytes){
    const auto start=reinterpret_cast<uintptr_t>(function->GetFuncPtr());
    const auto image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(image+dos->e_lfanew);
    const auto code=[&](uintptr_t address,size_t count,bool executable=true){
        if(address<image||address-image>=nt->OptionalHeader.SizeOfImage||count>nt->OptionalHeader.SizeOfImage-(address-image))return false;
        MEMORY_BASIC_INFORMATION page{};return VirtualQuery(reinterpret_cast<void*>(address),&page,sizeof(page))&&page.State==MEM_COMMIT
            &&!(page.Protect&(PAGE_NOACCESS|PAGE_GUARD))&&(page.Protect&(executable?(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY):(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))
            &&count<=page.RegionSize-(address-reinterpret_cast<uintptr_t>(page.BaseAddress));
    };
    DWORD64 owner=0;const auto* bounds=RtlLookupFunctionEntry(start,&owner,nullptr);
    size_t limit=96;
    if(bounds){
        if(owner!=image||image+bounds->BeginAddress!=start)throw std::runtime_error("Native exec function boundary changed");
        const NativeUnwindPart root{bounds->BeginAddress,bounds->EndAddress,bounds->UnwindData};
        const auto end=NativeUnwindEnd(root,[&](uintptr_t end)->std::optional<NativeUnwindChain>{
            if(end>=nt->OptionalHeader.SizeOfImage)return {};
            DWORD64 nextOwner=0;const auto* next=RtlLookupFunctionEntry(image+end,&nextOwner,nullptr);
            if(!next||nextOwner!=image||next->BeginAddress!=end)return {};
            const auto info=image+next->UnwindData;if(!code(info,4,false))throw std::runtime_error("Native chained unwind metadata is unreadable");
            const auto* bytes=reinterpret_cast<const unsigned char*>(info);const auto flags=bytes[0]>>3;
            if(!(flags&UNW_FLAG_CHAININFO))return {};
            if((bytes[0]&7)!=1||flags!=UNW_FLAG_CHAININFO)throw std::runtime_error("Native chained unwind format changed");
            const auto offset=(4+size_t(bytes[2])*2+3)&~size_t(3);
            if(!code(info,offset+sizeof(RUNTIME_FUNCTION),false))throw std::runtime_error("Native chained unwind metadata exceeds its bounds");
            RUNTIME_FUNCTION parent{};std::memcpy(&parent,bytes+offset,sizeof(parent));
            return NativeUnwindChain{{next->BeginAddress,next->EndAddress,next->UnwindData},{parent.BeginAddress,parent.EndAddress,parent.UnwindData}};
        });
        limit=end-root.Begin;
    }
    else if(resultBytes)throw std::runtime_error("Native result exec has no unwind boundary");
    if(limit>2048||!code(start,limit))throw std::runtime_error("Native exec bounds are unavailable");
    const auto found=NativeExecTarget(start,{reinterpret_cast<const unsigned char*>(start),limit},resultBytes);
    DWORD64 targetOwner=0;const auto* implementation=found?RtlLookupFunctionEntry(found,&targetOwner,nullptr):nullptr;
    if(!code(found,16)||!implementation||targetOwner!=image||image+implementation->BeginAddress!=found)throw std::runtime_error("Native implementation return contract changed");
    return found;
}
}
