#include <Windows.h>
#include "Utility/InlineHook.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
static int Replacement(){return 7;}
static void Check(bool value){if(!value)throw std::runtime_error("Inline hook lifecycle failure");}
int main(){
    auto* page=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    Check(page!=nullptr);std::memset(page,0x90,4096);
    const unsigned char code[]{0xb8,42,0,0,0,0xc3};std::memcpy(page,code,sizeof(code));
    FlushInstructionCache(GetCurrentProcess(),page,4096);
    using Function=int(*)();auto function=reinterpret_cast<Function>(page);
    SafetyHookInline slot;
    Check(!PS::InstallInlineHook(slot,nullptr,reinterpret_cast<void*>(Replacement)) && !slot);
    Check(function()==42);
    Check(PS::InstallInlineHook(slot,page,reinterpret_cast<void*>(Replacement)));
    Check(function()==7 && slot.call<int>()==42);
    Check(!PS::InstallInlineHook(slot,page,reinterpret_cast<void*>(Replacement)));
    slot={};Check(function()==42 && std::memcmp(page,code,sizeof(code))==0);
    VirtualFree(page,0,MEM_RELEASE);
    std::cout<<"PASS: production inline installer rejects missing/duplicate targets, dispatches, calls original and restores.\n";
}
