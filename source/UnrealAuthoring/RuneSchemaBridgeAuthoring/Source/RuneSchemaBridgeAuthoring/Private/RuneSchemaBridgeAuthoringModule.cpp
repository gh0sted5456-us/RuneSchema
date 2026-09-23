#include "Modules/ModuleManager.h"
#include "Containers/Ticker.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CustomEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Components/ActorComponent.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuneSchemaBridgeAuthoring, Log, All);

namespace
{
constexpr TCHAR RegistryPath[] = TEXT("/RuneSchema/Networking/BPC_RuneSchemaRegistryBridge.BPC_RuneSchemaRegistryBridge");
constexpr TCHAR IdentityPath[] = TEXT("/RuneSchema/Networking/BPC_RuneSchemaIdentity.BPC_RuneSchemaIdentity");
constexpr TCHAR WorldPath[] = TEXT("/RuneSchema/Networking/BPC_RuneSchemaWorldBridge.BPC_RuneSchemaWorldBridge");
constexpr TCHAR PluginConnectionPath[] = TEXT("/RuneSchema/Networking/Extensions/BPC_RuneSchemaPluginConnection.BPC_RuneSchemaPluginConnection");
constexpr TCHAR PluginStatePath[] = TEXT("/RuneSchema/Networking/Extensions/BPC_RuneSchemaPluginState.BPC_RuneSchemaPluginState");
constexpr TCHAR PluginPresentationPath[] = TEXT("/RuneSchema/Networking/Extensions/BPC_RuneSchemaPluginPresentation.BPC_RuneSchemaPluginPresentation");
struct PinSpec { const TCHAR* Name; FName Category; };
struct EventSpec { const TCHAR* Name; EFunctionFlags Flags; TArray<PinSpec> Pins; int32 Y; };

bool HasEvent(const UEdGraph* Graph, const FName Name)
{
    if (!Graph) return false;
    for (const UEdGraphNode* Node : Graph->Nodes)
        if (const UK2Node_CustomEvent* Event = Cast<UK2Node_CustomEvent>(Node);
            Event && Event->CustomFunctionName == Name) return true;
    return false;
}

bool AddEvent(UEdGraph* Graph, const EventSpec& Spec)
{
    if (HasEvent(Graph, FName(Spec.Name))) return true;
    UK2Node_CustomEvent* Event = NewObject<UK2Node_CustomEvent>(Graph);
    Event->CustomFunctionName = FName(Spec.Name);
    Event->FunctionFlags = Spec.Flags;
    Event->bIsEditable = true;
    Event->SetFlags(RF_Transactional);
    Event->NodePosX = 0;
    Event->NodePosY = Spec.Y;
    Graph->Modify();
    Graph->AddNode(Event, true, false);
    Event->CreateNewGuid();
    Event->PostPlacedNewNode();
    Event->AllocateDefaultPins();
    for (const PinSpec& Pin : Spec.Pins)
    {
        FEdGraphPinType Type;
        Type.PinCategory = Pin.Category;
        if (!Event->CreateUserDefinedPin(Pin.Name, Type, EGPD_Output)) return false;
    }
    return true;
}

bool EnsureVariable(UBlueprint* Blueprint, const TCHAR* Name, const FName Category,
    const EPropertyFlags Flags, const TCHAR* RepNotify = nullptr)
{
    const FName VariableName(Name);
    FBPVariableDescription* Existing = Blueprint->NewVariables.FindByPredicate(
        [&](const FBPVariableDescription& Value){ return Value.VarName == VariableName; });
    if (!Existing)
    {
        FEdGraphPinType Type; Type.PinCategory = Category;
        if (!FBlueprintEditorUtils::AddMemberVariable(Blueprint, VariableName, Type)) return false;
        Existing = Blueprint->NewVariables.FindByPredicate(
            [&](const FBPVariableDescription& Value){ return Value.VarName == VariableName; });
    }
    if (!Existing) return false;
    Existing->PropertyFlags |= Flags;
    if (RepNotify && *RepNotify)
    {
        Existing->PropertyFlags |= CPF_RepNotify;
        Existing->RepNotifyFunc = FName(RepNotify);
    }
    return true;
}

UBlueprint* EnsureComponentBlueprint(const TCHAR* ObjectPath, const TCHAR* PackagePath, const TCHAR* Name)
{
    if (UBlueprint* Existing = LoadObject<UBlueprint>(nullptr, ObjectPath)) return Existing;
    UPackage* Package = CreatePackage(PackagePath);
    if (!Package) return nullptr;
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(UActorComponent::StaticClass(), Package,
        Name, BPTYPE_Normal, UBlueprint::StaticClass(),
        UBlueprintGeneratedClass::StaticClass(), TEXT("RuneSchemaBridgeAuthoring"));
    if (Blueprint) FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    return Blueprint;
}

bool MigrateCoreAssets()
{
    UEditorAssetSubsystem* Assets = GEditor ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
    if (!Assets) return false;

    struct FDirectoryMove { const TCHAR* Source; const TCHAR* Destination; };
    const FDirectoryMove Moves[] = {
        {TEXT("/Game/RuneSchema/Networking"), TEXT("/RuneSchema/Networking")},
        {TEXT("/Game/RuneSchema/UI"), TEXT("/RuneSchema/UI")}
    };
    for (const FDirectoryMove& Move : Moves)
    {
        if (!Assets->DoesDirectoryExist(Move.Source)) continue;
        if (Assets->DoesDirectoryExist(Move.Destination))
        {
            UE_LOG(LogRuneSchemaBridgeAuthoring, Error,
                TEXT("Refusing ambiguous RuneSchema migration; both %s and %s exist."), Move.Source, Move.Destination);
            return false;
        }
        if (!Assets->RenameDirectory(Move.Source, Move.Destination))
        {
            UE_LOG(LogRuneSchemaBridgeAuthoring, Error, TEXT("Could not migrate %s to %s."), Move.Source, Move.Destination);
            return false;
        }
        UE_LOG(LogRuneSchemaBridgeAuthoring, Display, TEXT("Migrated %s to %s."), Move.Source, Move.Destination);
    }
    return true;
}

bool AuthorBlueprint(const TCHAR* Path, const TArray<EventSpec>& Specs)
{
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, Path);
    if (!Blueprint) { UE_LOG(LogRuneSchemaBridgeAuthoring, Error, TEXT("Bridge Blueprint missing: %s"), Path); return false; }
    UEdGraph* Graph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
    if (!Graph) { UE_LOG(LogRuneSchemaBridgeAuthoring, Error, TEXT("Bridge event graph missing: %s"), Path); return false; }
    bool Changed = false;
    for (const EventSpec& Spec : Specs)
    {
        const bool Existed = HasEvent(Graph, FName(Spec.Name));
        if (!AddEvent(Graph, Spec)) { UE_LOG(LogRuneSchemaBridgeAuthoring, Error, TEXT("Could not author %s"), Spec.Name); return false; }
        Changed |= !Existed;
    }
    if (Changed) FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
    if (Blueprint->Status == BS_Error) { UE_LOG(LogRuneSchemaBridgeAuthoring, Error, TEXT("Bridge compilation failed: %s"), Path); return false; }
    UEditorAssetSubsystem* Assets = GEditor ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
    if (!Assets || !Assets->SaveLoadedAsset(Blueprint, false)) { UE_LOG(LogRuneSchemaBridgeAuthoring, Error, TEXT("Bridge save failed: %s"), Path); return false; }
    return true;
}

