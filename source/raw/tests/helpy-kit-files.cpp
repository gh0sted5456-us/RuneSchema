#include "Core/HelpyKitFiles.h"
#include <cassert>
#include <chrono>
#include <fstream>
template<class F>static bool Rejects(F run){try{run();}catch(...){return true;}return false;}
int main(){
    namespace fs=std::filesystem;namespace Kits=PS::HelpyKitFiles;namespace Cart=PS::ItemCart;
    const auto root=fs::temp_directory_path()/("RuneSchema-kits-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    assert(Kits::List(root).empty());
    for(const auto* bad:{"", "../kit", "CON", "LPT1", "kit.json", " kit", "kit ", "a:b"})assert(!Kits::FileName(bad));
    assert(Kits::FileName("Starter armor-1"));
    Cart::Model cart;assert(cart.Add({{"Path","/Game/Items/A.A"},{"Name","A"}}));cart.SetQuantity("/Game/Items/A.A",3);
    const auto file=Kits::Write(root,"Starter",true,cart);assert(fs::exists(file));assert(Kits::List(root).size()==1);
    const auto doc=Kits::Read(root,"Starter.jsonc");assert(Cart::KitItems(doc)==cart.Payload());
    assert(Rejects([&]{(void)Kits::Write(root,"Starter",true,cart);}));
    assert(Rejects([&]{(void)Kits::Read(root,"../Starter.jsonc");}));
    Cart::Model imported;imported.ImportKit(doc);assert(imported.Payload()==cart.Payload());assert(!imported.Delivery());
    auto bad=doc;bad["Items"][0]["Item"]="/Game/../Unsafe.Unsafe";
    assert(Rejects([&]{imported.ImportKit(bad);}));assert(imported.Payload()==cart.Payload());
    assert(Rejects([&]{imported.ImportKit(doc,[](const std::string&)->nlohmann::json{throw std::runtime_error("metadata failed");});}));assert(imported.Payload()==cart.Payload());
    imported.ImportKit(doc,[](const std::string& path){return nlohmann::json{{"Path",path},{"Icon","/Game/Icons/A.A"},{"Masterwork",true}};});
    assert(imported.Entries()[0].Row["Masterwork"]==true&&imported.Entries()[0].Row["Name"]=="A");
    imported.RefreshMetadata(nlohmann::json::array({{{"Path","/Game/Items/A.A"},{"Name","Native A"},{"Icon","/Game/Icons/A.A"},{"PowerLevel",30}}}));
    assert(imported.Entries()[0].Quantity==3&&imported.Entries()[0].Row["PowerLevel"]==30);
    assert(imported.BeginDelivery(1));assert(Rejects([&]{imported.ImportKit(doc);}));imported.CancelDelivery();
    cart.SetQuantity("/Game/Items/A.A",4);(void)Kits::Write(root,"Starter",true,cart,true);
    assert(Kits::Read(root,"Starter.jsonc")["Items"][0]["Count"]==4);
    std::size_t backups=0;for(const auto& entry:fs::directory_iterator(root))if(entry.path().extension()==".bak")++backups;assert(backups==1);
    PS::ConfigFiles::Write(root/"Malformed.json","{");assert(Rejects([&]{(void)Kits::Read(root,"Malformed.json");}));
    PS::ConfigFiles::Write(root/"Oversized.json",std::string(Kits::MaxBytes+1,' '));assert(Rejects([&]{(void)Kits::Read(root,"Oversized.json");}));
    std::error_code error;fs::create_symlink(file,root/"Linked.jsonc",error);if(!error)assert(Rejects([&]{(void)Kits::Read(root,"Linked.jsonc");}));
    // Exact private fixture, not a user save or broad temporary directory.
    fs::remove_all(root);
}
