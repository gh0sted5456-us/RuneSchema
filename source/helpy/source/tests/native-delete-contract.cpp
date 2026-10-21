#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
static std::string Read(const std::filesystem::path& path){std::ifstream input(path);if(!input)throw std::runtime_error("Missing inventory contract input");return {std::istreambuf_iterator<char>(input),{}};}
static void Check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main(int argc,char**argv){
    Check(argc==2,"Expected source directory");const auto root=std::filesystem::path(argv[1]);
    const auto observer=Read(root/"raw/include/SDK/Helper/NativeDeleteObserver.h");
    Check(observer.find("PrepareDelete")!=observer.npos&&observer.find("ObserveDelete")!=observer.npos,"Delete boundaries must bracket native deletion");
    for(const auto* forbidden:{"PreventOriginalFunctionCall","SetReturnValue","RemoveFromSlot"})
        Check(observer.find(forbidden)==observer.npos,"Delete observer must never cancel or invoke deletion");
    Check(observer.find("Server_DestroyItemFromInventory")!=observer.npos&&observer.find("InventoryUIAPI:HandleDestroyItem")!=observer.npos,"Missing native Delete boundary");
    Check(observer.find("GetCurrentThreadId()!=Thread")!=observer.npos&&observer.find("Identify(controller)==capture->Character")!=observer.npos,"Delete outcomes must retain thread/character ownership");
    const auto serializer=Read(root/"raw/include/SDK/Helper/NativeItemState.h");
    Check(serializer.find("ExportText_InContainer")==serializer.npos,"Null-delta container exporter is unsafe");
    Check(serializer.find("CPF_SaveGame")!=serializer.npos&&serializer.find("ApplyNumericBits")!=serializer.npos&&serializer.find("ReferencingInventoriesCount")!=serializer.npos,"Recovery must preserve persistent numeric state without rewriting owned items");
    Check(serializer.find("path.starts_with(\"/Engine/Transient\")")!=serializer.npos,"Recovery must not serialize transient runtime references");
    const auto panel=Read(root/"helpy/source/include/Generator/HelpyStoragePanel.h");
    Check(panel.find("DrawPlacard")!=panel.npos&&panel.find("std::async")!=panel.npos&&panel.find("LazyCatalog::Search")==panel.npos,"Storage must reuse placards and bounded local data, not launch a catalog search");
    const auto scrap=Read(root/"raw/include/SDK/Helper/PlayerInventoryScrap.h");
    const auto commit=scrap.substr(scrap.find("inline Json Commit("));
    Check(commit.find("request.at(\"ExpectedPlan\")!=plan")<commit.find("\"BeginScrap\""),"Scrap must revalidate the preview before any deletion intent");
    Check(commit.find("quantity!=plan.at(\"ExpectedCount\")")<commit.find("\"BeginScrap\""),"Native deletion must consume only a complete explicitly selected stack");
    Check(commit.find("\"BeginScrap\"")<commit.find("remove.Invoke()")&&commit.find("\"ObserveScrap\"")<commit.find("refund.Call->Invoke()"),"Refunds require a durable intent and observed native removal");
    Check(commit.find("NativeDeleteObserver::ScrapScope")!=commit.npos&&commit.find("\"FinishScrap\"")!=commit.npos&&commit.find("SpawnAndLaunchItem_Sync")!=commit.npos,"Scrap must use native deletion/drop receipts without a recoverable copy");
}
