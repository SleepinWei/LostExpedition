#pragma once
#include "CoreMinimal.h"
#include "Components/PoseableMeshComponent.h"
#include "ExplorerPoseSpring.h"
#include "ExplorerVisualComponent.generated.h"

struct FExplorerRetargetState;

// Retarget locomotion, weapon layers and authored traversal performance.
// The visible rig owns the final wall-contact solve; free flight preserves the animation.
UCLASS()
class LOSTEXPEDITION_API UExplorerVisualComponent : public UPoseableMeshComponent {
    GENERATED_BODY()
public:
    UExplorerVisualComponent();
    virtual ~UExplorerVisualComponent() override;
    void Present(UPoseableMeshComponent* Source, float DeltaTime);
    bool IsRetargetReady() const;
    void SmoothWallPose(float DeltaTime,bool Wall);
    void StoreVisualPose();
    void ResetSmoothing();
    class USkeletalMesh* GetCharacterMesh() const;
    UPROPERTY() TObjectPtr<class UIKRetargeter> RetargetAsset;
private:
    TSharedPtr<FExplorerRetargetState> Retarget;
    FExplorerPoseSpring WallSpring;
    TArray<FTransform> LastPresented;
    bool bWasWall=false;
    float ExitBlendRemaining=0;
};
