#include "Core/IconDiskMaintenance.h"
#include <fstream>
#include <stdexcept>
#include <string>
static void Check(bool value){if(!value)throw std::runtime_error("Icon disk maintenance regression");}
static void Write(const std::filesystem::path& file){std::ofstream out(file,std::ios::binary);out<<std::string(128,'x');Check(bool(out));}
int main(int argc,char** argv){
    Check(argc==2);namespace fs=std::filesystem;using namespace PS::IconDiskMaintenance;
    const auto root=fs::absolute(argv[1])/std::to_string(GetCurrentProcessId());fs::create_directories(root);
    const auto old=root/"old.rsicon",current=root/"current.rsicon",other=root/"manifest.json";
    Write(old);Write(current);Write(other);
    const auto before=fs::file_time_type::clock::now();fs::last_write_time(old,before-std::chrono::hours(48));
    {Worker worker;worker.Post({root,current,200,false,before},1);} // joins only filesystem work
    Check(!fs::exists(old)&&fs::exists(current)&&fs::exists(other));
    Write(old);{Worker stopped;stopped.Shutdown();stopped.Post({root,current,1,true,before+std::chrono::hours(1)},2);}
    Check(fs::exists(old)&&fs::exists(current));
    Write(old);fs::last_write_time(old,before-std::chrono::hours(48));
    const auto fresh=root/"fresh.rsicon";Write(fresh);fs::last_write_time(fresh,before+std::chrono::hours(1));
    Run({root,current,4096,true,before});
    Check(!fs::exists(old)&&fs::exists(current)&&fs::exists(fresh)&&fs::exists(other));
    Run({root,current,0,false,before});Check(fs::exists(current));
    const auto child=root/"nested";fs::create_directories(child);const auto nested=child/"nested.rsicon";Write(nested);
    fs::last_write_time(nested,before-std::chrono::hours(24));Run({root,current,260,false,before});
    Check(!fs::exists(nested)&&fs::exists(other)&&fs::exists(current));
    // A redirected root must not permit maintenance outside the supplied tree.
    const auto outside=root/"outside";fs::create_directories(outside);Write(outside/"keep.rsicon");
    const auto redirect=root/"redirect";std::error_code ec;fs::create_directory_symlink(outside,redirect,ec);
    if(!ec){Run({redirect,redirect/"none.rsicon",1,true,before+std::chrono::hours(4)});Check(fs::exists(outside/"keep.rsicon"));fs::remove(redirect);}
    // Remove only this process's small, known fixture files; no recursive delete.
    for(const auto& file:{old,current,other,fresh,nested,outside/"keep.rsicon"})fs::remove(file,ec);
    fs::remove(child,ec);fs::remove(outside,ec);fs::remove(root,ec);
}
