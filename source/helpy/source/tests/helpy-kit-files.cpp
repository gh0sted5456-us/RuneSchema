#include "Core/HelpyKitFiles.h"
#include "Check.h"
#include <chrono>
#include <fstream>
template<class F>static bool Rejects(F run){try{run();}catch(...){return true;}return false;}
void RunChecks(){
    namespace fs=std::filesystem;namespace Kits=PS::HelpyKitFiles;namespace Cart=PS::ItemCart;
    const auto root=fs::temp_directory_path()/("RuneSchema-kits-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    CHECK(Kits::List(root).empty());
    for(const auto* bad:{"", "../kit", "CON", "LPT1", "kit.json", " kit", "kit ", "a:b"})CHECK(!Kits::FileName(bad));
    CHECK(Kits::FileName("Starter armor-1"));
    Cart::Model cart;CHECK(cart.Add({{"Path","/Game/Items/A.A"},{"Name","A"}}));cart.SetQuantity("/Game/Items/A.A",3);
    const auto file=Kits::Write(root,"Starter",true,cart);CHECK(fs::exists(file));CHECK(Kits::List(root).size()==1);
    const auto doc=Kits::Read(root,"Starter.jsonc");CHECK(Cart::KitItems(doc)==cart.Payload());
    CHECK(Rejects([&]{(void)Kits::Write(root,"Starter",true,cart);}));
    CHECK(Rejects([&]{(void)Kits::Read(root,"../Starter.jsonc");}));
    Cart::Model imported;imported.ImportKit(doc);CHECK(imported.Payload()==cart.Payload());CHECK(!imported.Delivery());
    auto bad=doc;bad["Items"][0]["Item"]="/Game/../Unsafe.Unsafe";
    CHECK(Rejects([&]{imported.ImportKit(bad);}));CHECK(imported.Payload()==cart.Payload());
    CHECK(Rejects([&]{imported.ImportKit(doc,[](const std::string&)->nlohmann::json{throw std::runtime_error("metadata failed");});}));CHECK(imported.Payload()==cart.Payload());
    imported.ImportKit(doc,[](const std::string& path){return nlohmann::json{{"Path",path},{"Icon","/Game/Icons/A.A"},{"Masterwork",true}};});
    CHECK(imported.Entries()[0].Row["Masterwork"]==true&&imported.Entries()[0].Row["Name"]=="A");
    imported.RefreshMetadata(nlohmann::json::array({{{"Path","/Game/Items/A.A"},{"Name","Native A"},{"Icon","/Game/Icons/A.A"},{"PowerLevel",30}}}));
    CHECK(imported.Entries()[0].Quantity==3&&imported.Entries()[0].Row["PowerLevel"]==30);
    CHECK(imported.BeginDelivery(1));CHECK(Rejects([&]{imported.ImportKit(doc);}));imported.CancelDelivery();
    cart.SetQuantity("/Game/Items/A.A",4);(void)Kits::Write(root,"Starter",true,cart,true);
    CHECK(Kits::Read(root,"Starter.jsonc")["Items"][0]["Count"]==4);
    std::size_t backups=0;for(const auto& entry:fs::directory_iterator(root))if(entry.path().extension()==".bak")++backups;CHECK(backups==1);
    PS::ConfigFiles::Write(root/"Malformed.json","{");CHECK(Rejects([&]{(void)Kits::Read(root,"Malformed.json");}));
    PS::ConfigFiles::Write(root/"Oversized.json",std::string(Kits::MaxBytes+1,' '));CHECK(Rejects([&]{(void)Kits::Read(root,"Oversized.json");}));
    CHECK(cart.Add({{"Path","/Game/Items/B.B"}}));CHECK(cart.Select("/Game/Items/A.A"));
    (void)Kits::Write(root,"Selection",false,cart,false,true);
    const auto selectedDoc=Kits::Read(root,"Selection.json");
    CHECK(selectedDoc["Items"].size()==1&&selectedDoc["Items"][0]["Item"]=="/Game/Items/A.A");
    imported.ImportKits({selectedDoc,selectedDoc});CHECK(imported.Payload()[0]["Count"]==8);
    std::error_code error;fs::create_symlink(file,root/"Linked.jsonc",error);if(!error)CHECK(Rejects([&]{(void)Kits::Read(root,"Linked.jsonc");}));
    // Exact private fixture, not a user save or broad temporary directory.
    CHECK(fs::weakly_canonical(root).parent_path()==fs::weakly_canonical(fs::temp_directory_path()));
    fs::remove_all(root);
}

int main(){try{RunChecks();return 0;}catch(const std::exception& e){std::fprintf(stderr,"Unexpected exception: %s\n",e.what());return 2;}}
