using UnrealBuildTool;
public class DronesSetupEditor : ModuleRules
{
    public DronesSetupEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UnrealEd", "BlueprintGraph", "Kismet", "KismetCompiler", "EnhancedInput", "InputBlueprintNodes", "InputCore" });
    }
}
