#include "Runtime/PluginCatalog.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("Plugin source root required");
    const std::filesystem::path root=argv[1];const auto plugins=PS::PluginCatalog::Discover(root);
    const auto found=std::find_if(plugins.begin(),plugins.end(),[](const auto& p){return p.Id=="RuneSchema.QuestItems";});
    if(found==plugins.end()||!found->Enabled||!found->MountPaks||!found->EntryPoint.empty())throw std::runtime_error("Included quest-scroll content plugin is missing/disabled or unexpectedly native");
    const auto folders=PS::PluginCatalog::PakDirectories(*found);if(folders.size()!=1)throw std::runtime_error("One complete shared quest-items package is required");
    for(const auto* extension:{".pak",".ucas",".utoc"}){const auto file=folders.front()/(std::string("RuneSchemaQuestItems_P")+extension);if(!std::filesystem::is_regular_file(file)||!std::filesystem::file_size(file))throw std::runtime_error("Shared quest-scroll container is incomplete");}
    std::ifstream input(found->Root/"plugin.json");const auto data=nlohmann::json::parse(input);
    if(data.at("CookedObjects")!=nlohmann::json::array({"/RuneSchemaQuestItems/Blueprints/BP_QuestScroll","/RuneSchemaQuestItems/Items/ITEM_QuestScroll_Template"}))throw std::runtime_error("Shared class/template announcement changed");
    for(const auto* platform:{"Steam","GamePass"}){
        const auto evidence=root.parent_path()/"content"/"RuneSchemaQuestItems"/"validation"/platform;
        std::ifstream classFile(evidence/"BP_QuestScroll.json");const auto exports=nlohmann::json::parse(classFile);
        const auto event=std::find_if(exports.begin(),exports.end(),[](const auto& row){return row.value("Type",std::string{})=="Function"&&row.value("Name",std::string{})=="UseItem";});
        if(event==exports.end())throw std::runtime_error("Decoded shared UseItem is absent");
        const auto flags=event->at("FunctionFlags").get<std::string>();
        if(flags.find("FUNC_Const")==flags.npos||flags.find("FUNC_Native")!=flags.npos||flags.find("FUNC_BlueprintPure")!=flags.npos)throw std::runtime_error("Decoded shared UseItem must be scripted/const/nonpure");
        const auto& parameters=event->at("ChildProperties");
        if(parameters.size()!=4||parameters[0]["Name"]!="PlayerController"||parameters[1]["Name"]!="Inventory"||parameters[2]["Name"]!="SlotIndex"||parameters[3]["Name"]!="ReturnValue")throw std::runtime_error("Decoded shared UseItem parameters changed");
        std::ifstream auditFile(evidence/"NativeProbeAudit.json");const auto audit=nlohmann::json::parse(auditFile);
        if(audit.at("expected")!=2||audit.at("verified")!=2||!audit.at("errors").empty())throw std::runtime_error("Shared package decode audit did not pass");
    }
    std::cout<<"Included content-only quest-scroll plugin and complete container verified\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
