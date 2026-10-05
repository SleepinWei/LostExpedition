using UnrealBuildTool;
public class LostExpeditionEditorTarget : TargetRules {
    public LostExpeditionEditorTarget(TargetInfo Target) : base(Target) {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("LostExpedition");
    }
}
