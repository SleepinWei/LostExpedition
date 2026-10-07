using UnrealBuildTool;
public class LostExpedition : ModuleRules {
    public LostExpedition(ReadOnlyTargetRules Target) : base(Target) {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] {"Core", "CoreUObject", "Engine", "InputCore", "PhysicsCore", "ProceduralMeshComponent", "PoseSearch", "AnimGraphRuntime", "BlendStack", "IKRig"});
        if (Target.bBuildEditor) PrivateDependencyModuleNames.AddRange(new[] {"UnrealEd", "AnimGraph", "BlueprintGraph", "PoseSearchEditor", "AssetRegistry"});
    }
}
