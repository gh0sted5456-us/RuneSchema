#pragma once
#include "Utility/Logging.h"
#include "Core/UE4SSBuildIdentity.h"
#include "Runtime/NativeExecutableDiscovery.h"
#include <Windows.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <sstream>
#include <nlohmann/json.hpp>

namespace PS::UE4SSCompatibility {
    inline bool HasHashTableApi()
    {
        const auto host = GetModuleHandleW(L"UE4SS.dll");
        return host && GetProcAddress(host, "?ClearGlobalObjectCache@Unreal@RC@@YAXXZ") != nullptr;
    }

    inline bool LoadedContainsCommit(HMODULE module,std::string_view commit) {
        const auto* base=reinterpret_cast<const std::uint8_t*>(module);
        if(!NativeExecutableDiscovery::Readable(base,sizeof(IMAGE_DOS_HEADER)))return false;
        const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<=0||dos->e_lfanew>4096)return false;
        const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
        if(!NativeExecutableDiscovery::Readable(nt,sizeof(*nt))||nt->Signature!=IMAGE_NT_SIGNATURE
            ||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||nt->FileHeader.NumberOfSections>96
            ||nt->OptionalHeader.SizeOfImage>128*1024*1024)return false;
        const auto* sections=IMAGE_FIRST_SECTION(nt);
        if(!NativeExecutableDiscovery::Readable(sections,nt->FileHeader.NumberOfSections*sizeof(*sections)))return false;
        const std::wstring wide(commit.begin(),commit.end());
        for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
            const auto& section=sections[i];const auto size=section.Misc.VirtualSize;
            if(!(section.Characteristics&IMAGE_SCN_MEM_READ)||(section.Characteristics&(IMAGE_SCN_MEM_WRITE|IMAGE_SCN_MEM_EXECUTE)))continue;
            if(section.VirtualAddress>=nt->OptionalHeader.SizeOfImage||size>nt->OptionalHeader.SizeOfImage-section.VirtualAddress)continue;
            const auto* start=base+section.VirtualAddress;if(!NativeExecutableDiscovery::Readable(start,size))continue;
            const auto narrow=std::string_view(reinterpret_cast<const char*>(start),size);
            if(narrow.find(commit)!=narrow.npos||narrow.find(std::string_view(reinterpret_cast<const char*>(wide.data()),wide.size()*sizeof(wchar_t)))!=narrow.npos)return true;
        }
        return false;
    }
    inline const nlohmann::json& Snapshot() {
        static const nlohmann::json value=[] {
            nlohmann::json result={{"Build","Not reported"},{"Version",""},{"Commit",""},{"Source","Unavailable"},
                {"Capabilities",HasHashTableApi()?"Hash-table API available":"Classic object-array fallback"},
                {"Policy","Capabilities and exported ABI; no build-number ceiling"}};
            const auto host=GetModuleHandleW(L"UE4SS.dll");if(!host)return result;
            wchar_t path[32768]{};const auto length=GetModuleFileNameW(host,path,_countof(path));
            if(!length||length>=_countof(path))return result;
            // Read only this host's fresh startup banner, not a log from the
            // other storefront. Verify its commit is embedded in the loaded DLL.
            const auto log=std::filesystem::path(path).parent_path()/"UE4SS.log";
            WIN32_FILE_ATTRIBUTE_DATA info{};FILETIME created{},exit{},kernel{},user{};
            if(!GetFileAttributesExW(log.c_str(),GetFileExInfoStandard,&info)
                ||!GetProcessTimes(GetCurrentProcess(),&created,&exit,&kernel,&user)
                ||CompareFileTime(&info.ftLastWriteTime,&created)<0)return result;
            std::ifstream file(log,std::ios::binary);std::string text(16384,'\0');
            file.read(text.data(),text.size());text.resize(static_cast<size_t>(file.gcount()));
            const auto identity=UE4SSBuildIdentity::ParseStartup(text);
            if(!identity||!LoadedContainsCommit(host,identity->Commit))return result;
            result["Version"]=identity->Version;result["Commit"]=identity->Commit;
            result["Build"]=identity->Version+" · "+identity->Commit.substr(0,8);
            result["Source"]="Current startup log; commit verified in loaded UE4SS.dll";
            return result;
        }();
        return value;
    }
    inline const char* DetectedBuild(){return Snapshot().at("Build").get_ref<const std::string&>().c_str();}

    inline void Report()
    {
        static std::atomic_flag reported = ATOMIC_FLAG_INIT;
        if (reported.test_and_set()) return;

        const auto host = GetModuleHandleW(L"UE4SS.dll");
        if (!host)
        {
            PS::Log<RC::LogLevel::Warning>(STR("UE4SS compatibility: host module could not be inspected.\n"));
            return;
        }

        // The hash-table object cache was added after the 946 line. Detect a
        // capability instead of binding RuneSchema behavior to a file version.
        const bool hashTables = HasHashTableApi();
        PS::ConsolidatedLog<RC::LogLevel::Normal>(
            STR("[RuneSchema] UE4SS compatibility: host={} | weak-serial=local-safe | lifecycle=InitGameState\n"),
            hashTables ? STR("hash-table line") : STR("classic line"));
    }
}
