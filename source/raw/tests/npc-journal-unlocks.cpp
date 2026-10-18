#include "Loader/NpcCatalog.h"
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <iostream>
#include <filesystem>
using nlohmann::json;
static void Require(bool value){if(!value)throw std::runtime_error("NPC journal unlock regression");}
static std::string Read(const char* path){std::ifstream file(path);Require(file.good());return {std::istreambuf_iterator<char>(file),{}};}
template<class F>static void Reject(F operation){bool rejected=false;try{operation();}catch(...){rejected=true;}Require(rejected);}
int Run(int argc,char** argv){
    DragonWilds::NpcCatalog catalog;
    json npc={{"Id","trader"},{"DisplayName","Trader"},{"Type","Prop"},{"Mesh","/Game/Props/Test.Test"},
        {"Location",{1,2,3}},{"JournalEntries",{"people_trader","Other:lore_page","/MyMod/Journal/ENTRY.ENTRY"}}};
    catalog.AddNpc("Dye",npc);const auto resolved=catalog.Resolve();Require(resolved.size()==1);
    Require(resolved[0].Data.at("JournalEntries")==json::array({"Dye:people_trader","Other:lore_page","/MyMod/Journal/ENTRY.ENTRY"}));
    Require(resolved[0].Data.at("Stage")=="Interaction");
    npc["NoInteract"]=true;DragonWilds::NpcCatalog disabled;disabled.AddNpc("Dye",npc);Require(!disabled.Resolve()[0].Data.contains("JournalEntries"));
    npc.erase("NoInteract");
    for(const auto invalid:{json::array(),json::array({"same","same"}),json::array({42}),json::array({"bad\nref"})}){npc["JournalEntries"]=invalid;Reject([&]{DragonWilds::NpcCatalog bad;bad.AddNpc("Dye",npc);});}
    Require(argc==4);const auto loader=Read(argv[1]),journal=Read(argv[2]),main=Read(argv[3]);
    Require(loader.find("findOwned(definition->InteractionClass) != source")<loader.find("DeliverAutomaticJournals(interactingController)"));
    Require(loader.find("UnlockJournal(interactingController,entry)")!=std::string::npos);
    Require(journal.find("JournalPlayerAccess::EnsureUnlocked(component,entry)")!=std::string::npos);
    Require(journal.find("m_acquisitionUnlocks.empty()&&m_unlock.empty()")!=std::string::npos);
    Require(journal.find("DeliverAutomaticUnlocks(controller)")!=std::string::npos);
    Require(main.find("npcService->UnlockJournal=")!=std::string::npos);
    Require(journal.find("EntryHandle(acquisitionItem,false)")!=std::string::npos);
    Require(journal.find("m_rejectedEntries.contains(binding.EntryKey)")!=std::string::npos);
    const auto placed=journal.find("Place(entry, def)"),binding=journal.find("m_acquisitionUnlocks.push_back({EntryHandle(acquisitionItem,false)");
    Require(placed!=std::string::npos&&binding!=std::string::npos&&placed<binding);
    const auto lore=Read((std::filesystem::path(argv[2]).parent_path()/"JournalLorePresentation.inl").string().c_str());
    const auto access=Read((std::filesystem::path(argv[2]).parent_path().parent_path().parent_path()/"include/Loader/JournalPlayerAccess.h").string().c_str());
    const auto popup=lore.find("SoftObjectArg(TEXT(\"Entry\"),entry).Invoke()"),read=lore.find("JournalPlayerAccess::MarkRead(journal,entry,markRead)");
    Require(popup!=std::string::npos&&read!=std::string::npos&&popup<read);
    Require(lore.find("JournalPlayerAccess::ReadFunction(entry)")<popup);
    Require(access.find("/Script/Dominion.JournalComponent:MarkJournalEntryRead")!=std::string::npos);
    Require(access.find("ReflectedContract::Count(fn)!=1")!=std::string::npos);
    Require(access.find("JournalComponent:Server_MarkJournalEntryRead")==std::string::npos);
    return 0;
}
int main(int argc,char** argv){try{return Run(argc,argv);}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
