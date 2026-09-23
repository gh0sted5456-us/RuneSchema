#include "Runtime/F2PreferenceFile.h"
#include <chrono>
#include <iostream>
#include <thread>
#include <atomic>
using namespace PS::F2PreferenceFile;
namespace fs=std::filesystem;
int checks=0;void check(bool y,const char* why){++checks;if(!y)throw std::runtime_error(why);}
template<class F>void rejects(F f,const char* why){try{f();}catch(...){++checks;return;}throw std::runtime_error(why);}
void put(const fs::path& p,const std::string& s){fs::create_directories(p.parent_path());std::ofstream o(p,std::ios::binary);o<<s;}
int main(){auto root=fs::temp_directory_path()/("runeschema-f2-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));try{
 const auto file=root/"settings/F2-favorites.json";auto staged=file;staged+=".pending";
 check(!Read(file),"missing preferences");
 const std::string first=R"({"Version":1,"Favorites":[{"Path":"/Game/Items/A.A","Name":"First Item"}]})";
 Write(file,first,[&](auto path){check(Read(path)==first,"staged bytes read back");});
 check(Read(file)==first&&!fs::exists(staged),"first save committed without staging residue");
 const auto second=std::string(R"({"Version":1,"Favorites":[]})");
 Write(file,second,[&](auto path){check(Read(path)==second,"replacement bytes read back");});
 check(Read(file)==second,"replaces existing preference file");
 rejects([&]{Write(file,first,[&](auto){throw std::runtime_error("injected verify failure");});},"verify failure must abort");
 check(Read(file)==second&&!fs::exists(staged),"failed readback preserves old file");
 put(staged,"other writer");rejects([&]{Write(file,first,[](auto){});},"staging collision fails");check(Read(staged)=="other writer"&&Read(file)==second,"another writer staging untouched");fs::remove(staged);
 put(file,std::string(MaxBytes+1,'x'));rejects([&]{Read(file);},"oversize read rejected");
 put(file,second);rejects([&]{Write(file,std::string(MaxBytes+1,'x'),[](auto){});},"oversize write rejected");check(Read(file)==second,"oversize leaves old bytes");
 Write(file,std::string(MaxBytes,'x'),[&](auto path){check(Read(path)->size()==MaxBytes,"exact byte limit supported");});
 put(file,second);
 const auto target=root/"target.json";put(target,"unrelated");fs::remove(file);fs::create_symlink(target,file);
 rejects([&]{Read(file);},"symlink read refused");rejects([&]{Write(file,first,[](auto){});},"symlink write refused");check(Read(target)=="unrelated","unrelated symlink target untouched");fs::remove(file);
 fs::create_symlink(root/"does-not-exist",file);rejects([&]{Read(file);},"dangling symlink not treated as empty file");fs::remove(file);
 fs::create_directory(file);rejects([&]{Read(file);},"directory read refused");rejects([&]{Write(file,first,[](auto){});},"directory replacement refused");check(fs::is_directory(file)&&!fs::exists(staged),"directory preserved; staging cleanup");fs::remove(file);
 put(file,second);std::atomic<bool> entered=false,release=false;std::exception_ptr err;
 std::thread writer([&]{try{Write(file,first,[&](auto p){if(Read(p)!=first)throw std::runtime_error("bad write");entered=true;while(!release.load())std::this_thread::yield();});}catch(...){err=std::current_exception();entered=true;}});
 while(!entered.load()){std::this_thread::yield();}
 rejects([&]{Write(file,second,[](auto){});},"simultaneous writer denied");release=true;writer.join();if(err)std::rethrow_exception(err);check(Read(file)==first,"first writer retained commit ownership");

 const auto catalog=root/"runtime/F2-catalog-cache.json";
 const auto large=std::string(MaxBytes+4096,'x');
 rejects([&]{Write(catalog,large,[](auto){});},"favorites default size stays bounded");
 Write(catalog,large,[&](auto p){check(Read(p,16*MaxBytes)==large,"catalog readback accepts explicit larger limit");},16*MaxBytes);
 check(Read(catalog,16*MaxBytes)==large,"catalog cache persists beyond favorite limit");
 rejects([&]{Read(catalog);},"default read does not silently widen for catalogs");
 rejects([&]{Write(catalog,std::string(16*MaxBytes+1,'x'),[](auto){},16*MaxBytes);},"catalog maximum enforced");
 check(Read(catalog,16*MaxBytes)==large,"failed oversize catalog preserves previous cache");
 fs::remove_all(root);std::cout<<"PASS: "<<checks<<" actual portable preference-file I/O assertions (POSIX branch).\n";return 0;
}catch(const std::exception& e){fs::remove_all(root);std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
