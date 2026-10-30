#pragma once
#include <windows.h>
#include <sddl.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace PS::HelpyDesktop {
using Json=nlohmann::json;
inline constexpr uint32_t Version=1,MaxBody=16*1024*1024,MaxFrame=40*1024*1024;
inline std::wstring Identity(const std::filesystem::path& root){
    auto value=std::filesystem::weakly_canonical(root).wstring();
    std::transform(value.begin(),value.end(),value.begin(),::towlower);return value;
}
inline std::wstring PipeName(const std::filesystem::path& root){
    uint64_t hash=14695981039346656037ull;for(const auto c:Identity(root)){hash^=c;hash*=1099511628211ull;}
    return L"\\\\.\\pipe\\RuneSchema.Helpy."+std::to_wstring(hash);
}
inline std::wstring WakeName(const std::filesystem::path& root){const auto pipe=PipeName(root);return L"Local\\"+pipe.substr(pipe.find_last_of(L'\\')+1)+L".updates";}
inline bool Enabled(const std::filesystem::path& root){
    try{const auto file=root/"plugins/RuneSchema.Helpy/settings/settings.jsonc";
        if(std::filesystem::file_size(file)>65536)return false;std::ifstream input(file);
        const auto value=Json::parse(input,nullptr,true,true);return value.value("enabled",true);
    }catch(...){return false;}
}
inline Json Parse(const std::string& text){
    size_t events=0;return Json::parse(text,[&](int depth,Json::parse_event_t,const Json&){if(depth>64||++events>1024*1024)throw std::runtime_error("Desktop message structure limit");return true;});
}
inline bool Transfer(HANDLE pipe,void* bytes,uint32_t count,bool write,bool overlapped){
    auto* cursor=static_cast<char*>(bytes);
    while(count){DWORD done=0;OVERLAPPED operation{};operation.hEvent=overlapped?CreateEventW(nullptr,TRUE,FALSE,nullptr):nullptr;
        if(overlapped&&!operation.hEvent)return false;
        bool ok=(write?WriteFile(pipe,cursor,count,&done,overlapped?&operation:nullptr):ReadFile(pipe,cursor,count,&done,overlapped?&operation:nullptr))!=FALSE;
        if(!ok&&overlapped&&GetLastError()==ERROR_IO_PENDING){
            if(WaitForSingleObject(operation.hEvent,750)!=WAIT_OBJECT_0){CancelIoEx(pipe,&operation);WaitForSingleObject(operation.hEvent,INFINITE);}
            ok=GetOverlappedResult(pipe,&operation,&done,FALSE)!=FALSE;
        }
        if(operation.hEvent)CloseHandle(operation.hEvent);
        if(!ok||!done)return false;cursor+=done;count-=done;
    }return true;
}
inline bool Send(HANDLE pipe,const Json& value,bool overlapped=false){
    auto text=value.dump();if(text.empty()||text.size()>MaxFrame)return false;uint32_t size=static_cast<uint32_t>(text.size());
    return Transfer(pipe,&size,sizeof(size),true,overlapped)&&Transfer(pipe,text.data(),size,true,overlapped);
}
inline bool Receive(HANDLE pipe,Json& value,bool overlapped=false){
    uint32_t size=0;if(!Transfer(pipe,&size,sizeof(size),false,overlapped)||!size||size>MaxFrame)return false;
    std::string text(size,'\0');if(!Transfer(pipe,text.data(),size,false,overlapped))return false;
    try{value=Parse(text);return value.is_object();}catch(...){return false;}
}
inline bool SameUserPipeSecurity(PSECURITY_DESCRIPTOR& descriptor){
    HANDLE token{};if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return false;DWORD bytes=0;
    GetTokenInformation(token,TokenUser,nullptr,0,&bytes);std::vector<char> data(bytes);
    const bool ok=bytes&&GetTokenInformation(token,TokenUser,data.data(),bytes,&bytes);CloseHandle(token);if(!ok)return false;
    LPWSTR sid{};if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(data.data())->User.Sid,&sid))return false;
    const auto sddl=std::wstring(L"D:P(A;;GA;;;")+sid+L")";LocalFree(sid);
    return ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),SDDL_REVISION_1,&descriptor,nullptr)!=FALSE;
}
class Server {
public:
    using Handler=std::function<Json(const Json&)>;
    ~Server(){Stop();}
    bool Start(const std::filesystem::path& root,Handler handler,std::function<void()> disconnected){
        if(Thread.joinable())return false;PSECURITY_DESCRIPTOR descriptor{};if(!SameUserPipeSecurity(descriptor))return false;
        SECURITY_ATTRIBUTES attributes{sizeof(attributes),descriptor,FALSE};
        Pipe=CreateNamedPipeW(PipeName(root).c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,65536,65536,0,&attributes);
        Updates=CreateEventW(&attributes,TRUE,FALSE,WakeName(root).c_str());LocalFree(descriptor);
        if(Pipe==INVALID_HANDLE_VALUE){if(Updates)CloseHandle(Updates);Updates=nullptr;return false;}Stopping=false;
        Thread=std::thread([this,handler=std::move(handler),disconnected=std::move(disconnected)]{
            while(!Stopping){
                if(!ConnectNamedPipe(Pipe,nullptr)&&GetLastError()!=ERROR_PIPE_CONNECTED){if(Stopping)break;DisconnectNamedPipe(Pipe);continue;}
                {std::lock_guard lock(Mutex);Connected=true;Events.clear();Bytes=0;Overflow=false;if(Updates)ResetEvent(Updates);}
                while(!Stopping){
                    Json request;if(!Receive(Pipe,request))break;Json reply;
                    try{reply=handler(request);}catch(...){reply={{"Code",6}};}
                    reply["Version"]=Version;reply["Pid"]=GetCurrentProcessId();
                    {std::lock_guard lock(Mutex);reply["Reset"]=Overflow;Overflow=false;reply["Events"]=Json::array();
                        size_t delivered=0;while(!Events.empty()&&reply["Events"].size()<16&&(!delivered||delivered+Events.front().size()<=1024*1024)){const auto bytes=Events.front().size();try{reply["Events"].push_back(Parse(Events.front()));}catch(...){reply["Reset"]=true;}Bytes-=bytes;delivered+=bytes;Events.pop_front();}if(Updates){if(Events.empty())ResetEvent(Updates);else SetEvent(Updates);}}
                    if(!Send(Pipe,reply))break;
                }
                {std::lock_guard lock(Mutex);Connected=false;Events.clear();Bytes=0;if(Updates)ResetEvent(Updates);}
                if(disconnected)disconnected();DisconnectNamedPipe(Pipe);
            }
        });return true;
    }
    void Push(const std::string& event){
        std::lock_guard lock(Mutex);if(!Connected||event.size()>MaxBody)return;
        if(Bytes+event.size()>MaxBody||Events.size()>=128){Events.clear();Bytes=0;Overflow=true;}
        Events.push_back(event);Bytes+=event.size();if(Updates)SetEvent(Updates);
    }
    void Stop(){
        Stopping=true;if(Updates)SetEvent(Updates);if(Thread.joinable()){do{CancelSynchronousIo(Thread.native_handle());}while(WaitForSingleObject(Thread.native_handle(),50)==WAIT_TIMEOUT);Thread.join();}
        if(Pipe!=INVALID_HANDLE_VALUE){CloseHandle(Pipe);Pipe=INVALID_HANDLE_VALUE;}if(Updates){CloseHandle(Updates);Updates=nullptr;}
    }
private:
    HANDLE Pipe=INVALID_HANDLE_VALUE,Updates=nullptr;std::thread Thread;std::atomic<bool> Stopping=false;
    std::mutex Mutex;bool Connected=false,Overflow=false;size_t Bytes=0;std::deque<std::string> Events;
};
class Client {
public:
    ~Client(){Disconnect();}
    bool Connect(const std::filesystem::path& root,bool verifyImage=true){
        std::lock_guard lock(Mutex);if(Pipe!=INVALID_HANDLE_VALUE)return true;
        const auto pipe=CreateFileW(PipeName(root).c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED|SECURITY_SQOS_PRESENT|SECURITY_IDENTIFICATION,nullptr);
        if(pipe==INVALID_HANDLE_VALUE)return false;ULONG pid=0;bool valid=GetNamedPipeServerProcessId(pipe,&pid)!=FALSE;
        if(valid&&verifyImage){HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);wchar_t image[32768]{};DWORD length=32768;
            try{valid=process&&QueryFullProcessImageNameW(process,0,image,&length)&&Identity(std::filesystem::path(image).parent_path())==Identity(root.parent_path().parent_path().parent_path());}catch(...){valid=false;}
            if(process)CloseHandle(process);
        }
        if(!valid){CloseHandle(pipe);return false;}Pipe=pipe;Pid=pid;Updates=OpenEventW(SYNCHRONIZE,FALSE,WakeName(root).c_str());LastExchangeMs=GetTickCount64();return true;
    }
    bool Exchange(const Json& request,Json& reply){
        std::lock_guard lock(Mutex);if(Pipe==INVALID_HANDLE_VALUE)return false;
        if(Send(Pipe,request,true)&&Receive(Pipe,reply,true)&&reply.value("Pid",uint32_t{})==Pid.load()){LastExchangeMs=GetTickCount64();return true;}
        CloseHandle(Pipe);Pipe=INVALID_HANDLE_VALUE;if(Updates){CloseHandle(Updates);Updates=nullptr;}Pid=0;return false;
    }
    void Disconnect(){std::lock_guard lock(Mutex);if(Pipe!=INVALID_HANDLE_VALUE)CloseHandle(Pipe);Pipe=INVALID_HANDLE_VALUE;if(Updates){CloseHandle(Updates);Updates=nullptr;}Pid=0;}
    bool Notifications(){std::lock_guard lock(Mutex);return Updates!=nullptr;}
    bool WaitForUpdates(DWORD timeout,HANDLE stopped=nullptr){
        HANDLE update=nullptr;{std::lock_guard lock(Mutex);if(Updates)DuplicateHandle(GetCurrentProcess(),Updates,GetCurrentProcess(),&update,SYNCHRONIZE,FALSE,0);}
        if(!update){if(stopped)WaitForSingleObject(stopped,timeout);else Sleep(timeout);return false;}
        const HANDLE handles[]{update,stopped};const auto result=WaitForMultipleObjects(stopped?2:1,handles,FALSE,timeout);CloseHandle(update);return result==WAIT_OBJECT_0;
    }
    std::atomic<uint32_t> Pid=0;std::atomic<ULONGLONG> LastExchangeMs=0;
private:
    HANDLE Pipe=INVALID_HANDLE_VALUE,Updates=nullptr;std::mutex Mutex;
};
}
