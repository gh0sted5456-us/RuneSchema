#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <array>
#include <atomic>
#include <stdexcept>
#include <string>
#include <string_view>
#include <cstdint>
#include <climits>
#pragma comment(lib,"bcrypt.lib")
namespace PS::CacheBundleIO {
namespace fs=std::filesystem;
inline bool Digest(const std::string& value){return value.size()==64&&value.find_first_not_of("0123456789abcdef")==value.npos;}
inline void Safe(const fs::path& path){
    auto text=path.wstring();if(text.starts_with(L"\\\\?\\UNC\\"))text=L"\\\\"+text.substr(8);else if(text.starts_with(L"\\\\?\\"))text=text.substr(4);
    auto value=fs::absolute(text).lexically_normal();for(;;){
        auto native=value.wstring();native=native.starts_with(L"\\\\")?L"\\\\?\\UNC\\"+native.substr(2):L"\\\\?\\"+native;
        const auto attributes=GetFileAttributesW(native.c_str());
        if(attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("[RS-CACHE-009] Bundle cache folders cannot pass through links.");
        if(attributes==INVALID_FILE_ATTRIBUTES){const auto error=GetLastError();if(error!=ERROR_FILE_NOT_FOUND&&error!=ERROR_PATH_NOT_FOUND)throw std::runtime_error("[RS-CACHE-009] Bundle path cannot be inspected.");}
        const auto parent=value.parent_path();if(parent.empty()||parent==value)break;value=parent;
    }
}
inline std::string Hash(std::string_view bytes){
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};
    struct Cleanup{BCRYPT_ALG_HANDLE& A;BCRYPT_HASH_HANDLE& H;~Cleanup(){if(H)BCryptDestroyHash(H);if(A)BCryptCloseAlgorithmProvider(A,0);}}cleanup{algorithm,hash};
    std::array<unsigned char,32> digest{};
    if(bytes.size()>ULONG_MAX||BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0||BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)<0||
       BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<char*>(bytes.data())),static_cast<ULONG>(bytes.size()),0)<0||BCryptFinishHash(hash,digest.data(),digest.size(),0)<0)
        throw std::runtime_error("[RS-CACHE-004] Bundle SHA-256 failed.");
    constexpr char digits[]="0123456789abcdef";std::string output;for(auto c:digest){output+=digits[c>>4];output+=digits[c&15];}return output;
}
inline fs::path NativePath(const fs::path& path){
    const auto text=fs::absolute(path).lexically_normal().wstring();if(text.starts_with(L"\\\\?\\"))return path;
    return text.starts_with(L"\\\\")?fs::path(L"\\\\?\\UNC\\"+text.substr(2)):fs::path(L"\\\\?\\"+text);
}
inline std::string Read(const fs::path& logical,size_t maximum){
    const auto path=NativePath(logical);
    Safe(path);const auto size=fs::file_size(path);if(size>maximum)throw std::runtime_error("[RS-CACHE-005] Bundle metadata exceeds its limit.");
    std::string bytes(size,'\0');std::ifstream input(path,std::ios::binary);input.read(bytes.data(),bytes.size());if(!input||input.peek()!=std::char_traits<char>::eof())throw std::runtime_error("[RS-CACHE-002] Bundle metadata changed while reading.");return bytes;
}
inline void Publish(const fs::path& logical,std::string_view bytes){
    const auto target=NativePath(logical);
    Safe(target);fs::create_directories(target.parent_path());Safe(target);
    static std::atomic<uint64_t> sequence{0};const auto temporary=target.wstring()+L".bundle-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(++sequence)+L".tmp";
    try{Safe(temporary);{
        const auto handle=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(handle==INVALID_HANDLE_VALUE)throw std::runtime_error("[RS-CACHE-009] Cannot stage bundle metadata.");
        DWORD written=0;const bool ok=bytes.size()<=MAXDWORD&&WriteFile(handle,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)&&written==bytes.size()&&FlushFileBuffers(handle);CloseHandle(handle);
        if(!ok)throw std::runtime_error("[RS-CACHE-009] Bundle metadata write failed.");}
        Safe(target);
        if(!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("[RS-CACHE-009] Bundle metadata commit failed.");
    }catch(...){std::error_code error;fs::remove(temporary,error);throw;}
}
}

