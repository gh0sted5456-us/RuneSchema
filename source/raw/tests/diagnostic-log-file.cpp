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
    assert(sink.Open(path));assert(sink.Write("next session",true));sink.Close();assert(Read(path).contains("normal summary"));
    File rotating(512);const auto rotated=root/"small.log";
    assert(rotating.Open(rotated));assert(rotating.Write(std::string(600,'x'),true));assert(rotating.Write("new file",true));rotating.Close();
    assert(Read(rotated).contains("new file")&&Read(root/"small.log.previous").contains(std::string(600,'x')));
    assert(!sink.Open(root/"missing"/"RuneSchema.log"));assert(sink.TakeFailure());assert(!sink.TakeFailure());
    assert(!std::filesystem::exists(root/"missing"));
    assert(!sink.Open(root));assert(sink.TakeFailure());sink.Close();
    std::filesystem::remove_all(root);
}
