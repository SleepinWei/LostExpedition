#pragma once
#include "CoreMinimal.h"
#include "Components/PoseableMeshComponent.h"
#include "ExplorerPoseSpring.h"
#include "ExplorerPoseComponent.generated.h"

// Evaluate the presentation pose after the locomotion mesh has updated its bones.
UCLASS()
class LOSTEXPEDITION_API UExplorerPoseComponent : public UPoseableMeshComponent {
    GENERATED_BODY()
public:
    UExplorerPoseComponent();
    virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction) override;
    // Inertial offsets are evaluated before contact IK. Contacts remain authoritative.
    void BlendActionTransition(float Clock,int32 Action);
    void StorePresentedPose(float Clock);
    void ResetTransition();
    void SmoothWallPose(float Clock);
private:
    FExplorerPoseSpring WallSpring;
    float WallClock=-1;
    TArray<FTransform> PreviousPose;
    TArray<FVector> LinearVelocity,AngularVelocity,PositionOffset,RotationOffset,PositionRate,RotationRate;
    float PreviousClock=-1,TransitionClock=0;
    int32 PreviousAction=-1;
    FTransform PreviousComponentTransform;
};
