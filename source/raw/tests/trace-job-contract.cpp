#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "nlohmann/json.hpp"
#include "Core/DeferredTraceExport.h"
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

static std::string Read(const std::filesystem::path& path)
{
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error("cannot read "+path.string());
    std::ostringstream text;text<<file.rdbuf();return text.str();
}

int main(int argc,char** argv)
{
    if(argc!=3)return 2;
    const auto loader=Read(argv[1]);
    const auto config=Read(argv[2]);

    assert(loader.find("SettingsDirectory()/\"jobs\"")!=std::string::npos);
    assert(loader.find("RuneSchema.TraceJobResult.v1")!=std::string::npos);
    assert(loader.find("if(job.ConsoleEvents)PS::Log")!=std::string::npos);
    assert(loader.find("characterEditorPreset")==std::string::npos);
    assert(loader.find("add(\"character-editor\"")==std::string::npos);
    assert(config.find("characterEditorPreset")==std::string::npos);
    const auto callback=loader.substr(loader.find("void DragonWildsRawTableLoader::TraceJobEvent"),loader.find("void DragonWildsRawTableLoader::OnDatatableSerialized")-loader.find("void DragonWildsRawTableLoader::TraceJobEvent"));
    assert(callback.find("ConfigFiles::Write")==callback.npos&&callback.find("dump(")==callback.npos);
    assert(loader.contains("iteration.RemoveSelf();return;")&&loader.contains("m_traceExports->Submit(job.Id,result)"));
    {
        std::mutex mutex;std::condition_variable ready;bool entered=false,release=false,submitted=false;
        std::vector<int> writes;std::thread::id writer;
        PS::DeferredTraceExport exports([&](const auto&,const auto& snapshot){
            std::unique_lock lock(mutex);writer=std::this_thread::get_id();
            if(!entered){entered=true;ready.notify_all();ready.wait(lock,[&]{return release;});}
            writes.push_back(snapshot.template get<int>());
        },[](const auto&,const auto&){assert(false);});
        exports.Submit("job",1);
        {std::unique_lock lock(mutex);assert(ready.wait_for(lock,std::chrono::seconds(2),[&]{return entered;}));}
        std::thread producer([&]{exports.Submit("job",2);exports.Submit("job",3);std::lock_guard lock(mutex);submitted=true;ready.notify_all();});
        {std::unique_lock lock(mutex);assert(ready.wait_for(lock,std::chrono::seconds(2),[&]{return submitted;}));release=true;ready.notify_all();}
        producer.join();exports.Stop();exports.Stop();
        assert(writer!=std::this_thread::get_id()&&writes==std::vector<int>({1,3}));
    }
    {
        std::mutex mutex;std::condition_variable ready;int attempts=0,failures=0;bool written=false;
        PS::DeferredTraceExport exports([&](const auto&,const auto&){std::lock_guard lock(mutex);if(++attempts==1)throw std::runtime_error("retry me");written=true;ready.notify_all();},
            [&](const auto&,const auto&){std::lock_guard lock(mutex);++failures;});
        exports.Submit("retry",1);
        {std::unique_lock lock(mutex);assert(ready.wait_for(lock,std::chrono::seconds(3),[&]{return written;}));}
        exports.Stop();assert(attempts==2&&failures==1);
    }
    {
        int failures=0;PS::DeferredTraceExport exports([](const auto&,const auto&){throw std::runtime_error("disk unavailable");},[&](const auto&,const auto&){++failures;});
        exports.Submit("final",1);exports.Stop();assert(failures==1);
    }

    std::cout<<"Trace jobs are JSON-owned and file-exported without default console event spam.\n";
}
