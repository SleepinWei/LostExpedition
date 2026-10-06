#include "ExplorerPoseComponent.h"
#include "ExplorerCharacter.h"

UExplorerPoseComponent::UExplorerPoseComponent() {
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.TickGroup=TG_PostPhysics;
}
void UExplorerPoseComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction) {
    Super::TickComponent(DeltaTime,TickType,TickFunction);
    if(auto* Character=Cast<AExplorerCharacter>(GetOwner()))Character->UpdateClimbPose();
}

namespace {
FVector RotationVector(FQuat Q) {
    Q.Normalize();if(Q.W<0)Q=Q*-1;
    FVector Axis;float Angle;Q.ToAxisAndAngle(Axis,Angle);return Axis*Angle;
}
FQuat FromRotationVector(const FVector& V) {
    const float Angle=V.Size();return Angle<SMALL_NUMBER?FQuat::Identity:FQuat(V/Angle,Angle);
}
}
void UExplorerPoseComponent::ResetTransition() {
    PreviousPose.Reset();PreviousClock=-1;PreviousAction=-1;
}
void UExplorerPoseComponent::BlendActionTransition(float Clock,int32 Action) {
    const int32 Count=BoneSpaceTransforms.Num();
    if(PreviousPose.Num()!=Count)return;
    if(Action!=PreviousAction) {
        PositionOffset.SetNum(Count);RotationOffset.SetNum(Count);PositionRate.SetNum(Count);RotationRate.SetNum(Count);
        TransitionClock=Clock;
        for(int32 I=0;I<Count;I++) {
            // Grabbing can move/rotate the capsule. Preserve the outgoing root
            // in world space as well as its local skeletal pose.
            const FTransform Outgoing=I==0?(PreviousPose[I]*PreviousComponentTransform).GetRelativeTransform(GetComponentTransform()):PreviousPose[I];
            PositionOffset[I]=Outgoing.GetTranslation()-BoneSpaceTransforms[I].GetTranslation();
            RotationOffset[I]=RotationVector(Outgoing.GetRotation()*BoneSpaceTransforms[I].GetRotation().Inverse());
            PositionRate[I]=LinearVelocity[I].GetClampedToMaxSize(150);
            RotationRate[I]=AngularVelocity[I].GetClampedToMaxSize(8);
        }
    }
    PreviousAction=Action;
    const float T=Clock-TransitionClock;
    if(PositionOffset.Num()!=Count||T>.35f)return;
    // A critically damped response preserves the outgoing pose and velocity,
    // then decays towards the new action without a frame-rate-dependent lerp.
    constexpr float Frequency=28;
    const float Decay=FMath::Exp(-Frequency*T);
    for(int32 I=0;I<Count;I++) {
        const FVector P=(PositionOffset[I]+(PositionRate[I]+Frequency*PositionOffset[I])*T)*Decay;
        const FVector R=(RotationOffset[I]+(RotationRate[I]+Frequency*RotationOffset[I])*T)*Decay;
        BoneSpaceTransforms[I].AddToTranslation(P);
        BoneSpaceTransforms[I].SetRotation(FromRotationVector(R)*BoneSpaceTransforms[I].GetRotation());
        BoneSpaceTransforms[I].NormalizeRotation();
    }
    MarkRefreshTransformDirty();RefreshBoneTransforms();
}
void UExplorerPoseComponent::StorePresentedPose(float Clock) {
    const float Delta=Clock-PreviousClock;
    if(PreviousPose.Num()==BoneSpaceTransforms.Num()&&Delta>SMALL_NUMBER) {
        for(int32 I=0;I<BoneSpaceTransforms.Num();I++) {
            LinearVelocity[I]=(BoneSpaceTransforms[I].GetTranslation()-PreviousPose[I].GetTranslation())/Delta;
            AngularVelocity[I]=RotationVector(BoneSpaceTransforms[I].GetRotation()*PreviousPose[I].GetRotation().Inverse())/Delta;
        }
    } else if(PreviousPose.Num()!=BoneSpaceTransforms.Num()) {
        LinearVelocity.Init(FVector::ZeroVector,BoneSpaceTransforms.Num());AngularVelocity.Init(FVector::ZeroVector,BoneSpaceTransforms.Num());
    }
    // Re-evaluations within the same frame must not overwrite the velocity history.
    if(Delta>SMALL_NUMBER||PreviousClock<0){PreviousPose=BoneSpaceTransforms;PreviousClock=Clock;PreviousComponentTransform=GetComponentTransform();}
}
