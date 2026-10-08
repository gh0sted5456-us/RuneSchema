#include <Windows.h>
#include <cstring>
#include <stdexcept>
#include <iostream>
static unsigned char* testImage;
static HMODULE TestModule(LPCWSTR) { return reinterpret_cast<HMODULE>(testImage); }
// Exercise the production broker against a private, never-executed image.
#define GetModuleHandleW TestModule
#include "../src/Loader/AppearanceEvents.cpp"
#undef GetModuleHandleW
using namespace PS::AppearanceTraceContract;
using namespace DragonWilds::AppearanceEvents;
static unsigned ghostCalls,traceCalls;
static void Ghost(unsigned,uintptr_t,uint32_t) { ++ghostCalls; }
static void Trace(unsigned,uintptr_t,uint32_t) { ++traceCalls; }
static void Require(bool condition) { if(!condition)throw std::runtime_error("Shared appearance lifecycle failed"); }
static bool Original() {
    for(const auto& site:Sites)
        if(std::memcmp(testImage+site.rva,site.bytes.data(),site.bytes.size()))return false;
    return true;
}
int main() {
    testImage=static_cast<unsigned char*>(VirtualAlloc(nullptr,ImageSize,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    Require(testImage!=nullptr);
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(testImage);dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=0x100;
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(testImage+dos->e_lfanew);
    nt->Signature=IMAGE_NT_SIGNATURE;nt->FileHeader.Machine=IMAGE_FILE_MACHINE_AMD64;
    nt->FileHeader.TimeDateStamp=Timestamp;nt->OptionalHeader.SizeOfImage=ImageSize;
    for(const auto& site:Sites)std::memcpy(testImage+site.rva,site.bytes.data(),site.bytes.size());
    Subscribe(Consumer::Ghost,Ghost);Require(!Original());
    Subscribe(Consumer::Trace,Trace);Notify(0,123);Require(ghostCalls==1 && traceCalls==1);
    Unsubscribe(Consumer::Trace);Require(!Original());Notify(1,123);Require(ghostCalls==2 && traceCalls==1);
    Subscribe(Consumer::Trace,Trace);Unsubscribe(Consumer::Ghost);Require(!Original());
    Notify(2,123);Require(ghostCalls==2 && traceCalls==2);
    Unsubscribe(Consumer::Trace);Require(Original());
    ++nt->FileHeader.TimeDateStamp;
    bool rejected=false;try { Subscribe(Consumer::Ghost,Ghost); }catch(...) { rejected=true; }
    Require(rejected && Original());--nt->FileHeader.TimeDateStamp;
    testImage[Sites[4].rva]^=1;
    rejected=false;try { Subscribe(Consumer::Trace,Trace); }catch(...) { rejected=true; }
    Require(rejected);testImage[Sites[4].rva]^=1;Require(Original());
    Unsubscribe(Consumer::Ghost);Unsubscribe(Consumer::Trace);
    VirtualFree(testImage,0,MEM_RELEASE);
    std::cout<<"PASS: production shared broker coexistence, independent detach, restoration and mismatch rejection.\n";
}
