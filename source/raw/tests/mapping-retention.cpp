#include "Core/MappingRetention.h"
#include <cassert>
#include <chrono>
#include <fstream>
using namespace PS::MappingRetention;
int main(){
    const Json receipt={{"Version",1},{"Build","0123456789abcdef"},{"Fingerprint","1111111111111111"}};
    assert(Identity(receipt));auto malformed=receipt;malformed["Build"]="../../";assert(!Identity(malformed));
    malformed=receipt;malformed["Fingerprint"]="C:/Mappings";assert(!Identity(malformed));
    const auto root=fs::temp_directory_path()/("mapping-retention-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto legacy=root/"legacy",cache=root/"AppData/STEAM/mappings",other=root/"AppData/GAMEPASS/mappings";
    const auto write=[](const fs::path& path,const std::string& value){fs::create_directories(path.parent_path());std::ofstream stream(path,std::ios::binary);stream<<value;assert(stream.good());};
    const auto source=File(legacy,receipt),active=File(cache,receipt);
    write(source,"good");write(legacy/"current.json",receipt.dump());
    const auto valid=[](const fs::path& path,const std::string& fingerprint){assert(fingerprint=="1111111111111111");if(PS::ConfigFiles::Read(path)!="good")throw std::runtime_error("bad mapping");};
    const auto safe=[](const fs::path& path){for(auto current=path;!current.empty();){if(fs::is_symlink(current))throw std::runtime_error("redirected path");const auto parent=current.parent_path();if(parent==current)break;current=parent;}};
    bool failed=false;
    try{Migrate(legacy,cache,[](auto&,auto&){throw std::runtime_error("invalid source");},safe);}catch(...){failed=true;}
    assert(failed&&fs::exists(source)&&!fs::exists(cache/"current.json"));
    int validations=0;failed=false;
    try{Migrate(legacy,cache,[&](const auto& path,const auto& fingerprint){valid(path,fingerprint);if(++validations==2)throw std::runtime_error("failed copied validation");},safe);}catch(...){failed=true;}
    assert(failed&&fs::exists(source)&&!fs::exists(active)&&!fs::exists(fs::path(active.string()+".migrating"))&&!fs::exists(cache/"current.json"));
    assert(Migrate(legacy,cache,valid,safe));assert(fs::exists(source)&&PS::ConfigFiles::Read(active)=="good");
    assert(!Migrate(legacy,cache,valid,safe));assert(!Migrate(root/"missing",other,valid,safe));
    write(other/"keep.usmap","other platform");write(cache/"custom.usmap","custom");
    write(active.parent_path()/"user-notes.txt","user");write(cache/"unrelated.json","keep");
    write(cache/"current.json.invalid-123-1.bak","old receipt");
    write(cache/"current.json.invalid-not-ours.bak","keep");
    write(cache/"index/usmap-2222222222222222.json","old");
    write(cache/"index/usmap-1111111111111111.json","active index");
    write(cache/"index/custom.json","keep");
    write(cache/"requests/2222222222222222.request","stale");
    write(cache/"requests/2222222222222222.request.result","stale");
    write(cache/"requests/3333333333333333.request","fresh");
    const auto oldTime=fs::file_time_type::clock::now()-std::chrono::hours(25);
    fs::last_write_time(cache/"requests/2222222222222222.request",oldTime);
    fs::last_write_time(cache/"requests/2222222222222222.request.result",oldTime);
    const auto retired=cache/"9999999999999999/2222222222222222.usmap";write(retired,"retired");
    const auto interrupted=cache/"9999999999999999/2222222222222222.usmap.migrating";
    write(interrupted,"interrupted copy");fs::last_write_time(interrupted,oldTime);
    fs::create_directory(cache/"8888888888888888");
    failed=false;try{Cleanup(cache,receipt,active,[](auto&,auto&){throw std::runtime_error("bad active");},safe);}catch(...){failed=true;}
    assert(failed&&fs::exists(retired)&&fs::exists(cache/"index/usmap-2222222222222222.json"));
    const auto link=cache/"7777777777777777";std::error_code linkError;fs::create_directory_symlink(other,link,linkError);
    if(!linkError){
        failed=false;try{Cleanup(cache,receipt,active,valid,safe);}catch(...){failed=true;}
        assert(failed&&fs::exists(other/"keep.usmap")&&fs::exists(retired));fs::remove(link);
    }
    assert(Cleanup(cache,receipt,active,valid,safe)==6);
    assert(!fs::exists(retired)&&!fs::exists(retired.parent_path())&&!fs::exists(cache/"8888888888888888"));
    assert(fs::exists(active)&&fs::exists(cache/"current.json")&&fs::exists(cache/"custom.usmap")&&fs::exists(cache/"unrelated.json"));
    assert(fs::exists(active.parent_path()/"user-notes.txt")&&fs::exists(cache/"current.json.invalid-not-ours.bak"));
    assert(fs::exists(cache/"index/usmap-1111111111111111.json")&&fs::exists(cache/"index/custom.json")&&fs::exists(cache/"requests/3333333333333333.request"));
    assert(Cleanup(legacy,receipt,active,valid,safe)==2);assert(!fs::exists(source)&&!fs::exists(legacy/"current.json"));
    assert(Cleanup(cache,receipt,active,valid,safe)==0&&fs::exists(other/"keep.usmap"));
    write(root/"bad/current.json",malformed.dump());failed=false;
    try{Migrate(root/"bad",root/"destination",valid,safe);}catch(...){failed=true;}
    assert(failed&&!fs::exists(root/"destination/current.json")&&fs::exists(root/"bad/current.json"));
    // Every fixture is beneath this unique test-owned temporary root.
    assert(root.filename().string().starts_with("mapping-retention-"));fs::remove_all(root);
}
