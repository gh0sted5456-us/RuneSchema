#include <filesystem>
#include <fstream>
#include <iterator>
#include <cassert>
#include <string>
int main(int argc,char** argv){
    assert(argc==2);const auto root=std::filesystem::path(argv[1]);
    const auto read=[&](const char* path){std::ifstream file(root/path);assert(file.good());return std::string(std::istreambuf_iterator<char>(file),{});};
    const auto adapter=read("include/Loader/QuestNativeAdapter.h");
    const auto begin=adapter.find("void NormalizeOwnership("),end=adapter.find("void SetGiven(",begin);
    assert(begin!=adapter.npos&&end!=adapter.npos);const auto method=adapter.substr(begin,end-begin);
    for(const auto* token:{"HasAuthority","InternalName","PersistenceID","QuestIntName","QuestIntValue","CopyCompleteValue(live,old.Data)","read(live)!=planned","RuneSchema_Quest_"})assert(method.contains(token));
    for(const auto* token:{"ChangeRestartState","SetObjective(","ResetForDefinitionChange","Empty()"})assert(!method.contains(token));
    const auto receipt=read("include/Loader/NativeQuestReceipt.h");assert(receipt.contains("api.NormalizeOwnership(")&&receipt.contains("definition.PersistenceId,definition.Key"));
    const auto loader=read("include/Loader/DragonWildsQuestLoader.h");assert(loader.contains("ParseJsonFilesInPathWithSource")&&loader.contains("source.string()"));
    const auto service=read("include/Loader/QuestService.h");assert(service.contains("if(declarationConflict)throw"));
}
