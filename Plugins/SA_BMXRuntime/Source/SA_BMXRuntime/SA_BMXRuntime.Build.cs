using UnrealBuildTool;
public class SA_BMXRuntime : ModuleRules
{
    public SA_BMXRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });
        PrivateDependencyModuleNames.AddRange(new string[] { "ChaosVehicles", "Slate", "SlateCore", "UMG", "CinematicCamera" });
    }
}
