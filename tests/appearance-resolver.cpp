#include "Generator/AppearanceResolver.h"
#include "Generator/NativeCallResolver.h"
#include "Loader/JournalNativeContract.h"
#include "Loader/NativeShopContract.h"
#include "Loader/JournalPersistenceContract.h"
#include "Loader/JournalJsonFieldContract.h"
#include <iostream>
#include <fstream>
#include <Windows.h>
using namespace PS::AppearanceResolver;
static void Require(bool value){if(!value)throw std::runtime_error("Appearance resolver test failed");}
template<class F> void Reject(F&& call){bool failed=false;try{call();}catch(const std::exception&){failed=true;}Require(failed);}
int main(int argc,char** argv) {
    try {
    std::cout<<std::unitbuf;
    std::vector<uint8_t> image(256,0);
    Section section{0,image.size(),true};
    PS::AppearanceSignatures::Target target{8,4,true};
    PS::AppearanceSignatures::Signature signature{"test",2,"010203040506070800000000c3","ffffffffffffffff00000000ff",&target,1};
    const auto bytes=Decode(signature.code);
    std::copy(bytes.begin(),bytes.end(),image.begin()+32);
    int32_t delta=100-44;std::memcpy(image.data()+40,&delta,4);
    Require(Resolve(image,{&section,1},signature)==34);
    image[35]^=1;Reject([&]{Resolve(image,{&section,1},signature);});image[35]^=1;
    std::copy(image.begin()+32,image.begin()+45,image.begin()+64);
    Reject([&]{Resolve(image,{&section,1},signature);});std::fill(image.begin()+64,image.begin()+77,0);
    delta=-1000;std::memcpy(image.data()+40,&delta,4);Reject([&]{Resolve(image,{&section,1},signature);});
    section.size=300;Reject([&]{Resolve(image,{&section,1},signature);});
    const auto& shop=DragonWilds::NativeShopContract::Definition;
    const auto shopBytes=Decode(shop.code);
    image.assign(4096,0);section={0,image.size(),true};
    const auto placeShop=[&](size_t offset) {
        std::copy(shopBytes.begin(),shopBytes.end(),image.begin()+offset);
        for(size_t i=0;i<shop.targetCount;++i) {
            const auto& target=shop.targets[i];
            int32_t displacement=static_cast<int32_t>(3500-offset-target.offset-target.next);
            std::memcpy(image.data()+offset+target.offset,&displacement,4);
        }
    };
    placeShop(128);Require(Resolve(image,{&section,1},shop)==128);
    image[128]^=1;Reject([&]{Resolve(image,{&section,1},shop);});image[128]^=1;
    placeShop(1200);Reject([&]{Resolve(image,{&section,1},shop);});
    {
        image.assign(256,0);section={0,image.size(),true};
        const PS::AppearanceSignatures::Target callTarget{9,4,true};
        const PS::AppearanceSignatures::Signature caller{"caller",0,"0102030405060708e800000000c3","ffffffffffffffffff00000000ff",&callTarget,1};
        const PS::AppearanceSignatures::Signature callee{"callee",0,"1122334455667788c3","ffffffffffffffffff",nullptr,0};
        const auto callCode=Decode(caller.code),targetCode=Decode(callee.code);
        std::copy(callCode.begin(),callCode.end(),image.begin()+32);
        std::copy(targetCode.begin(),targetCode.end(),image.begin()+128);
        std::copy(targetCode.begin(),targetCode.end(),image.begin()+160);
        int32_t relative=128-(32+8+5);std::memcpy(image.data()+41,&relative,4);
        Reject([&]{Resolve(image,{&section,1},callee);});
        Require(ResolveCalled(image,{&section,1},caller,8,callee)==128);
        image[128]^=1;Reject([&]{ResolveCalled(image,{&section,1},caller,8,callee);});image[128]^=1;
        Reject([&]{ResolveCalled(image,{&section,1},caller,caller.code.size(),callee);});
        Reject([&]{ResolveCalled(image,{&section,1},caller,7,callee);});
        relative=1000;std::memcpy(image.data()+41,&relative,4);
        Reject([&]{ResolveCalled(image,{&section,1},caller,8,callee);});
    }
    for(const auto& definition:DragonWilds::JournalPersistenceContract::Definitions) {
        const auto code=Decode(definition.code);
        image.assign(65536,0);section={0,image.size(),true};
        const auto place=[&](size_t offset) {
            std::copy(code.begin(),code.end(),image.begin()+offset);
            for(size_t i=0;i<definition.targetCount;++i) {
                const auto& target=definition.targets[i];
                const int32_t displacement=static_cast<int32_t>(60000-offset-target.offset-target.next);
                std::memcpy(image.data()+offset+target.offset,&displacement,4);
            }
        };
        place(128);Require(Resolve(image,{&section,1},definition)==128);
        image[128]^=1;Reject([&]{Resolve(image,{&section,1},definition);});image[128]^=1;
        place(8192);Reject([&]{Resolve(image,{&section,1},definition);});
    }
    if(argc==2) {
        std::ifstream file(argv[1],std::ios::binary);Require(file.good());
        std::vector<uint8_t> raw((std::istreambuf_iterator<char>(file)),{});
        Require(raw.size()>sizeof(IMAGE_DOS_HEADER));
        auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(raw.data());
        Require(dos->e_magic==IMAGE_DOS_SIGNATURE && dos->e_lfanew>0 && static_cast<size_t>(dos->e_lfanew)+sizeof(IMAGE_NT_HEADERS64)<raw.size());
        auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(raw.data()+dos->e_lfanew);
        Require(nt->Signature==IMAGE_NT_SIGNATURE);
        image.assign(nt->OptionalHeader.SizeOfImage,0);
        std::vector<Section> sections;
        const auto* s=IMAGE_FIRST_SECTION(nt);
        for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
            Require(static_cast<size_t>(s[i].PointerToRawData)+s[i].SizeOfRawData<=raw.size());
            Require(static_cast<size_t>(s[i].VirtualAddress)+s[i].SizeOfRawData<=image.size());
            std::copy_n(raw.data()+s[i].PointerToRawData,s[i].SizeOfRawData,image.data()+s[i].VirtualAddress);
            if((s[i].Characteristics&IMAGE_SCN_MEM_READ) && !(s[i].Characteristics&IMAGE_SCN_MEM_DISCARDABLE))
                sections.push_back({s[i].VirtualAddress,s[i].Misc.VirtualSize,(s[i].Characteristics&IMAGE_SCN_MEM_EXECUTE)!=0});
        }
        for(const auto& definition:PS::AppearanceSignatures::Definitions)
            std::cout<<definition.name<<" 0x"<<std::hex<<Resolve(image,sections,definition)<<'\n';
        for(const auto& definition:DragonWilds::JournalNativeContract::Definitions)
            std::cout<<definition.name<<" 0x"<<std::hex<<Resolve(image,sections,definition)<<'\n';
        std::cout<<shop.name<<" 0x"<<std::hex<<Resolve(image,sections,shop)<<'\n';
        for(size_t i=0;i<std::size(DragonWilds::JournalPersistenceContract::Definitions);++i) {
            const auto& definition=DragonWilds::JournalPersistenceContract::Definitions[i];
            const auto address=i==7?ResolveCalled(image,sections,DragonWilds::JournalPersistenceContract::Definitions[1],0x1ed,definition)
                :Resolve(image,sections,definition);
            std::cout<<definition.name<<" 0x"<<std::hex<<address<<'\n';
        }
        for(size_t i=0;i<2;++i) {
            const auto& definition=DragonWilds::JournalJsonFieldContract::Definitions[i];
            std::cout<<definition.name<<" 0x"<<std::hex<<ResolveCalled(image,sections,
                DragonWilds::JournalPersistenceContract::Definitions[2],i?0x4a:0x3b,definition)<<'\n';
        }
    }
    std::cout<<"PASS: relocation, mutation, ambiguity, out-of-range target and section rejection.\n";
    return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
