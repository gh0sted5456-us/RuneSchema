using UnrealBuildTool;

public class RuneSchemaBridgeAuthoring : ModuleRules
{
    public RuneSchemaBridgeAuthoring(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine", "UnrealEd", "Kismet",
            "BlueprintGraph", "GraphEditor", "EditorSubsystem"
        });
    }
}
