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
    // Canonical Manny names also resolve on the legacy Mixamo rig.
    FName ResolveBone(FName Canonical) const;
    FQuat ContactWrist(int32 Side, const FVector& Forward, const FVector& Palm) const;
    void CurlFingers(int32 Side, float Degrees, float Weight);
    void Present(UPoseableMeshComponent* Source, float DeltaTime);
    bool IsRetargetReady() const;
    void SmoothWallPose(float DeltaTime,bool Wall,int32 Stage);
    void StoreVisualPose(const FQuat* UnconstrainedWrists=nullptr);
    void ResetSmoothing();
    class USkeletalMesh* GetCharacterMesh() const;
    UPROPERTY() TObjectPtr<class UIKRetargeter> RetargetAsset;
private:
    TSharedPtr<FExplorerRetargetState> Retarget;
    void CalibrateHands();
    FQuat HandFrames[2]={FQuat::Identity,FQuat::Identity};
    TMap<int32,FVector> FingerAxes;
    FExplorerPoseSpring WallSpring;
    TArray<FTransform> LastPresented;
    int32 PreviousStage=-1;
    bool bWasWall=false;
    float ExitBlendRemaining=0;
};
