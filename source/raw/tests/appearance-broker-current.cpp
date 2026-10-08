#include <Windows.h>
#include <cstring>
#include <iostream>
static unsigned char* testImage;
static HMODULE TestModule(LPCWSTR){return reinterpret_cast<HMODULE>(testImage);}
#define GetModuleHandleW TestModule
#include "../src/Loader/AppearanceEvents.cpp"
#undef GetModuleHandleW
using namespace DragonWilds::AppearanceEvents;
static unsigned ghostCalls,traceCalls;
static void Ghost(unsigned,uintptr_t,uint32_t){++ghostCalls;}
static void Trace(unsigned,uintptr_t,uint32_t){++traceCalls;}
static void Require(bool value){if(!value)throw std::runtime_error("Appearance broker lifecycle failed");}
int main() {
    constexpr size_t size=0x40000;
    testImage=static_cast<unsigned char*>(VirtualAlloc(nullptr,size,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));Require(testImage);
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(testImage);dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=0x100;
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(testImage+0x100);
    nt->Signature=IMAGE_NT_SIGNATURE;nt->FileHeader.Machine=IMAGE_FILE_MACHINE_AMD64;
    nt->FileHeader.SizeOfOptionalHeader=sizeof(IMAGE_OPTIONAL_HEADER64);nt->FileHeader.NumberOfSections=1;
    nt->OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR64_MAGIC;nt->OptionalHeader.SizeOfImage=size;
    auto* section=IMAGE_FIRST_SECTION(nt);section->VirtualAddress=0x1000;section->Misc.VirtualSize=size-0x1000;
    section->Characteristics=IMAGE_SCN_MEM_READ|IMAGE_SCN_MEM_EXECUTE;
    std::array<uintptr_t,5> expected{};
    std::array<std::array<uint8_t,16>,5> before{};
    for(size_t i=0;i<5;++i) {
        const auto& definition=PS::AppearanceSignatures::Definitions[i];
        auto code=PS::AppearanceResolver::Decode(definition.code);const size_t start=0x1000+i*0x2000;
        for(size_t t=0;t<definition.targetCount;++t) {
            const auto& target=definition.targets[t];
            const int32_t delta=static_cast<int32_t>(0x30000-start-target.offset-target.next);
            std::memcpy(code.data()+target.offset,&delta,4);
        }
        std::memcpy(testImage+start,code.data(),code.size());expected[i]=start+definition.hookOffset;
        std::memcpy(before[i].data(),testImage+expected[i],16);
    }
    auto restored=[&]{for(size_t i=0;i<5;++i)if(std::memcmp(testImage+expected[i],before[i].data(),16))return false;return true;};
    Subscribe(Consumer::Ghost,Ghost);Require(!restored());
    for(size_t i=0;i<5;++i)Require(ResolvedRva(static_cast<unsigned>(i))==expected[i]);
    Subscribe(Consumer::Trace,Trace);Notify(0,123);Require(ghostCalls==1 && traceCalls==1);
    Unsubscribe(Consumer::Trace);Notify(1,123);Require(ghostCalls==2 && traceCalls==1 && !restored());
    Subscribe(Consumer::Trace,Trace);Unsubscribe(Consumer::Ghost);Notify(2,123);Require(traceCalls==2 && ghostCalls==2);
    Unsubscribe(Consumer::Trace);Require(restored());
    ++nt->FileHeader.TimeDateStamp;Subscribe(Consumer::Ghost,Ghost);Unsubscribe(Consumer::Ghost);Require(restored());
    testImage[expected[0]]^=1;
    bool rejected=false;try{Subscribe(Consumer::Ghost,Ghost);}catch(...){rejected=true;}
    Require(rejected && Unavailable());testImage[expected[0]]^=1;
    rejected=false;try{Subscribe(Consumer::Trace,Trace);}catch(...){rejected=true;}
    Require(rejected && restored());
    VirtualFree(testImage,0,MEM_RELEASE);
    std::cout<<"PASS: all five hook relocations, shared subscription/detach, restoration, timestamp independence, tamper rejection and permanent failure latch.\n";
}
