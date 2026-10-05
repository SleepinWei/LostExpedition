using UnrealBuildTool;
public class LostExpedition : ModuleRules {
    public LostExpedition(ReadOnlyTargetRules Target) : base(Target) {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] {"Core", "CoreUObject", "Engine", "InputCore", "PhysicsCore"});
    }
}
