#pragma once
#include <windows.h>
#include <filesystem>
#include <array>
#include <string>

namespace PS {
class HelpyModMetadataWatch {
    HANDLE Directory=INVALID_HANDLE_VALUE;OVERLAPPED Operation{};
    alignas(FILE_NOTIFY_INFORMATION) std::array<unsigned char,65536> Buffer{};
    std::filesystem::path Root;
    bool Arm(){ResetEvent(Operation.hEvent);return ReadDirectoryChangesW(Directory,Buffer.data(),DWORD(Buffer.size()),TRUE,FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_DIR_NAME|FILE_NOTIFY_CHANGE_LAST_WRITE,nullptr,&Operation,nullptr)!=FALSE;}
public:
    HelpyModMetadataWatch()=default;HelpyModMetadataWatch(const HelpyModMetadataWatch&)=delete;
    ~HelpyModMetadataWatch(){Close();}
    static bool Relevant(std::wstring name){
        for(auto& c:name){c=towlower(c);if(c==L'/')c=L'\\';}
        if(name.find(L'\\')==name.npos)return true;
        if(name.ends_with(L"\\cache\\taglines.json")||name.ends_with(L"\\cache\\tips.json")||name.ends_with(L"\\ue4ss\\status.txt"))return true;
        for(const auto* excluded:{L"\\cache\\",L"\\runtime\\",L"\\saved\\",L"\\logs\\",L"\\settings\\",L"\\exports\\",L"\\.runeschema.cache."})if(name.find(excluded)!=name.npos)return false;
        if(name.ends_with(L"\\id.txt")||name.ends_with(L"\\enabled.txt")||name.ends_with(L"\\mods.txt")||name.ends_with(L"\\runeschema.txt"))return true;
        for(const auto* extension:{L".pak",L".ucas",L".utoc",L".dll",L".exe",L".lua",L".json",L".jsonc",L".ini",L".uplugin"})if(name.ends_with(extension))return true;return false;
    }
    void Close(){if(Directory!=INVALID_HANDLE_VALUE){CancelIoEx(Directory,&Operation);DWORD bytes{};GetOverlappedResult(Directory,&Operation,&bytes,TRUE);CloseHandle(Directory);Directory=INVALID_HANDLE_VALUE;}if(Operation.hEvent)CloseHandle(Operation.hEvent);Operation={};}
    void Watch(const std::filesystem::path& root){
        if(root==Root)return;Close();Root=root;
        Directory=CreateFileW((root/L"mods").c_str(),FILE_LIST_DIRECTORY,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED,nullptr);
        if(Directory==INVALID_HANDLE_VALUE)return;Operation.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!Operation.hEvent||!Arm())Close();
    }
    bool Changed(){
        if(Directory==INVALID_HANDLE_VALUE||WaitForSingleObject(Operation.hEvent,0)!=WAIT_OBJECT_0)return false;
        DWORD bytes{};if(!GetOverlappedResult(Directory,&Operation,&bytes,FALSE)){Close();return true;}
        bool relevant=bytes==0;size_t offset=0;
        while(offset+offsetof(FILE_NOTIFY_INFORMATION,FileName)<=bytes){
            const auto* entry=reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(Buffer.data()+offset);
            if(entry->FileNameLength%sizeof(wchar_t)||offset+offsetof(FILE_NOTIFY_INFORMATION,FileName)+entry->FileNameLength>bytes){relevant=true;break;}
            relevant|=Relevant(std::wstring(entry->FileName,entry->FileNameLength/sizeof(wchar_t)));
            if(!entry->NextEntryOffset)break;
            if(entry->NextEntryOffset<offsetof(FILE_NOTIFY_INFORMATION,FileName)||offset+entry->NextEntryOffset>=bytes){relevant=true;break;}offset+=entry->NextEntryOffset;
        }
        if(!Arm())Close();return relevant;
    }
};
}
