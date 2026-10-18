#include "Core/DiagnosticLogFile.h"
#include <algorithm>
#include <cassert>
#include <iterator>
#include <vector>
using namespace PS::DiagnosticLog;
std::string Read(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(){
    const auto root=std::filesystem::temp_directory_path()/("runeschema-log-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    const auto path=root/"RuneSchema.log";
    File sink;
    assert(sink.Open(path));assert(sink.Write("normal summary",true));
    assert(Read(path).contains("normal summary"));
    assert(sink.Write("idle tail"));
    // A short idle burst is flushed by the timer, not stranded until 64 rows.
    bool flushed=false;
    for(int i=0;i<50&&!flushed;++i){std::this_thread::sleep_for(std::chrono::milliseconds(10));flushed=Read(path).contains("idle tail");}
    assert(flushed);
    std::vector<std::thread> writers;
    for(int i=0;i<4;++i)writers.emplace_back([&,i]{for(int n=0;n<100;++n)assert(sink.Write("writer="+std::to_string(i)+" row="+std::to_string(n)));});
    for(auto& writer:writers)writer.join();
    assert(sink.Write("warning: urgent",true));assert(Read(path).contains("warning: urgent"));
    sink.Close();sink.Close();assert(!sink.Write("after shutdown"));
    const auto all=Read(path);
    for(int i=0;i<4;++i)for(int n=0;n<100;++n)assert(all.contains("writer="+std::to_string(i)+" row="+std::to_string(n)+"\n"));
    assert(std::count(all.begin(),all.end(),'\n')==403&&all.contains("Z thread="));
    assert(sink.Open(path));assert(sink.Write("next session",true));sink.Close();assert(!Read(path).contains("normal summary"));
    assert(!std::filesystem::exists(root/"logs"));
    assert(sink.Open(path,Session::Append));assert(sink.Write("configuration error",true));sink.Close();assert(Read(path).contains("next session"));
    const auto previous=Read(path);
    assert(sink.Open(path,Session::ArchivePrevious));assert(sink.Write("archived session",true));sink.Close();
    std::vector<std::filesystem::path> archives;
    for(const auto& entry:std::filesystem::directory_iterator(root/"logs")){
        const auto name=entry.path().filename().string();assert(name.starts_with("RuneSchema-")&&name.ends_with("Z.log"));archives.push_back(entry.path());
    }
    assert(archives.size()==1&&Read(archives[0])==previous&&Read(path).contains("archived session"));
    assert(sink.Open(path,Session::ArchivePrevious));assert(sink.Write("third session",true));sink.Close();
    assert(Read(archives[0])==previous);size_t count=0;for(const auto& entry:std::filesystem::directory_iterator(root/"logs"))++count;assert(count==2);
    // No size-triggered seam or dropped beginning, including beyond the old 8 MiB limit.
    const auto large=root/"continuous.log";assert(sink.Open(large));assert(sink.Write("start",true));
    assert(sink.Write(std::string(8*1024*1024+1,'x'),true));assert(sink.Write("end",true));sink.Close();
    const auto continuous=Read(large);assert(continuous.contains("start")&&continuous.ends_with("end\n")&&!std::filesystem::exists(root/"continuous.log.previous"));
    const auto blocked=root/"blocked";std::filesystem::create_directory(blocked);
    {std::ofstream original(blocked/"RuneSchema.log");original<<"do not discard";std::ofstream blocker(blocked/"logs");blocker<<"not a folder";}
    assert(!sink.Open(blocked/"RuneSchema.log",Session::ArchivePrevious)&&sink.TakeFailure());assert(Read(blocked/"RuneSchema.log")=="do not discard");
    assert(!sink.Open(root/"missing"/"RuneSchema.log"));assert(sink.TakeFailure());assert(!sink.TakeFailure());
    assert(!std::filesystem::exists(root/"missing"));
    assert(!sink.Open(root));assert(sink.TakeFailure());sink.Close();
    std::filesystem::remove_all(root);
}
