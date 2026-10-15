#include "Core/NativeFunctionDiscovery.h"
#include "Runtime/NativeHookPatterns.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#ifdef _WIN32
#include "Runtime/NativeExecutableDiscovery.h"
#endif
namespace Discover=PS::NativeFunctionDiscovery;
namespace Patterns=PS::NativeHookPatterns;
static unsigned Checks=0;
static void Require(bool value,const char* why){++Checks;if(!value)throw std::runtime_error(why);}
static void Synthetic() {
    for(const auto pattern:{Patterns::SteamWearableBuilder,Patterns::SteamWearableEvaluator,Patterns::GamePassWearableBuilder,
        Patterns::GamePassWearableEvaluator,Patterns::GamePassSpudLoad,Patterns::GamePassSpudDeserialize}) {
        const auto parsed=Discover::Parse(pattern);std::vector<uint8_t> bytes(parsed.size()+32,0xcc);
        for(size_t i=0;i<parsed.size();++i)bytes[16+i]=parsed[i]<0?0x79:static_cast<uint8_t>(parsed[i]);
        std::vector<Discover::Region> regions{{0x1000,bytes}};const std::vector<uint32_t> entries{0x1010};
        Require(Discover::FindUnique(regions,entries,pattern)==0x1010,"unique masked body not found at function entry");
        Require(!Discover::FindUnique(regions,std::vector<uint32_t>{0x100f},pattern),"middle-of-function match accepted");
        Require(!Discover::FindUnique({},entries,pattern),"non-executable data accepted");
        regions.push_back({0x2000,bytes});
        Require(!Discover::FindUnique(regions,std::vector<uint32_t>{0x1010,0x2010},pattern),"ambiguous match accepted");
        regions.pop_back();bytes[16+parsed.size()-1]^=1;
        Require(!Discover::FindUnique(regions,entries,pattern),"changed argument/body contract accepted");
    }
    bool rejected=false;try{Discover::Parse("?? 11");}catch(...){rejected=true;}
    Require(rejected,"short/wildcard-anchored pattern accepted");
}
#ifdef _WIN32
static void AuditImage(const char* path,bool gamePass,bool rawPe) {
    std::ifstream file(path,std::ios::binary);Require(file.good(),"native image fixture unavailable");
    std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file),{}};
    if(rawPe) {
        Require(bytes.size()>4096,"PE fixture too short");
        const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
        Require(dos->e_lfanew>0&&dos->e_lfanew<4096,"PE fixture header invalid");
        const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(bytes.data()+dos->e_lfanew);
        std::vector<uint8_t> mapped(nt->OptionalHeader.SizeOfImage);
        std::copy_n(bytes.begin(),nt->OptionalHeader.SizeOfHeaders,mapped.begin());
        const auto* sections=IMAGE_FIRST_SECTION(nt);
        for(size_t i=0;i<nt->FileHeader.NumberOfSections;++i){const auto& s=sections[i];
            Require(s.PointerToRawData<=bytes.size()&&s.SizeOfRawData<=bytes.size()-s.PointerToRawData,"PE section outside file");
            Require(s.VirtualAddress<=mapped.size()&&s.SizeOfRawData<=mapped.size()-s.VirtualAddress,"PE section outside image");
            std::copy_n(bytes.data()+s.PointerToRawData,s.SizeOfRawData,mapped.data()+s.VirtualAddress);
        }
        bytes=std::move(mapped);
    }
    const auto check=[&](std::string_view pattern,uint32_t expected){
        auto* found=PS::NativeExecutableDiscovery::ResolveImage(bytes.data(),pattern);
        Require(found&&static_cast<uint8_t*>(found)-bytes.data()==expected,"real image function discovery differs from disassembly");
        std::cout<<"Verified image target RVA=0x"<<std::hex<<expected<<std::dec<<'\n';
    };
    if(gamePass){check(Patterns::GamePassWearableBuilder,0x6cd0380);check(Patterns::GamePassWearableEvaluator,0x6ea5020);
        check(Patterns::GamePassSpudLoad,0x654c8b0);check(Patterns::GamePassSpudDeserialize,0x6530c20);}
    else {check(Patterns::SteamWearableBuilder,0x6396930);check(Patterns::SteamWearableEvaluator,0x6599e10);}
}
#endif
int main(int argc,char** argv) {
    try {
        Synthetic();
#ifdef _WIN32
        if(argc==3)AuditImage(argv[2],std::string_view(argv[1])=="--gamepass",std::string_view(argv[1])=="--steam");
#endif
        std::cout<<Checks<<" native function discovery checks passed.\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
