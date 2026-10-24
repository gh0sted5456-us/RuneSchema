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
    const auto cleanup=read("include/Loader/NativeQuestCleanup.h"),npc=read("src/Loader/DragonWildsNpcLoader.cpp");
    assert(cleanup.contains("PS::SaveCleanup::IsOrphanQuest(id,registry)")&&!cleanup.contains("unresolved quest exchange")&&!cleanup.contains("unresolved dialogue reward"));
    const auto commit=cleanup.find("beforePrune();");
    assert(commit<cleanup.find("ActorHelper::FunctionCall(component,notify->GetPathName())",commit));
    assert(cleanup.find("ActorHelper::FunctionCall(component,notify->GetPathName())",commit)<cleanup.find("property->CopyCompleteValue(&live,retained.Data)",commit));
    assert(cleanup.contains("Client_OnQuestsUpdated")&&cleanup.contains("CopyCompleteValue(&live,original.Data)"));
    assert(cleanup.contains("QuestCompletedState")&&cleanup.contains("CompletedQuest(")&&cleanup.contains("IdentityKind::Quest"));
    assert(npc.contains("NativeQuestCleanup::Run(controller,*registry")&&npc.contains("SavePruneBackup::Provider")&&npc.contains("BP_OnPlayerControllerStateLoaded"));
    const auto pruner=read("src/Core/PersistencePruner.cpp"),registrar=read("src/Misc/DragonWildsDataRegistrar.cpp"),main=read("src/Loader/DragonWildsMainLoader.cpp");
    assert(pruner.find("SavePruneBackup::Write(")<pruner.find("characterJson=std::move(replacement)"));
    for(const auto& source:{pruner,registrar,main})assert(!source.contains("SaveRecovery::")&&!source.contains("persistence.recovery"));
}
