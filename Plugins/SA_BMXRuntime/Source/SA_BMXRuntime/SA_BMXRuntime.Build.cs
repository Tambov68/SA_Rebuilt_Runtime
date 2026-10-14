using UnrealBuildTool;
public class SA_BMXRuntime : ModuleRules
{
    public SA_BMXRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });
        PrivateDependencyModuleNames.AddRange(new string[] { "ChaosVehicles", "Slate", "SlateCore", "UMG", "CinematicCamera" });
        // Explicit opt-in: ordinary builds do not require the NVIDIA plugin.
        bool WithDLSS = System.Environment.GetEnvironmentVariable("SA_BMX_WITH_DLSS") == "1";
        PublicDefinitions.Add("SA_BMX_WITH_DLSS=" + (WithDLSS ? "1" : "0"));
        if (WithDLSS)
        {
            PrivateDependencyModuleNames.Add("DLSSBlueprint");
        }
    }
}
