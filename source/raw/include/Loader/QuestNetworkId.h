#pragma once
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <span>

namespace DragonWilds::QuestRegistry {
struct NetworkIdField { std::string Name,Type; int Size,Offset,Dim; };
inline void ValidateNetworkIdWrapper(std::string_view path,int size,int dim,std::span<const NetworkIdField> fields) {
    if(path!="/Script/Dominion.DominionDataAssetNetId" || size!=2 || dim!=1 || fields.size()!=1
        || fields[0].Name!="NetId" || fields[0].Type!="UInt16Property"
        || fields[0].Size!=2 || fields[0].Offset!=0 || fields[0].Dim!=1)
        throw std::runtime_error("Quest network ID wrapper differs from captured DominionDataAssetNetId.NetId uint16 layout: "+std::string(path));
}
// Match actual reflected class names and storage widths, not a cached UE4SS cast.
class NetworkIdLayout {
    enum class Kind { U16, I16, U32, I32 };
    Kind kind;
    template<class T> static T Load(const void* address) {T value;std::memcpy(&value,address,sizeof(value));return value;}
    template<class T> static void Store(void* address,uint16_t value) {const auto typed=static_cast<T>(value);std::memcpy(address,&typed,sizeof(typed));}
public:
    NetworkIdLayout(std::string_view type,int size,int dim) {
        if(dim==1 && type=="UInt16Property" && size==2)kind=Kind::U16;
        else if(dim==1 && type=="Int16Property" && size==2)kind=Kind::I16;
        else if(dim==1 && type=="UInt32Property" && size==4)kind=Kind::U32;
        else if(dim==1 && type=="IntProperty" && size==4)kind=Kind::I32;
        else throw std::runtime_error("Unsupported quest network ID layout: "+std::string(type)+", size="+std::to_string(size)+", arrayDim="+std::to_string(dim));
    }
    uint16_t Read(const void* address) const {
        if(!address)throw std::runtime_error("Missing quest network ID storage");
        int64_t value=0;
        switch(kind) {
            case Kind::U16:value=Load<uint16_t>(address);break;
            case Kind::I16:value=Load<int16_t>(address);break;
            case Kind::U32:value=Load<uint32_t>(address);break;
            case Kind::I32:value=Load<int32_t>(address);break;
        }
        if(value<0 || value>=65535)throw std::runtime_error("Quest network ID outside supported index range");
        return static_cast<uint16_t>(value);
    }
    void Write(void* address,uint16_t value) const {
        if(!address || value>=65535 || (kind==Kind::I16 && value>32767))
            throw std::runtime_error("Quest network ID exceeds reflected storage range");
        switch(kind) {
            case Kind::U16:Store<uint16_t>(address,value);break;
            case Kind::I16:Store<int16_t>(address,value);break;
            case Kind::U32:Store<uint32_t>(address,value);break;
            case Kind::I32:Store<int32_t>(address,value);break;
        }
    }
};
}
