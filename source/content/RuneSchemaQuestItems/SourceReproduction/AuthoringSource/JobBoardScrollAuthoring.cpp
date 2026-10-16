#include "JobBoardScrollAuthoring.h"
#include "ConsumableItemData.h"
#include "JobBoardUsableItemInterface.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

bool UJobBoardScrollAuthoring::AuthorScrollBlueprint()
{
    constexpr const TCHAR* Path = TEXT("/RuneSchemaQuestItems/Blueprints/BP_QuestScroll");
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, Path);
    if (!Blueprint)
    {
        UPackage* Package = CreatePackage(Path);
        Blueprint = FKismetEditorUtilities::CreateBlueprint(UConsumableItemData::StaticClass(), Package,
            TEXT("BP_QuestScroll"), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(),
            TEXT("JobBoardScrollAuthoring"));
        if (!Blueprint) return false;
        FAssetRegistryModule::AssetCreated(Blueprint);
    }
    const auto InterfacePath = UUsableItemInterface::StaticClass()->GetClassPathName();
    bool HasInterface = false;
    for (const FBPInterfaceDescription& Interface : Blueprint->ImplementedInterfaces)
        HasInterface |= Interface.Interface == UUsableItemInterface::StaticClass();
    UFunction* Existing = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->FindFunctionByName(TEXT("UseItem")) : nullptr;
    if (HasInterface && Existing && Existing->GetOuter() == Blueprint->GeneratedClass
        && Existing->HasAnyFunctionFlags(FUNC_BlueprintPure))
    {
        FBlueprintEditorUtils::RemoveInterface(Blueprint, InterfacePath, false);
        HasInterface = false;
    }
    if (!HasInterface && !FBlueprintEditorUtils::ImplementNewInterface(Blueprint, InterfacePath)) return false;
    UEdGraph* UseGraph = nullptr;
    for (const FBPInterfaceDescription& Interface : Blueprint->ImplementedInterfaces)
        for (UEdGraph* Graph : Interface.Graphs)
            if (Graph && Graph->GetFName() == TEXT("UseItem")) UseGraph = Graph;
    if (!UseGraph) return false;
    int32 Results = 0;
    UK2Node_FunctionEntry* EntryNode = nullptr;
    UK2Node_FunctionResult* ResultNode = nullptr;
    for (UEdGraphNode* Node : UseGraph->Nodes)
    {
        if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node)) EntryNode = Entry;
        if (UK2Node_FunctionResult* Result = Cast<UK2Node_FunctionResult>(Node))
        {
            ResultNode = Result;
            UEdGraphPin* Pin = Result->FindPin(TEXT("ReturnValue"));
            if (!Pin || !Pin->LinkedTo.IsEmpty()) return false;
            Pin->DefaultValue = TEXT("false");
            ++Results;
        }
    }
    if (Results != 1 || UseGraph->Nodes.Num() != 2 || !EntryNode || !ResultNode) return false;
    EntryNode->ClearExtraFlags(FUNC_BlueprintPure);
    EntryNode->ReconstructNode();
    ResultNode->ReconstructNode();
    UEdGraphPin* ExecOut = EntryNode->FindPin(UEdGraphSchema_K2::PN_Then);
    UEdGraphPin* ExecIn = ResultNode->FindPin(UEdGraphSchema_K2::PN_Execute);
    if (!ExecOut || !ExecIn || !UseGraph->GetSchema()->TryCreateConnection(ExecOut, ExecIn)) return false;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
    if (Blueprint->Status == BS_Error || !Blueprint->GeneratedClass) return false;
    UFunction* Function = Blueprint->GeneratedClass->FindFunctionByName(TEXT("UseItem"));
    if (!Function || Function->GetOuter() != Blueprint->GeneratedClass || Function->Script.IsEmpty()
        || Function->HasAnyFunctionFlags(FUNC_Native | FUNC_BlueprintPure) || !Function->HasAnyFunctionFlags(FUNC_Const)) return false;
    // An isolated editor CDO call verifies the fallback; no inventory/world exists here.
    if (IUsableItemInterface::Execute_UseItem(Blueprint->GeneratedClass->GetDefaultObject(), nullptr, nullptr, 0)) return false;
    Blueprint->MarkPackageDirty();
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    const FString Filename = FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension());
    if (!UPackage::SavePackage(Blueprint->GetOutermost(), Blueprint, *Filename, SaveArgs)) return false;
    UE_LOG(LogTemp, Display, TEXT("JOBBOARD_INERT_SCROLL_BP_AUTHORED parameters=%d frame=%d script=%d"),
        Function->NumParms, Function->ParmsSize, Function->Script.Num());
    return true;
}
