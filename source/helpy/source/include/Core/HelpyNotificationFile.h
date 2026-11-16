#pragma once
#include "Core/HelpyNotifications.h"
#include "Core/ConfigFiles.h"
#include "Core/RuneSchemaDataRoot.h"

namespace PS::HelpyNotifications {
struct File {
    std::filesystem::path Path;std::string Baseline;bool Existed=false,Ready=false;
    Store Load(){
        RuneSchemaData::Safe(Path);Existed=std::filesystem::exists(Path);
        Baseline=Existed?ConfigFiles::Read(Path,2*1024*1024):std::string{};
        auto store=Existed?Store::Parse(Json::parse(Baseline)):Store{};Ready=true;return store;
    }
    void Save(const Store& store){
        if(!Ready)throw std::runtime_error("Notification history is unavailable; the original file was retained.");
        uint64_t hash=1469598103934665603ull;for(auto c:std::filesystem::absolute(Path).wstring()){hash^=towlower(c);hash*=1099511628211ull;}
        const auto name=L"Local\\RuneSchema-Notifications-"+std::to_wstring(hash);
        HANDLE mutex=CreateMutexW(nullptr,FALSE,name.c_str());if(!mutex)throw std::runtime_error("Cannot lock notification history.");
        const auto wait=WaitForSingleObject(mutex,1000);if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED){CloseHandle(mutex);throw std::runtime_error("Notification history is busy.");}
        struct Close{HANDLE Value;~Close(){ReleaseMutex(Value);CloseHandle(Value);}}close{mutex};
        RuneSchemaData::Safe(Path);const bool exists=std::filesystem::exists(Path);
        if(exists!=Existed||(exists&&ConfigFiles::Read(Path,2*1024*1024)!=Baseline))throw std::runtime_error("Notification history changed in another Helpy process. Restart Helpy to reload it.");
        const auto text=store.Document().dump(-1,' ',false,Json::error_handler_t::replace);if(text.size()>2*1024*1024)throw std::runtime_error("Notification history is too large.");
        ConfigFiles::Write(Path,text);Baseline=text;Existed=true;
    }
};
}
