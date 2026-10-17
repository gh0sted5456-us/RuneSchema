#pragma once
#include <Windows.h>
#include "Core/NativeFunctionDiscovery.h"
namespace PS::NativeExecutableDiscovery {
inline bool Readable(const void* value,size_t length) {
    auto address=reinterpret_cast<uintptr_t>(value);
    if(!address||length>SIZE_MAX-address)return false;
    const auto end=address+length;
    while(address<end) {
        MEMORY_BASIC_INFORMATION page{};
        if(!VirtualQuery(reinterpret_cast<void*>(address),&page,sizeof(page))||page.State!=MEM_COMMIT
            ||(page.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
        const auto next=reinterpret_cast<uintptr_t>(page.BaseAddress)+page.RegionSize;
        if(next<=address)return false;address=next;
    }
    return true;
}
inline void* ResolveImage(const std::uint8_t* base,std::string_view signature) noexcept {
    try {
        if(!Readable(base,sizeof(IMAGE_DOS_HEADER)))return nullptr;
        const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<=0||dos->e_lfanew>4096)return nullptr;
        const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
        if(!Readable(nt,sizeof(*nt))||nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64
            ||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||nt->OptionalHeader.SizeOfImage>512*1024*1024
            ||nt->FileHeader.NumberOfSections>96||nt->OptionalHeader.NumberOfRvaAndSizes<=IMAGE_DIRECTORY_ENTRY_EXCEPTION)return nullptr;
        const auto imageSize=nt->OptionalHeader.SizeOfImage;
        const auto within=[&](std::uint32_t rva,size_t count){return rva<imageSize&&count<=imageSize-rva&&Readable(base+rva,count);};
        const auto& exception=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
        if(exception.Size%sizeof(IMAGE_RUNTIME_FUNCTION_ENTRY)||!within(exception.VirtualAddress,exception.Size))return nullptr;
        const auto* table=reinterpret_cast<const IMAGE_RUNTIME_FUNCTION_ENTRY*>(base+exception.VirtualAddress);
        std::vector<std::uint32_t> entries;
        for(size_t i=0;i<exception.Size/sizeof(*table);++i) {
            if(table[i].BeginAddress>=table[i].EndAddress||table[i].EndAddress>imageSize)return nullptr;
            entries.push_back(table[i].BeginAddress);
        }
        if(!std::is_sorted(entries.begin(),entries.end()))return nullptr;
        const auto* sections=IMAGE_FIRST_SECTION(nt);
        if(!Readable(sections,nt->FileHeader.NumberOfSections*sizeof(*sections)))return nullptr;
        std::vector<NativeFunctionDiscovery::Region> regions;
        for(size_t i=0;i<nt->FileHeader.NumberOfSections;++i) {
            const auto& section=sections[i];
            if(!(section.Characteristics&IMAGE_SCN_MEM_EXECUTE)||(section.Characteristics&IMAGE_SCN_MEM_WRITE))continue;
            if(!within(section.VirtualAddress,section.Misc.VirtualSize))return nullptr;
            regions.push_back({section.VirtualAddress,{base+section.VirtualAddress,section.Misc.VirtualSize}});
        }
        const auto found=NativeFunctionDiscovery::FindUnique(regions,entries,signature);
        return found?const_cast<std::uint8_t*>(base+*found):nullptr;
    } catch(...) {return nullptr;}
}
inline void* Resolve(std::string_view signature) noexcept {
    return ResolveImage(reinterpret_cast<const std::uint8_t*>(GetModuleHandleW(nullptr)),signature);
}
}
