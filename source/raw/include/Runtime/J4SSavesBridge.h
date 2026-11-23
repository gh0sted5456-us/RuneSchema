#pragma once
#include "Core/J4SSections.h"
#include <windows.h>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
// Calls the J4SSaves store through the DLL the game loaded. Nothing is loaded here: when
// J4SSaves is absent, disabled or older than 1.1 (no list/delete), the store is unavailable
// and Helpy works on the save copy alone.
namespace PS::J4SSavesBridge {
enum Status:int{Ok=0,Missing=1,Conflict=2,Invalid=3,Busy=4,Incompatible=5,StorageError=6,Unavailable=7,BufferTooSmall=8};
inline const char* Name(int status){
    switch(status){
    case Ok:return "ok";case Missing:return "missing";case Conflict:return "conflict";case Invalid:return "invalid";
    case Busy:return "busy";case Incompatible:return "incompatible";case StorageError:return "storage error";
    case BufferTooSmall:return "buffer too small";default:return "unavailable";
    }
}
using ReadFn=int(*)(const char*,const char*,int64_t*,int32_t*,void*,uint32_t,uint32_t*);
using WriteFn=int(*)(const char*,const char*,int64_t,int32_t,const void*,uint32_t,int64_t*);
using DeleteFn=int(*)(const char*,const char*,int64_t);
using ListFn=int(*)(const char*,char*,uint32_t,uint32_t*);
struct Api{ReadFn Read=nullptr;WriteFn Write=nullptr;DeleteFn Delete=nullptr;ListFn List=nullptr;
    bool Complete()const{return Read&&Write&&Delete&&List;}};
// The exact installed DLL only, never a same-named module from another mod.
inline Api Find(const std::filesystem::path& ue4ssMods){
    Api api;const auto path=ue4ssMods/L"J4SSaves"/L"dlls"/L"main.dll";
    const HMODULE module=GetModuleHandleW(path.c_str());if(!module)return api;
    api.Read=reinterpret_cast<ReadFn>(GetProcAddress(module,"J4SSavesReadV1"));
    api.Write=reinterpret_cast<WriteFn>(GetProcAddress(module,"J4SSavesWriteV1"));
    api.Delete=reinterpret_cast<DeleteFn>(GetProcAddress(module,"J4SSavesDeleteV1"));
    api.List=reinterpret_cast<ListFn>(GetProcAddress(module,"J4SSavesListV1"));
    return api;
}
struct Stored{std::string Owner;int64_t Revision=0;int Schema=0;uint32_t Size=0;};
inline int List(const Api& api,const std::string& character,std::vector<Stored>& rows){
    rows.clear();if(!api.Complete())return Unavailable;
    uint32_t size=0;int status=api.List(character.c_str(),nullptr,0,&size);
    if(status==Ok&&size==0)return Ok;
    if(status!=BufferTooSmall)return status;
    if(size>4*1024*1024)return StorageError;
    std::string text(size,'\0');status=api.List(character.c_str(),text.data(),size,&size);if(status!=Ok)return status;text.resize(size);
    const auto parsed=nlohmann::json::parse(text,nullptr,false);if(!parsed.is_array())return StorageError;
    for(const auto& row:parsed){
        if(!row.is_object()||!row.contains("owner")||!row["owner"].is_string())return StorageError;
        rows.push_back({row["owner"].get<std::string>(),row.value("revision",int64_t{}),row.value("schema",0),row.value("size",uint32_t{})});
    }
    return Ok;
}
// Reads one section as an embedded entry [schema, revision, "HEX"].
inline int Read(const Api& api,const std::string& character,const std::string& owner,nlohmann::json& entry){
    if(!api.Complete())return Unavailable;
    int64_t revision=0;int32_t schema=0;uint32_t size=0;
    int status=api.Read(character.c_str(),owner.c_str(),&revision,&schema,nullptr,0,&size);
    if(status!=BufferTooSmall&&status!=Ok)return status;
    if(size==0||size>J4SSections::MaxPayload)return StorageError;
    std::string bytes(size,'\0');status=api.Read(character.c_str(),owner.c_str(),&revision,&schema,bytes.data(),size,&size);if(status!=Ok)return status;
    static constexpr char digits[]="0123456789ABCDEF";std::string hex;hex.reserve(size*2);
    for(uint32_t i=0;i<size;++i){const auto c=static_cast<unsigned char>(bytes[i]);hex+=digits[c>>4];hex+=digits[c&15];}
    entry=nlohmann::json::array({schema,revision,hex});return Ok;
}
// Writes an embedded entry into the store at the stored revision (0 creates it).
inline int Write(const Api& api,const std::string& character,const std::string& owner,int64_t expected,const nlohmann::json& entry,int64_t& revision){
    revision=0;if(!api.Complete())return Unavailable;if(!J4SSections::ValidEntry(entry))return Invalid;
    const auto& hex=entry[2].get_ref<const std::string&>();std::string bytes(hex.size()/2,'\0');
    const auto digit=[](char c){return c<='9'?c-'0':(c|0x20)-'a'+10;};
    for(size_t i=0;i<bytes.size();++i)bytes[i]=static_cast<char>(digit(hex[i*2])*16+digit(hex[i*2+1]));
    return api.Write(character.c_str(),owner.c_str(),expected,entry[0].get<int32_t>(),bytes.data(),static_cast<uint32_t>(bytes.size()),&revision);
}
}
