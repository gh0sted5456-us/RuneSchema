#include "Runtime/Layout.h"
#include "Core/ConfigFiles.h"
#include <cassert>
#include <chrono>
int main(){
    namespace fs=std::filesystem;using namespace PS;
    const auto root=fs::temp_directory_path()/("runeschema-layout-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto old=root/"game/RuneSchema",runtime=root/"appdata/RSDragonwilds/RuneSchema/steam/runtime";
    ConfigFiles::Write(old/"runtime/live/saved/cache/old.bin","legacy cache");
    ConfigFiles::Write(old/"runtime/live/saved/progress/old.json","legacy progress");
    ConfigFiles::Write(old/"settings/Helpy.json","legacy settings");
    RuntimeLayout::Initialize(runtime);
    assert(fs::is_directory(runtime/"saved/progress")&&fs::is_directory(runtime/"saved/references"));
    assert(!fs::exists(runtime/"saved/cache")&&!fs::exists(runtime/"saved/progress/old.json"));
    assert(!fs::exists(runtime/"saved/references/layout-migration-v2.json"));
    assert(ConfigFiles::Read(old/"runtime/live/saved/cache/old.bin")=="legacy cache");
    assert(ConfigFiles::Read(old/"runtime/live/saved/progress/old.json")=="legacy progress");
    assert(ConfigFiles::Read(old/"settings/Helpy.json")=="legacy settings");
    ConfigFiles::Write(runtime/"saved/progress/keep.json","current progress");
    RuntimeLayout::Initialize(runtime);
    assert(ConfigFiles::Read(runtime/"saved/progress/keep.json")=="current progress");
    fs::remove(runtime/"saved/progress/keep.json");fs::remove(runtime/"saved/progress");fs::remove(runtime/"saved/references");fs::remove(runtime/"saved");
    RuntimeLayout::Initialize(runtime);
    assert(fs::is_directory(runtime/"saved/progress")&&fs::is_directory(runtime/"saved/references"));
    assert(!fs::exists(runtime/"saved/cache")&&fs::exists(old/"runtime/live/saved/cache/old.bin"));
    assert(root.filename().string().starts_with("runeschema-layout-test-"));fs::remove_all(root);
}
