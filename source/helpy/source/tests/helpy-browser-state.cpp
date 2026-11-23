#define HELPY_BROWSER_TEST
#include "../src/Generator/HelpyBrowser.cpp"
#include "miniz.h"
#include <iostream>
#include <thread>

namespace {
std::filesystem::path TestRoot;
std::atomic<int> Requests=0;
std::vector<char> Read(const std::filesystem::path& file){
    std::ifstream in(file,std::ios::binary);
    if(!in)throw std::runtime_error("Missing browser fixture");
    return {std::istreambuf_iterator<char>(in),{}};
}
void Check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
void DownloadFixture(){
    PS::HelpyBrowser::DownloadForTest=[](const std::string& url){
        ++Requests;
        return Read(TestRoot/(url==PS::HelpyBlockerRelease::LatestApi?"release.json":"extension.zip"));
    };
}
void Release(const std::string& tag){
    const auto file=TestRoot/"extension.zip";
    mz_zip_archive zip{};Check(mz_zip_writer_init_file(&zip,file.string().c_str(),0),"Fixture ZIP opens");
    const std::string manifest="{\"manifest_version\":3,\"name\":\"Review fixture\",\"version\":\""+tag+"\"}";
    Check(mz_zip_writer_add_mem(&zip,"manifest.json",manifest.data(),manifest.size(),MZ_NO_COMPRESSION),"Fixture ZIP entry");
    Check(mz_zip_writer_finalize_archive(&zip),"Fixture ZIP completes");mz_zip_writer_end(&zip);
    const auto bytes=Read(file);const auto name="uBOLite_"+tag+".edge.zip";
    const nlohmann::json release={{"tag_name",tag},{"assets",nlohmann::json::array({{
        {"name",name},{"size",bytes.size()},{"digest","sha256:"+PS::HelpyBrowser::Sha256(bytes)},
        {"browser_download_url",std::string(PS::HelpyBlockerRelease::DownloadPrefix)+tag+"/"+name}}})}};
    PS::ConfigFiles::Write(TestRoot/"release.json",release.dump());
}
void Complete(){
    using namespace PS::HelpyBrowser;
    Check(Worker.valid(),"Download worker started");
    Check(Worker.wait_for(std::chrono::seconds(15))==std::future_status::ready,"Download worker completed");
    Poll();
}
void Children(){
    wchar_t executable[32768]{};Check(GetModuleFileNameW(nullptr,executable,32768)>0,"Executable path");
    PROCESS_INFORMATION children[2]{};
    for(auto& child:children){
        std::wstring command=L"\""+std::wstring(executable)+L"\" --publish \""+TestRoot.wstring()+L"\"";
        STARTUPINFOW startup{sizeof(startup)};
        Check(CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&child)!=FALSE,"Child installer starts");
    }
    bool ok=true;
    for(auto& child:children){
        if(WaitForSingleObject(child.hProcess,20000)!=WAIT_OBJECT_0){TerminateProcess(child.hProcess,2);ok=false;}
        DWORD code=1;GetExitCodeProcess(child.hProcess,&code);ok&=code==0;
        CloseHandle(child.hThread);CloseHandle(child.hProcess);
    }
    Check(ok,"Concurrent installer processes both succeed");
}
}
namespace PS::HostServices {std::filesystem::path StateDirectory(){return TestRoot;}}
int main(int argc,char** argv)try{
    using namespace PS::HelpyBrowser;
    if(argc==3&&std::string(argv[1])=="--publish"){
        TestRoot=std::filesystem::path(argv[2]);DownloadFixture();
        State=Install({});Wanted=true;Activated(State.Folder);
        Check(!State.PendingActivation,"Child activation recorded");return 0;
    }
    const auto temp=std::filesystem::temp_directory_path();
    TestRoot=temp/("HelpyBrowser-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
    Check(!std::filesystem::exists(TestRoot),"Unique test boundary");
    struct Cleanup{std::filesystem::path Root,Parent;~Cleanup(){
        Shutdown();std::error_code error;
        if(Root.parent_path()==Parent&&Root.filename().string().starts_with("HelpyBrowser-"))std::filesystem::remove_all(Root,error);
    }}cleanup{TestRoot,temp};
    DownloadFixture();AdBlock(false);
    Check(Requests==0&&!std::filesystem::exists(TestRoot),"Disabled browser creates no files or requests");
    std::filesystem::create_directories(TestRoot);Release("1.0");
    Wanted=true;Checked=true;State.State=Blocker::Failed;
    NextAttempt=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    AdBlock(false);AdBlock(true);
    Check(!Checked&&!Worker.valid()&&Requests==0,"Cooldown remains pending without filesystem/network polling");
    NextAttempt={};AdBlock(true);Complete();
    Check(State.State==Blocker::Ready&&State.PendingActivation,"Cooldown recovers and downloads candidate");
    const auto downloadedRequests=Requests.load();
    AdBlock(false);AdBlock(true);
    Check(State.State==Blocker::Ready&&State.PendingActivation&&!Worker.valid()&&Requests==downloadedRequests,"Enabling a downloaded candidate reuses it without network or cooldown");
    const auto first=State.Folder,marker=TestRoot/"web-browser-extensions/installed.json";
    Check(!std::filesystem::exists(marker),"Download does not publish an unactivated marker");
    Activated(first);Check(Installed().Version=="1.0"&&!State.PendingActivation,"Accepted extension becomes installed");
    const auto firstMarker=Read(marker);
    Release("1.1");NextAttempt={};Check(Start(true),"Update starts");Complete();
    const auto second=State.Folder;
    Check(State.PendingActivation&&std::filesystem::exists(first/"manifest.json")&&Read(marker)==firstMarker,"Update retains old version and marker");
    Rejected(second,"Simulated WebView rejection");
    Check(State.State==Blocker::Ready&&State.Folder==first&&Read(marker)==firstMarker,"Failed activation restores previous installed state");
    NextAttempt={};Check(Start(true),"Retry starts");Complete();Activated(second);
    Check(Installed().Version=="1.1"&&std::filesystem::exists(first/"manifest.json"),"Retry reuses immutable candidate and preserves other processes' files");
    Release("1.2");
    auto corrupted=Read(TestRoot/"extension.zip");corrupted[corrupted.size()/2]^=1;
    {std::ofstream out(TestRoot/"extension.zip",std::ios::binary|std::ios::trunc);out.write(corrupted.data(),corrupted.size());out.close();Check(bool(out),"Corrupt fixture saved");}
    NextAttempt={};Check(Start(true),"Checksum failure starts");Complete();
    Check(State.State==Blocker::Ready&&Installed().Version=="1.1"&&State.Message.find("checksum")!=std::string::npos,"Bad download preserves installed extension before extraction");
    Release("1.2");NextAttempt={};Check(Start(true),"Late candidate starts");Complete();
    const auto late=State.Folder;AdBlock(false);Activated(late);
    Check(Installed().Version=="1.1","A late activation cannot publish after disabling blocking");
    // Two independent Helpy processes publish the same downloaded version and marker.
    Release("1.3");Children();Check(Installed().Version=="1.3","Concurrent publication leaves a valid installed marker");
    Check(std::filesystem::exists(first/"manifest.json")&&std::filesystem::exists(second/"manifest.json"),"Concurrent installers retain loaded versions");
    std::cout<<"PASS: opt-in, cooldown recovery, activation commit, rollback, late replies, immutable candidates, concurrent publication.\n";
    return 0;
}catch(const std::exception& error){PS::HelpyBrowser::Shutdown();std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
