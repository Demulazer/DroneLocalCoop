using UnrealBuildTool;
public class DronesSetup : ModuleRules
{
    public DronesSetup(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "EnhancedInput", "InputCore", "ApplicationCore", "UMG", "ChaosVehicles", "DeveloperSettings" });
    }
}
