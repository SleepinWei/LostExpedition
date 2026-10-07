#pragma once
#include "CoreMinimal.h"
#include "Components/PoseableMeshComponent.h"
#include "ExplorerPoseSpring.h"
#include "ExplorerVisualComponent.generated.h"

struct FExplorerRetargetState;

// Retarget the finished locomotion / weapon / traversal pose to the clothed hero.
// Keeping retargeting last also carries the exact wall contacts through the rig change.
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
