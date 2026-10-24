#include "Runtime/ModHealthService.h"
#include "Core/ModManager.h"
#include "Core/RuntimeModPath.h"
#include "Runtime/HostServices.h"
#include "Runtime/Storefront.h"
#include "Core/ModHealthLedger.h"
#include "Core/RuneSchemaDataRoot.h"
#include <process.h>
#include <condition_variable>
namespace PS::ModHealthService {
namespace {
std::mutex Mutex,Lifecycle;std::condition_variable Wake;bool Stop=false;
// Process exit can skip uninstall_mod; never destroy a joinable C++ thread in DLL teardown.
HANDLE Worker=nullptr;
nlohmann::json Snapshot={{"Ready",false}};
bool GameLogMatchesProcess(const std::filesystem::path& file){
    const auto handle=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(handle==INVALID_HANDLE_VALUE)return false;
    std::string header(128*1024,'\0');DWORD read=0;const bool ok=ReadFile(handle,header.data(),DWORD(header.size()),&read,nullptr)!=0;CloseHandle(handle);if(!ok)return false;header.resize(read);
    const auto base=LiveLog::GameBinaryDirectory(header);if(base.empty())return false;wchar_t executable[32768]{};const auto length=GetModuleFileNameW(nullptr,executable,32768);if(!length||length>=32768)return false;
    std::error_code error;const auto expected=std::filesystem::weakly_canonical(std::filesystem::path(executable).parent_path(),error);if(error)return false;
    const auto actual=std::filesystem::weakly_canonical(std::filesystem::u8path(base),error);return !error&&LiveLog::Lower(expected.generic_string())==LiveLog::Lower(actual.generic_string());
}
void Run()noexcept{
    try{
        const auto root=RuntimeModPath::Resolve(HostServices::ModDirectory());
        wchar_t local[32768]{};const auto length=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
        if(!length||length>=32768)throw std::runtime_error("Local AppData is unavailable for mod-health statistics");
        const auto folder=RuneSchemaData::SharedDirectory("health");
        const auto file=folder/(Storefront::IsDedicatedServer()?"mods-server.json":Storefront::Current()==Storefront::Kind::GamePass?"mods-gamepass.json":"mods-steam.json");
        ModManager::Safe(file);ModHealth::Ledger ledger;bool dirty=false;
        if(std::filesystem::exists(file))try{ledger.Load(nlohmann::json::parse(ConfigFiles::Read(file,8*1024*1024)));}
        catch(...){ConfigFiles::Backup(file);dirty=true;} // Retain an unreadable history before rebuilding it.
        std::array<LiveLog::Cursor,3> cursors;std::array<bool,3> readable{};
        std::array<std::filesystem::path,3> paths{LiveLog::LogPath(root,LiveLog::Stream::RuneSchema),LiveLog::LogPath(root,LiveLog::Stream::UE4SS),{}};
        std::set<std::string> present;nlohmann::json rows=nlohmann::json::array();
        auto nextScan=std::chrono::steady_clock::time_point{},nextSave=nextScan;
        FILETIME started{},ended{},kernel{},user{};const bool processTime=GetProcessTimes(GetCurrentProcess(),&started,&ended,&kernel,&user)!=0;
        std::string storageError;
        for(;;){
            {std::lock_guard lock(Mutex);if(Stop)break;}
            const auto now=std::chrono::steady_clock::now();
            if(now>=nextScan){
                std::lock_guard gate(ModManager::Gate);rows=ModManager::Read(root,true).at("Rows");present.clear();
                for(const auto& row:rows)if(row.value("Present",false))present.insert(row.at("Name").get<std::string>());
                dirty|=ledger.Reconcile(present);nextScan=now+std::chrono::seconds(5);
                const auto game=LiveLog::ResolveGameLog(root,local);
                // Never replay an older play/storefront's game log into this session.
                WIN32_FILE_ATTRIBUTE_DATA attributes{};
                if(processTime&&GetFileAttributesExW(game.c_str(),GetFileExInfoStandard,&attributes)&&CompareFileTime(&attributes.ftLastWriteTime,&started)>=0&&GameLogMatchesProcess(game)){if(game!=paths[2]){paths[2]=game;cursors[2]={};}}
                else{paths[2].clear();cursors[2]={};readable[2]=false;}
            }
            for(size_t source=0;source<paths.size();++source){
                if(paths[source].empty())continue;auto batch=LiveLog::Read(paths[source],cursors[source],false);cursors[source]=std::move(batch.Next);readable[source]=batch.Error.empty();
                for(const auto& line:batch.Lines){if(source==1&&line.find("[RuneSchema]")!=line.npos)continue; // Core messages already counted from RuneSchema.log.
                    const auto entry=LiveLog::Parse(line);for(const auto& owner:present)dirty|=ledger.Observe(owner,entry,source==2?"Game":source==1?"UE4SS":"RuneSchema");}
            }
            if(dirty&&now>=nextSave){try{const auto text=ledger.Document().dump();if(text.size()>8*1024*1024)throw std::runtime_error("Mod-health history exceeds 8 MiB");ConfigFiles::Write(file,text);dirty=false;storageError.clear();}
                catch(const std::exception& error){storageError=error.what();}nextSave=now+std::chrono::seconds(5);}
            nlohmann::json output={{"Ready",readable[0]||readable[1]},{"File",file.string()},{"GameLogAvailable",readable[2]},{"StorageError",storageError},{"Mods",nlohmann::json::object()}};
            for(const auto& owner:present)output["Mods"][owner]=ledger.Read(owner);
            {std::unique_lock lock(Mutex);Snapshot=std::move(output);if(Wake.wait_for(lock,std::chrono::milliseconds(250),[]{return Stop;}))break;}
        }
        if(dirty){const auto text=ledger.Document().dump();if(text.size()<=8*1024*1024)ConfigFiles::Write(file,text);}
    }catch(const std::exception& error){std::lock_guard lock(Mutex);Snapshot={{"Ready",false},{"StorageError",error.what()}};}
    catch(...){std::lock_guard lock(Mutex);Snapshot={{"Ready",false},{"StorageError","Mod-health monitor stopped unexpectedly"}};}
}
unsigned __stdcall Monitor(void*)noexcept{Run();return 0;}
}
void Initialize(){
    std::lock_guard lifecycle(Lifecycle);std::lock_guard lock(Mutex);if(Worker)return;Stop=false;
    Worker=reinterpret_cast<HANDLE>(_beginthreadex(nullptr,0,Monitor,nullptr,0,nullptr));
    if(!Worker)throw std::runtime_error("Mod-health monitor thread could not start");
}
void Shutdown()noexcept{
    std::lock_guard lifecycle(Lifecycle);{std::lock_guard lock(Mutex);Stop=true;}Wake.notify_all();
    if(Worker&&WaitForSingleObject(Worker,INFINITE)==WAIT_OBJECT_0){CloseHandle(Worker);Worker=nullptr;}
}
nlohmann::json Read(){std::lock_guard lock(Mutex);return Snapshot;}
}
