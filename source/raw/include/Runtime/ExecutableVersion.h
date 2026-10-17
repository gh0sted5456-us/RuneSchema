#pragma once
#include <windows.h>
#include <algorithm>
#include <string>
#include <vector>

namespace PS::ExecutableVersion {
// Data-only Windows version resource, cached once. No game function calls,
// signature scans, package-version guesses or reading another session's log.
inline const std::string& Current() {
    static const std::string value=[]()->std::string {
        wchar_t path[32768]{};const auto length=GetModuleFileNameW(nullptr,path,_countof(path));
        if(!length||length>=_countof(path))return {};
        DWORD unused=0;const auto size=GetFileVersionInfoSizeW(path,&unused);
        if(!size||size>4*1024*1024)return {};
        std::vector<unsigned char> bytes(size);
        if(!GetFileVersionInfoW(path,0,size,bytes.data()))return {};
        struct Translation {WORD Language,CodePage;};Translation* translations=nullptr;UINT count=0;
        if(!VerQueryValueW(bytes.data(),L"\\VarFileInfo\\Translation",reinterpret_cast<void**>(&translations),&count))return {};
        for(size_t index=0;index<count/sizeof(Translation);++index) {
            wchar_t query[96]{};swprintf_s(query,L"\\StringFileInfo\\%04x%04x\\ProductVersion",translations[index].Language,translations[index].CodePage);
            wchar_t* text=nullptr;UINT characters=0;
            if(!VerQueryValueW(bytes.data(),query,reinterpret_cast<void**>(&text),&characters)||!text||characters<2||characters>1024)continue;
            const auto end=std::find(text,text+characters,L'\0');
            if(end==text||end==text+characters)continue;
            const auto chars=static_cast<int>(end-text);const auto required=WideCharToMultiByte(CP_UTF8,0,text,chars,nullptr,0,nullptr,nullptr);
            if(required<=0)continue;std::string result(required,'\0');
            if(WideCharToMultiByte(CP_UTF8,0,text,chars,result.data(),required,nullptr,nullptr)==required)return result;
        }
        return {};
    }();
    return value;
}
}
