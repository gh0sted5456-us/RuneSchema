#pragma once
#include <Windows.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <stdexcept>
#include <new>
namespace PS::CharacterPreviewFrame {
inline constexpr uint32_t Side=512,Bytes=Side*Side*4,Magic=0x52535034;
struct Data {uint32_t Signature=Magic,Version=3;uint64_t Serial=0,Heartbeat=0;int32_t Yaw=0,Zoom=100,Pitch=0,Animation=0,Playing=0;uint32_t Stop=0;char Status[256]{};unsigned char Pixels[Bytes]{};};
// Local, bounded pixel transport. Neither side publishes engine pointers or
// reads a cache file on the frame path. A busy writer never blocks the UI.
class Channel {
    HANDLE mapping=nullptr,gate=nullptr;Data* data=nullptr;std::string name;
public:
    ~Channel(){Close();}Channel()=default;Channel(const Channel&)=delete;Channel& operator=(const Channel&)=delete;
    void Close(){if(data)UnmapViewOfFile(data);if(mapping)CloseHandle(mapping);if(gate)CloseHandle(gate);mapping=nullptr;gate=nullptr;data=nullptr;name.clear();}
    const std::string& Name()const{return name;}
    bool Lock(){if(!gate)return false;const auto result=WaitForSingleObject(gate,0);if(result==WAIT_ABANDONED){ReleaseMutex(gate);return false;}return result==WAIT_OBJECT_0;}
    void Unlock(){ReleaseMutex(gate);}
    Data* Get(){return data;}
    void Open(const std::string& value,bool create){
        Close();const auto prefix="Local\\RuneSchema.CharacterPreview."+std::to_string(GetCurrentProcessId())+".";
        if(!value.starts_with(prefix)||value.size()>128||value.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.\\")!=value.npos)throw std::runtime_error("Invalid local preview channel");
        name=value;const std::wstring wide(value.begin(),value.end());gate=CreateMutexW(nullptr,FALSE,(wide+L".gate").c_str());
        mapping=create?CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Data),wide.c_str()):OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,wide.c_str());
        if(create&&mapping&&GetLastError()==ERROR_ALREADY_EXISTS){Close();throw std::runtime_error("Preview channel already exists");}
        if(!gate||!mapping){Close();throw std::runtime_error("Native preview channel unavailable");}
        data=static_cast<Data*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Data)));if(!data){Close();throw std::runtime_error("Cannot map native preview pixels");}
        if(create){if(!Lock()){Close();throw std::runtime_error("Preview channel is busy");}new(data)Data;data->Heartbeat=GetTickCount64();Unlock();}
        else if(data->Signature!=Magic||data->Version!=3){Close();throw std::runtime_error("Native preview transport version mismatch");}
    }
};
}
