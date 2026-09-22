// Exercises the shipped migration and file-I/O implementations, not Unreal.
// The injected validator is a stand-in; the production nlohmann codec is not
// compiled by this portable test. Real files, staging, and publication are used.
#include "Runtime/F2CacheMigration.h"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <fstream>

namespace fs=std::filesystem;
namespace io=PS::F2PreferenceFile;
namespace migration=PS::F2CacheMigration;
int checks=0;
void Check(bool condition,const char* name) {
    ++checks;
    if(!condition)throw std::runtime_error(name);
}
template<class Action> void Reject(Action&& action,const char* name) {
    bool failed=false;
    try{action();}catch(const std::exception&){failed=true;}
    Check(failed,name);
}
void Write(const fs::path& file,const std::string& data) {
    fs::create_directories(file.parent_path());
    std::ofstream out(file,std::ios::binary);
    out<<data;
    if(!out)throw std::runtime_error("test fixture write failed");
}
std::string Read(const fs::path& file) {return io::Read(file,1024*1024).value_or("[absent]");}
int main() {
    const auto root=fs::temp_directory_path()/("rs-f2-settings-"+std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        const auto owner=root/"ue4ss"/"Mods"/"RuneSchema";
        const auto current=owner/"settings"/"F2-catalog-cache.json";
        const auto legacy=owner/"runtime"/"F2-catalog-cache.json";
        const auto reference=owner/"settings"/"F2-reference-index.json";
        const auto oldReference=owner/"runtime"/"F2-reference-index.json";
        auto staging=current;staging+=".pending";
        const auto value=std::string("{\"Version\":1,\"Entries\":[]}");
        int validations=0;
        const auto validate=[&](const fs::path& stage){
            ++validations;
            if(Read(stage)!=value)throw std::runtime_error("fixture validator rejected data");
        };
        const auto reset=[&]{fs::remove_all(root);validations=0;};
        const auto run=[&]{return migration::Prepare(current,legacy,validate,1024);};

        Check(run()==migration::Result::NoLegacyFile,"empty install has no migration");
        Check(!fs::exists(root),"empty check creates no folders");
        Check(validations==0,"absent file is not validated");

        Write(legacy,value);
        Write(owner/"settings"/"F2-catalog-sources.json","user settings sentinel");
        Write(owner/"settings"/"F2-favorites.json","favorites sentinel");
        Write(owner/"mods"/"runeschema"/"assets"/"item.json","authored item sentinel");
        Write(owner/"mods"/"runeschema"/"spawns"/"enemy.json","authored spawn sentinel");
        Check(run()==migration::Result::CopiedLegacy,"legacy cache migrates to settings");
        Check(Read(current)==value,"destination preserves bytes");
        Check(Read(legacy)==value,"legacy retained");
        Check(!fs::exists(staging),"staging removed after success");
        Check(validations==1,"staged validation occurs before publish");
        Check(Read(owner/"settings"/"F2-catalog-sources.json")=="user settings sentinel","settings not replaced");
        Check(Read(owner/"settings"/"F2-favorites.json")=="favorites sentinel","favorites not replaced");
        Check(Read(owner/"mods"/"runeschema"/"assets"/"item.json")=="authored item sentinel","assets not touched");
        Check(Read(owner/"mods"/"runeschema"/"spawns"/"enemy.json")=="authored spawn sentinel","spawns not touched");
        Check(!fs::exists(owner/"mods"/"runeschema"/"settings"),"no content-mod settings subtree");
        Write(legacy,"stale different content");
        Check(run()==migration::Result::CurrentExists,"current copy always wins");
        Check(Read(current)==value,"current data not overwritten by stale legacy");
        Check(validations==1,"no repeated legacy migration");

        Write(oldReference,value);
        Check(migration::Prepare(reference,oldReference,validate,1024)==migration::Result::CopiedLegacy,"reference cache independently migrates");
        Check(Read(reference)==value&&Read(oldReference)==value,"reference source and target retained");
        Check(Read(current)==value,"other cache unaffected");

        reset();Write(current,"invalid destination sentinel");Write(legacy,value);
        Check(run()==migration::Result::CurrentExists,"invalid current copy is not replaced");
        Check(Read(current)=="invalid destination sentinel","current repair remains user choice");
        Check(validations==0,"legacy not validated when current exists");

        reset();Write(legacy,"invalid source");
        Reject(run,"validator failure stops publication");
        Check(!fs::exists(current),"invalid source not installed");
        Check(Read(legacy)=="invalid source","invalid legacy retained for inspection");
        Check(!fs::exists(staging),"own failed stage removed");

        reset();Write(legacy,value);
        Reject([&]{migration::Prepare(current,legacy,[&](const fs::path& stage){Write(stage,"tampered");},1024);},"readback mismatch stops publication");
        Check(!fs::exists(current)&&Read(legacy)==value,"tampered stage preserves legacy without destination");
        Check(!fs::exists(staging),"tampered stage cleaned");

        reset();Write(legacy,value);Write(staging,"other writer");
        Reject(run,"existing staging file blocks migration");
        Check(Read(staging)=="other writer","other writer stage retained");
        Check(!fs::exists(current)&&Read(legacy)==value,"staging conflict preserves data");

        reset();Write(legacy,value);
        Reject([&]{migration::Prepare(current,legacy,[&](const fs::path&){Write(current,"new user data");},1024);},"late destination creation cannot be clobbered");
        Check(Read(current)=="new user data","atomic create-only keeps late current copy");
        Check(Read(legacy)==value,"late destination race retains legacy");
        Check(!fs::exists(staging),"late race cleans own stage");

        reset();Write(legacy,std::string(65,'x'));
        Reject([&]{migration::Prepare(current,legacy,[](const fs::path&){},64);},"oversized source rejected");
        Check(!fs::exists(current),"oversized source not published");
        Check(!fs::exists(owner/"settings"),"oversized source creates no settings directory");
        Check(Read(legacy).size()==65,"oversized source preserved");

        reset();Write(legacy,value);Write(owner/"settings","blocking file");
        Reject(run,"bad destination parent fails safely");
        Check(Read(owner/"settings")=="blocking file"&&Read(legacy)==value,"bad destination parent preserves both files");

        reset();fs::create_directories(legacy);
        Reject(run,"directory legacy rejected");
        Check(fs::is_directory(legacy)&&!fs::exists(current),"directory legacy retained");

        reset();Write(legacy,value);fs::create_directories(current);
        Check(run()==migration::Result::CurrentExists,"destination directory not replaced");
        Check(fs::is_directory(current)&&Read(legacy)==value,"directory destination retained for normal reader to report");

        reset();Write(root/"outside",value);fs::create_directories(legacy.parent_path());fs::create_symlink(root/"outside",legacy);
        Reject(run,"legacy symlink rejected");
        Check(!fs::exists(current)&&Read(root/"outside")==value,"legacy symlink cannot redirect a copy");
        fs::remove(legacy);fs::create_symlink(root/"missing",legacy);
        Reject(run,"dangling legacy symlink rejected");
        Check(fs::is_symlink(legacy),"dangling legacy symlink retained");

        reset();Write(legacy,value);Write(root/"outside","external");fs::create_directories(current.parent_path());fs::create_symlink(root/"outside",current);
        Reject(run,"destination symlink rejected");
        Check(Read(root/"outside")=="external"&&Read(legacy)==value,"destination symlink leaves files unchanged");
        fs::remove(current);fs::create_symlink(root/"missing",current);
        Reject(run,"dangling destination symlink rejected");
        Check(fs::is_symlink(current)&&!fs::exists(root/"missing"),"dangling target never followed");

        reset();Write(legacy,value);
        Reject([&]{migration::Prepare(legacy,legacy,validate);},"same-path migration rejected");
        Check(Read(legacy)==value,"same-path guard preserves data");

        reset();Write(current,value);fs::create_directories(legacy.parent_path());fs::create_symlink(root/"missing",legacy);
        Check(run()==migration::Result::CurrentExists,"existing settings cache avoids inspecting invalid legacy");
        Check(validations==0&&Read(current)==value,"current copy preferred without source callbacks");

        reset();Write(legacy,value);
        std::atomic<bool> inStage{false},release{false},firstOK{false};
        std::thread first([&]{
            try {
                firstOK=migration::Prepare(current,legacy,[&](const fs::path& stage){
                    if(Read(stage)!=value)throw std::runtime_error("bad stage");
                    inStage.store(true);while(!release.load())std::this_thread::yield();
                },1024)==migration::Result::CopiedLegacy;
            }catch(...){inStage.store(true);}
        });
        while(!inStage.load())std::this_thread::yield();
        bool secondRejected=false;
        try{(void)run();}catch(...){secondRejected=true;}
        release.store(true);first.join();
        Check(secondRejected,"concurrent copy rejected while stage owned");
        Check(firstOK.load(),"owning migration completes");
        Check(Read(current)==value&&Read(legacy)==value,"concurrent migration preserves both copies");
        Check(!fs::exists(staging),"concurrent migration leaves no stage");

        // Existing preference writes still replace, including after migration.
        io::Write(current,"next",[&](const fs::path& stage){Check(Read(stage)=="next","normal update staged data");});
        Check(Read(current)=="next"&&Read(legacy)==value,"normal update goes only to settings copy");
        Check(run()==migration::Result::CurrentExists&&Read(current)=="next","old cache cannot roll back updated current copy");
        Check(!fs::exists(owner/"mods"/"runeschema"),"migration never created content mod folders");
        fs::remove_all(root);
        std::cout<<"PASS: "<<checks<<" migration/file-I/O assertions (POSIX; validator injected).\n";
        return 0;
    }catch(const std::exception& e){
        std::error_code ignored;fs::remove_all(root,ignored);
        std::cerr<<"FAIL after "<<checks<<" assertions: "<<e.what()<<'\n';return 1;
    }
}
