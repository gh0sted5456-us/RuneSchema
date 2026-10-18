#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
void Check(bool value){if(!value)throw std::runtime_error("Quest scroll routing contract failed");}
std::string Read(const char* path){std::ifstream file(path);if(!file)throw std::runtime_error("Missing quest-scroll contract source");return {std::istreambuf_iterator<char>(file),{}};}
int main(int argc,char** argv){
    Check(argc==5);const auto use=Read(argv[1]),actions=Read(argv[2]),contract=Read(argv[3]),loader=Read(argv[4]);
    const auto block=use.find("dispatch.PreventOriginalFunctionCall()");
    Check(block!=use.npos&&block<use.find("RunQuestAction(binding"));
    Check(use.find("inventory->GetOuterPrivate()!=controller")!=use.npos);
    Check(use.find("ItemDataPersistenceId")!=use.npos&&use.find("bHasLoadedFromSave")!=use.npos);
    Check(use.find("const auto& value=field->GetPropertyValue")!=use.npos);
    Check(use.find("QuestScrolls::ReadIdentity(chars.GetData(),chars.Num(),chars.Max())")!=use.npos);
    Check(use.find("[QUEST-SCROLL][IDENTITY]")!=use.npos&&use.find("storageNum=")!=use.npos&&use.find("boundedChars=")!=use.npos);
    Check(use.find("Item:BP_GetItemData")!=use.npos&&use.find("getData.Result<UObject*>()!=source")!=use.npos);
    Check(use.find("field->HasAnyPropertyFlags(CPF_SaveGame)")!=use.npos&&use.find("field->HasAnyPropertyFlags(CPF_Transient)")!=use.npos);
    Check(use.find("identity(item,TEXT(\"ItemDataPersistenceId\"),true)")<use.find("if(!authority.Result<bool>())"));
    Check(use.find("ItemDataPersistenceId\"),true)")!=use.npos);
    Check(use.find("SetObjectPropertyValue")==use.npos); // No identity repair/rewriting on use.
    Check(use.find("Server_UseItemFromInventory")!=use.npos&&use.find("QuestProgress::OperationGuard")!=use.npos);
    Check(use.find(".Arg(TEXT(\"QuestID\")")==use.npos&&use.find(".Arg(TEXT(\"Reward\")")==use.npos);
    Check(use.find("RunQuestAction(binding,controller,current,source,&accepted)")!=use.npos);
    Check(use.find("binding.QuestAction=contract->Action")!=use.npos);
    Check(use.find("accepted==QuestProgress::Result::Abandoned")!=use.npos);
    Check(actions.find("m_events.CancelQuestContract(quest.Key,player)")!=actions.npos);
    Check(actions.find("write(\"RuneSchema.Run\",completedRuns)")!=actions.npos);
    Check(actions.find("write(\"RuneSchema.StageRun\",completedRuns)")!=actions.npos);
    Check(use.find("accepted==QuestProgress::Result::Accepted")!=use.npos);
    Check(actions.find("std::map<UObject*,int> charges")<actions.find("result=receipt.Accept("));
    Check(actions.find("if(scrollItem)++charges[scrollItem]")!=actions.npos);
    Check(contract.find("EFieldIterationFlags::IncludeSuper")!=contract.npos);
    Check(contract.find("EFieldIterationFlags::IncludeInterfaces")==contract.npos);
    Check(contract.find("function->HasAnyFunctionFlags(FUNC_Native)")!=contract.npos);
    Check(contract.find("function->HasAnyFunctionFlags(FUNC_BlueprintPure)")!=contract.npos);
    Check(loader.find("QuestScrolls::RequireUseEvent(object)")<loader.find("QuestScrolls::Bind("));
}
