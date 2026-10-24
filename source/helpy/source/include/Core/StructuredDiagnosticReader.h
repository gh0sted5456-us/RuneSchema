#pragma once
#include "Core/StructuredDiagnosticContract.h"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace PS::StructuredDiagnosticReader {
using PS::StructuredDiagnostic::Record;
struct Cursor {std::uint64_t Offset=0,Identity=0;std::string Partial;bool Started=false,Discard=false;};
struct Batch {Cursor Next;std::vector<Record> Records;std::string Error;std::size_t Invalid=0;bool Reset=false,More=false,Gap=false;};
inline std::uint64_t Identity(const std::filesystem::path& path){
#ifdef _WIN32
    const HANDLE handle=CreateFileW(path.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(handle==INVALID_HANDLE_VALUE)return 0;
    BY_HANDLE_FILE_INFORMATION info{};const bool valid=GetFileInformationByHandle(handle,&info)!=0;CloseHandle(handle);
    return valid?(std::uint64_t(info.dwVolumeSerialNumber)<<32)^((std::uint64_t(info.nFileIndexHigh)<<32)|info.nFileIndexLow):0;
#else
    (void)path;return 0; // Linux tests use size truncation; runtime uses Win32 file identity.
#endif
}
inline void Consume(Batch& batch,const char* bytes,std::size_t count){
    auto& next=batch.Next;
    for(std::size_t i=0;i<count;++i){const char c=bytes[i];
        if(c=='\n'){
            if(!next.Discard){
                if(!next.Partial.empty()&&next.Partial.back()=='\r')next.Partial.pop_back();
                if(!next.Partial.empty()){
                    if(auto record=StructuredDiagnostic::Parse(next.Partial))batch.Records.push_back(std::move(*record));
                    else ++batch.Invalid;
                }
            }
            next.Partial.clear();next.Discard=false;
        }else if(!next.Discard){
            if(next.Partial.size()+1<StructuredDiagnostic::MaxRecord)next.Partial.push_back(c);
            else{next.Partial.clear();next.Discard=true;++batch.Invalid;}
        }
    }
}
inline Batch Read(const std::filesystem::path& path,Cursor cursor){
    Batch batch;batch.Next=std::move(cursor);
    try{
        std::error_code ec;const auto size=std::filesystem::file_size(path,ec);
        if(ec){batch.Error="RSDiagLog.jsonl is not available yet.";return batch;}
        const auto identity=Identity(path);
        if(!batch.Next.Started){batch.Next.Started=true;batch.Next.Identity=identity;}
        else if((identity&&batch.Next.Identity&&identity!=batch.Next.Identity)||size<batch.Next.Offset){
            batch.Reset=true;
            // Complete any unread tail of the previous rotated file before
            // switching to the current active file. Never guess if identities
            // do not match; expose a possible evidence gap instead.
            const auto previous=std::filesystem::path(path.wstring()+L".1");
            if(batch.Next.Identity&&Identity(previous)==batch.Next.Identity){
                std::error_code prevError;const auto prevSize=std::filesystem::file_size(previous,prevError);
                if(!prevError&&prevSize>batch.Next.Offset){
                    std::ifstream prior(previous,std::ios::binary);
                    prior.seekg(static_cast<std::streamoff>(batch.Next.Offset));
                    std::string buffer(static_cast<std::size_t>(std::min<std::uintmax_t>(256*1024,prevSize-batch.Next.Offset)),'\0');
                    prior.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));
                    Consume(batch,buffer.data(),static_cast<std::size_t>(prior.gcount()));
                    batch.Next.Offset+=static_cast<std::uint64_t>(prior.gcount());
                    if(batch.Next.Offset<prevSize){batch.More=true;return batch;}
                }
            }else batch.Gap=true;
            batch.Next={};batch.Next.Started=true;batch.Next.Identity=identity;
        }
        if(size<=batch.Next.Offset)return batch;
        std::ifstream file(path,std::ios::binary);
        if(!file){batch.Error="Could not open structured diagnostic log for reading.";return batch;}
        file.seekg(static_cast<std::streamoff>(batch.Next.Offset));
        const auto amount=static_cast<std::size_t>(std::min<std::uintmax_t>(256*1024,size-batch.Next.Offset));
        std::string buffer(amount,'\0');file.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));
        const auto received=static_cast<std::size_t>(file.gcount());
        Consume(batch,buffer.data(),received);batch.Next.Offset+=received;batch.More=batch.Next.Offset<size;
    }catch(...){batch.Error="Structured diagnostic log read failed.";}
    return batch;
}
}