bool AuthorBridges()
{
    if (!MigrateCoreAssets()) return false;
    const EFunctionFlags Common = FUNC_BlueprintEvent | FUNC_Net | FUNC_NetReliable | FUNC_Public;
    const TArray<EventSpec> RegistryEvents{
        {TEXT("ServerRequestRegistryAction"), Common | FUNC_NetServer, {{TEXT("ActionKey"), UEdGraphSchema_K2::PC_String}}, 256},
        {TEXT("ServerRequestRuneSchemaAction"), Common | FUNC_NetServer,
            {{TEXT("Channel"), UEdGraphSchema_K2::PC_String}, {TEXT("EntityId"), UEdGraphSchema_K2::PC_String},
             {TEXT("ActionKey"), UEdGraphSchema_K2::PC_String},
             {TEXT("Payload"), UEdGraphSchema_K2::PC_String}, {TEXT("Revision"), UEdGraphSchema_K2::PC_Int64}}, 512},
        {TEXT("ServerAcknowledgeRuneSchemaPresentation"), Common | FUNC_NetServer,
            {{TEXT("Channel"), UEdGraphSchema_K2::PC_String}, {TEXT("EntityId"), UEdGraphSchema_K2::PC_String},
             {TEXT("Revision"), UEdGraphSchema_K2::PC_Int64}, {TEXT("Success"), UEdGraphSchema_K2::PC_Boolean},
             {TEXT("Detail"), UEdGraphSchema_K2::PC_String}}, 768},
        {TEXT("ServerRequestRuneSchemaResync"), Common | FUNC_NetServer,
            {{TEXT("Channel"), UEdGraphSchema_K2::PC_String}, {TEXT("EntityId"), UEdGraphSchema_K2::PC_String},
             {TEXT("KnownRevision"), UEdGraphSchema_K2::PC_Int64}}, 1024},
        {TEXT("ClientRuneSchemaNotification"), Common | FUNC_NetClient,
            {{TEXT("Channel"), UEdGraphSchema_K2::PC_String}, {TEXT("EntityId"), UEdGraphSchema_K2::PC_String},
             {TEXT("Payload"), UEdGraphSchema_K2::PC_String}, {TEXT("Revision"), UEdGraphSchema_K2::PC_Int64}}, 1280},
        {TEXT("ClientRuneSchemaReceipt"), Common | FUNC_NetClient,
            {{TEXT("Channel"), UEdGraphSchema_K2::PC_String}, {TEXT("EntityId"), UEdGraphSchema_K2::PC_String},
             {TEXT("Revision"), UEdGraphSchema_K2::PC_Int64}, {TEXT("Success"), UEdGraphSchema_K2::PC_Boolean},
             {TEXT("Detail"), UEdGraphSchema_K2::PC_String}}, 1536}
    };
    const TArray<EventSpec> IdentityEvents{
        {TEXT("OnRep_DurableStateRevision"), FUNC_BlueprintEvent | FUNC_Public, {}, 128},
        {TEXT("MulticastRuneSchemaPresentation"), Common | FUNC_NetMulticast,
            {{TEXT("Channel"), UEdGraphSchema_K2::PC_String}, {TEXT("EntityId"), UEdGraphSchema_K2::PC_String},
             {TEXT("Payload"), UEdGraphSchema_K2::PC_String}, {TEXT("Revision"), UEdGraphSchema_K2::PC_Int64}}, 256}
    };
    UBlueprint* Identity = LoadObject<UBlueprint>(nullptr, IdentityPath);
    if (!Identity
        || !EnsureVariable(Identity,TEXT("DurableStateEnvelope"),UEdGraphSchema_K2::PC_String,CPF_Net)
        || !EnsureVariable(Identity,TEXT("DurableStateRevision"),UEdGraphSchema_K2::PC_Int,CPF_Net,TEXT("OnRep_DurableStateRevision")))
        return false;
    UBlueprint* World = EnsureComponentBlueprint(WorldPath,
        TEXT("/RuneSchema/Networking/BPC_RuneSchemaWorldBridge"),TEXT("BPC_RuneSchemaWorldBridge"));
    if (!World) { UE_LOG(LogRuneSchemaBridgeAuthoring, Error, TEXT("Could not create WorldBridge Blueprint.")); return false; }
    UEdGraph* WorldGraph = FBlueprintEditorUtils::FindEventGraph(World);
    const TArray<EventSpec> WorldEvents{
        {TEXT("OnRep_WorldStateRevision"), FUNC_BlueprintEvent | FUNC_Public, {}, 256},
        {TEXT("OnRep_ActivationRevision"), FUNC_BlueprintEvent | FUNC_Public, {}, 512}
    };
    for (const EventSpec& Spec : WorldEvents) if (!AddEvent(WorldGraph, Spec)) return false;
    if (!EnsureVariable(World,TEXT("ProtocolVersion"),UEdGraphSchema_K2::PC_Int,CPF_Net)
        || !EnsureVariable(World,TEXT("WorldStateEnvelope"),UEdGraphSchema_K2::PC_String,CPF_Net,TEXT("OnRep_WorldStateRevision"))
        || !EnsureVariable(World,TEXT("WorldStateRevision"),UEdGraphSchema_K2::PC_Int,CPF_Net,TEXT("OnRep_WorldStateRevision"))
        || !EnsureVariable(World,TEXT("ActivationEnvelope"),UEdGraphSchema_K2::PC_String,CPF_Net,TEXT("OnRep_ActivationRevision"))
        || !EnsureVariable(World,TEXT("ActivationRevision"),UEdGraphSchema_K2::PC_Int,CPF_Net,TEXT("OnRep_ActivationRevision"))) return false;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(World);
    FKismetEditorUtilities::CompileBlueprint(World, EBlueprintCompileOptions::SkipGarbageCollection);
    UEditorAssetSubsystem* Assets = GEditor ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
    if (World->Status == BS_Error || !Assets || !Assets->SaveLoadedAsset(World,false)) return false;
    if (!AuthorBlueprint(RegistryPath, RegistryEvents) || !AuthorBlueprint(IdentityPath, IdentityEvents)) return false;
    const TArray<EventSpec> ConnectionEvents{
        {TEXT("ServerRuneSchemaPluginRequest"), Common | FUNC_NetServer,
            {{TEXT("PluginId"), UEdGraphSchema_K2::PC_String}, {TEXT("Connection"), UEdGraphSchema_K2::PC_String},
             {TEXT("RequestId"), UEdGraphSchema_K2::PC_String}, {TEXT("Payload"), UEdGraphSchema_K2::PC_String},
             {TEXT("Revision"), UEdGraphSchema_K2::PC_Int64}}, 256},
        {TEXT("ClientRuneSchemaPluginReceipt"), Common | FUNC_NetClient,
            {{TEXT("PluginId"), UEdGraphSchema_K2::PC_String}, {TEXT("Connection"), UEdGraphSchema_K2::PC_String},
             {TEXT("RequestId"), UEdGraphSchema_K2::PC_String}, {TEXT("Revision"), UEdGraphSchema_K2::PC_Int64},
             {TEXT("Success"), UEdGraphSchema_K2::PC_Boolean}, {TEXT("Detail"), UEdGraphSchema_K2::PC_String}}, 512}
    };
    const TArray<EventSpec> PresentationEvents{
        {TEXT("ClientRuneSchemaPluginPresentation"), Common | FUNC_NetClient,
            {{TEXT("PluginId"), UEdGraphSchema_K2::PC_String}, {TEXT("Connection"), UEdGraphSchema_K2::PC_String},
             {TEXT("EntityId"), UEdGraphSchema_K2::PC_String}, {TEXT("Payload"), UEdGraphSchema_K2::PC_String},
             {TEXT("Revision"), UEdGraphSchema_K2::PC_Int64}}, 256},
        {TEXT("MulticastRuneSchemaPluginPresentation"), Common | FUNC_NetMulticast,
            {{TEXT("PluginId"), UEdGraphSchema_K2::PC_String}, {TEXT("Connection"), UEdGraphSchema_K2::PC_String},
             {TEXT("EntityId"), UEdGraphSchema_K2::PC_String}, {TEXT("Payload"), UEdGraphSchema_K2::PC_String},
             {TEXT("Revision"), UEdGraphSchema_K2::PC_Int64}}, 512}
    };
    UBlueprint* Connection=EnsureComponentBlueprint(PluginConnectionPath,
        TEXT("/RuneSchema/Networking/Extensions/BPC_RuneSchemaPluginConnection"),TEXT("BPC_RuneSchemaPluginConnection"));
    UBlueprint* State=EnsureComponentBlueprint(PluginStatePath,
        TEXT("/RuneSchema/Networking/Extensions/BPC_RuneSchemaPluginState"),TEXT("BPC_RuneSchemaPluginState"));
    UBlueprint* Presentation=EnsureComponentBlueprint(PluginPresentationPath,
        TEXT("/RuneSchema/Networking/Extensions/BPC_RuneSchemaPluginPresentation"),TEXT("BPC_RuneSchemaPluginPresentation"));
    if(!Connection||!State||!Presentation||!AuthorBlueprint(PluginConnectionPath,ConnectionEvents)
        ||!AuthorBlueprint(PluginPresentationPath,PresentationEvents))return false;
    UEdGraph* StateGraph=FBlueprintEditorUtils::FindEventGraph(State);
    const EventSpec StateRep{TEXT("OnRep_PluginStateRevision"),FUNC_BlueprintEvent|FUNC_Public,{},256};
    if(!AddEvent(StateGraph,StateRep)
        ||!EnsureVariable(State,TEXT("PluginId"),UEdGraphSchema_K2::PC_String,CPF_Net)
        ||!EnsureVariable(State,TEXT("Connection"),UEdGraphSchema_K2::PC_String,CPF_Net)
        ||!EnsureVariable(State,TEXT("StateEnvelope"),UEdGraphSchema_K2::PC_String,CPF_Net,TEXT("OnRep_PluginStateRevision"))
        ||!EnsureVariable(State,TEXT("StateRevision"),UEdGraphSchema_K2::PC_Int64,CPF_Net,TEXT("OnRep_PluginStateRevision")))return false;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(State);
    FKismetEditorUtilities::CompileBlueprint(State,EBlueprintCompileOptions::SkipGarbageCollection);
    if(State->Status==BS_Error||!Assets->SaveLoadedAsset(State,false))return false;
    UE_LOG(LogRuneSchemaBridgeAuthoring, Display, TEXT("RuneSchema generic network transport bridges authored successfully."));
    return true;
}
}

class FRuneSchemaBridgeAuthoringModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float)
        {
            AuthorBridges(); TickHandle.Reset(); return false;
        }), 1.0f);
    }
    virtual void ShutdownModule() override
    {
        if (TickHandle.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
        TickHandle.Reset();
    }
private:
    FTSTicker::FDelegateHandle TickHandle;
};

IMPLEMENT_MODULE(FRuneSchemaBridgeAuthoringModule, RuneSchemaBridgeAuthoring)
