// Item Bay delivery status: mirrors the core's delivery gate before the player clicks Give.
#include "Generator/DeliveryStatus.h"
#include <cassert>
#include <fstream>
#include <iostream>
namespace {
namespace fs=std::filesystem;using Json=nlohmann::json;
struct Row{std::string Statement,Sig;};
// Signed fixtures shared with helpy-delivery-flags.cpp (copied verbatim).
const std::string KeyA="3059301306072a8648ce3d020106082a8648ce3d030107034200048c4a837cb567e4883fcc518ed4e9229caea10c8ede6d0830f8a978ba86f706247bd3cddb04fd341f290fb488480f8459982b86298d1914cb99584004446a50e6",KeyB="3059301306072a8648ce3d020106082a8648ce3d03010703420004f77f4c3c67be0834c8ba25d24157c68da61cb9aa1c4b634ceef8af33bf5063ad9923b66992f3f1aeb020dfe4376c9b0573779ed6caa2fc627d73c955fed2a7bb";
const Row charOn{"helpy-flag/1\n{\"mod\":\"examplemod\",\"target\":\"character\",\"id\":\"5A3F0C21B6D84E7F9A1C2B3D4E5F6071\",\"flag\":\"noPlayerDeliveries\",\"on\":true,\"seq\":3,\"at\":1800000000,\"message\":\"This character trades on the Grand Exchange.\"}","3044022050845734cf11753403b7bc88800445c41f7bccb46b9e4b141a4c567bda3d2411022070b15b9e17f96394934de3ad7a2d1bc885841c4ca5f10c8d68146da3c01c5d3b"};
const Row worldOn{"helpy-flag/1\n{\"mod\":\"examplemod\",\"target\":\"world\",\"id\":\"00112233445566778899AABBCCDDEEFF\",\"flag\":\"noPlayerDeliveries\",\"on\":true,\"seq\":1,\"at\":1800000000}","304502204c318117efd6541364d8f68f2bb59e55c19947a35eccb8a5e7a41b77e447fdd7022100cfc3dd38a715bd7bfeadbba6a325d0df41ca9e16d624a9cfd3cf356ce998ab26"};
const std::string C="5A3F0C21B6D84E7F9A1C2B3D4E5F6071",Other="ABCDEFABCDEFABCDEFABCDEFABCDEF01";
void Put(const fs::path& path,const std::string& text){fs::create_directories(path.parent_path());std::ofstream(path,std::ios::binary)<<text;}
void Mod(const fs::path& folder,const std::vector<Row>& rows){
    Put(folder/"examplemod"/"mod.json",Json{{"id","examplemod"},{"name","Example Mod"},{"publicKey",KeyA}}.dump());
    Json flags=Json::array();for(const auto& row:rows)flags.push_back({{"statement",row.Statement},{"sig",row.Sig}});
    Put(folder/"examplemod"/"flags.json",Json{{"flags",flags}}.dump());
}
}
int main(){
    namespace D=PS::DeliveryStatus;
    const auto folder=fs::temp_directory_path()/("helpy-delivery-status-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(folder);D::FolderOverride=folder;D::TimeOverride=1800000100;

    // Not the player's own character: never gated, whatever the flags say.
    D::Refresh(true);
    auto status=D::For(Other,C,true);assert(!status.Own&&!status.Block);
    status=D::For(C,"",true);assert(!status.Own&&!status.Block);
    // Own character, nothing set: deliveries go through.
    status=D::For(C,C,true);assert(status.Own&&!status.Block&&status.WorldMods.empty());
    // GUID spelling does not matter: braces, dashes and case are normalized like the core does.
    status=D::For("{5a3f0c21-b6d8-4e7f-9a1c-2b3d4e5f6071}",C,true);assert(status.Own);

    // The player's own switch: refused, explained, and lifted in place.
    PS::HelpyDeliveryFlags::SetUserDeliveries(folder,C,false,D::Now(),"Aria");
    status=D::For(C,C,true);assert(status.Block&&status.Block->User);
    assert(D::Refusal(*status.Block,"Aria").find("off for Aria")!=std::string::npos);
    std::string message;assert(D::TurnOn(C,"Aria",message)&&message.find("on for Aria")!=std::string::npos);
    status=D::For(C,C,true);assert(!status.Block);

    // A trusted mod's character pause: refused with the mod's own message; the player cannot lift it here.
    Mod(folder,{charOn});
    status=D::For(C,C,true);assert(status.Block&&!status.Block->User&&status.Block->ModName=="Example Mod");
    assert(D::Refusal(*status.Block,"Aria").find("Only Example Mod can lift this")!=std::string::npos);
    // A world pause never blocks here (the loaded world is the core's to know) but is reported.
    Mod(folder,{worldOn});
    status=D::For(C,C,true);assert(!status.Block&&status.WorldMods==std::vector<std::string>{"Example Mod"});

    // Throttled reads: without force, a change shows on the next read window, not instantly.
    D::Refresh(true);PS::HelpyDeliveryFlags::SetUserDeliveries(folder,C,false,D::Now(),"Aria");
    assert(!D::For(C,C,false).Block);assert(D::For(C,C,true).Block);

    fs::remove_all(folder);
    std::cout<<"Item Bay delivery status matches the core gate for own-character, mod and world pauses.\n";
    return 0;
}
