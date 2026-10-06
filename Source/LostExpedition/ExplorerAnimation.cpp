#include "ExplorerCharacter.h"
#include "ExpeditionTower.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AttributeTypes.h"
#include "AnimationRuntime.h"
#include "Misc/MemStack.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/SkeletalMeshSocket.h"

namespace {
float Phase(float Begin,float End,float Time){return FMath::SmoothStep(Begin,End,Time);}
FVector ReachArc(const FVector& Start,const FVector& End,float T,const FVector& Normal,float Lift) {
    return FMath::Lerp(Start,End,T)+(Normal*12+FVector::UpVector*Lift)*FMath::Sin(PI*T);
}
void SolveLimb(UPoseableMeshComponent* Pose,FName Upper,FName Lower,FName End,FVector Target,FVector Pole) {
    FTransform Root=Pose->GetBoneTransformByName(Upper,EBoneSpaces::WorldSpace);
    const FVector A=Root.GetLocation(),B=Pose->GetBoneLocationByName(Lower,EBoneSpaces::WorldSpace),C=Pose->GetBoneLocationByName(End,EBoneSpaces::WorldSpace);
    const float L1=FVector::Dist(A,B),L2=FVector::Dist(B,C);if(L1<1||L2<1)return;
    const FVector Direction=(Target-A).GetSafeNormal();const float D=FMath::Clamp(FVector::Dist(A,Target),FMath::Abs(L1-L2)+1,L1+L2-1);
    Target=A+Direction*D;
    FVector Bend=(Pole-A)-Direction*FVector::DotProduct(Pole-A,Direction);
    if(!Bend.Normalize())Bend=FVector::CrossProduct(Direction,FVector::RightVector).GetSafeNormal();
    const float Along=(L1*L1-L2*L2+D*D)/(2*D);
    const FVector Joint=A+Direction*Along+Bend*FMath::Sqrt(FMath::Max(0.f,L1*L1-Along*Along));
    Root.SetRotation(FQuat::FindBetweenNormals((B-A).GetSafeNormal(),(Joint-A).GetSafeNormal())*Root.GetRotation());
    Pose->SetBoneTransformByName(Upper,Root,EBoneSpaces::WorldSpace);
    FTransform Mid=Pose->GetBoneTransformByName(Lower,EBoneSpaces::WorldSpace);
    const FVector Tip=Pose->GetBoneLocationByName(End,EBoneSpaces::WorldSpace);
    Mid.SetRotation(FQuat::FindBetweenNormals((Tip-Mid.GetLocation()).GetSafeNormal(),(Target-Mid.GetLocation()).GetSafeNormal())*Mid.GetRotation());
    Pose->SetBoneTransformByName(Lower,Mid,EBoneSpaces::WorldSpace);
}
void RotateBone(UPoseableMeshComponent* Pose,FName Bone,const FVector& Axis,float Degrees) {
    FTransform Transform=Pose->GetBoneTransformByName(Bone,EBoneSpaces::WorldSpace);
    Transform.SetRotation(FQuat(Axis,FMath::DegreesToRadians(Degrees))*Transform.GetRotation());
    Pose->SetBoneTransformByName(Bone,Transform,EBoneSpaces::WorldSpace);
}
// UE's firearm clips use mesh-space additive rotations; preserve that convention.
FVector ApplyFirearmPose(UPoseableMeshComponent* Mesh,UAnimSequence* Idle,UAnimSequence* Fire,float Clock,float ShotTime,float Weight) {
    if(!Idle||Mesh->RequiredBones.GetNumBones()==0)return Mesh->GetForwardVector();
    FMemMark Mark(FMemStack::Get());
    FCompactPose Pose;Pose.SetBoneContainer(&Mesh->RequiredBones);Pose.ResetToRefPose();
    FBlendedCurve Curve;Curve.InitFrom(Mesh->RequiredBones);
    UE::Anim::FStackAttributeContainer Attributes;
    FAnimationPoseData Data(Pose,Curve,Attributes);
    Idle->GetAnimationPose(Data,FAnimExtractContext(double(FMath::Fmod(Clock,Idle->GetPlayLength()))));
    FVector NeutralDirection=Mesh->GetForwardVector();
    if(const USkeletalMeshSocket* Socket=Mesh->GetSocketByName(TEXT("HandGrip_R"))) {
        const FCompactPoseBoneIndex Hand=Mesh->RequiredBones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Mesh->GetBoneIndex(Socket->BoneName)));
        if(Hand.GetInt()!=INDEX_NONE) {
            FQuat Rotation=Pose[Hand].GetRotation();
            for(FCompactPoseBoneIndex Parent=Mesh->RequiredBones.GetParentBoneIndex(Hand);Parent.GetInt()!=INDEX_NONE;Parent=Mesh->RequiredBones.GetParentBoneIndex(Parent))
                Rotation=Pose[Parent].GetRotation()*Rotation;
            // Both template weapons point down local +Y. Calibrate the idle pose,
            // before applying recoil, so aiming does not cancel the firing motion.
            NeutralDirection=(Mesh->GetComponentQuat()*Rotation*Socket->GetSocketLocalTransform().GetRotation()).RotateVector(FVector::RightVector);
        }
    }
    if(Fire&&ShotTime>=0) {
        FCompactPose Additive;Additive.SetBoneContainer(&Mesh->RequiredBones);Additive.ResetToAdditiveIdentity();
        FBlendedCurve AdditiveCurve;AdditiveCurve.InitFrom(Mesh->RequiredBones);
        UE::Anim::FStackAttributeContainer AdditiveAttributes;
        FAnimationPoseData AdditiveData(Additive,AdditiveCurve,AdditiveAttributes);
        const float Time=FMath::Clamp(ShotTime,0.f,Fire->GetPlayLength());
        Fire->GetAnimationPose(AdditiveData,FAnimExtractContext(double(Time)));
        const float Fade=1-Phase(Fire->GetPlayLength()*.8f,Fire->GetPlayLength(),Time);
        FAnimationRuntime::AccumulateAdditivePose(Data,AdditiveData,Fade,Fire->GetAdditiveAnimType());
    }
    Pose.NormalizeRotations();
    FCompactPose Base;Base.SetBoneContainer(&Mesh->RequiredBones);
    for(FCompactPoseBoneIndex Index:Base.ForEachBoneIndex())Base[Index]=Mesh->BoneSpaceTransforms[Mesh->RequiredBones.MakeMeshPoseIndex(Index).GetInt()];
    // The supplied aim clips have different pelvis orientations from locomotion.
    // Blend upper-body rotations in mesh space, then rebuild locals under the preserved pelvis.
    FAnimationRuntime::ConvertPoseToMeshRotation(Pose);
    FAnimationRuntime::ConvertPoseToMeshRotation(Base);
    const FReferenceSkeleton& Ref=Mesh->GetSkinnedAsset()->GetRefSkeleton();
    const int32 Spine=Ref.FindBoneIndex(TEXT("spine_01"));
    for(FCompactPoseBoneIndex Index:Pose.ForEachBoneIndex()) {
        const int32 Bone=Mesh->RequiredBones.MakeMeshPoseIndex(Index).GetInt();
        int32 Parent=Bone;while(Parent>0&&Parent!=Spine)Parent=Ref.GetParentIndex(Parent);
        if(Parent==Spine) {
            Base[Index].SetRotation(FQuat::Slerp(Base[Index].GetRotation(),Pose[Index].GetRotation(),Weight));
            Base[Index].SetTranslation(FMath::Lerp(Base[Index].GetTranslation(),Pose[Index].GetTranslation(),double(Weight)));
        }
    }
    FAnimationRuntime::ConvertMeshRotationPoseToLocalSpace(Base);Base.NormalizeRotations();
    for(FCompactPoseBoneIndex Index:Base.ForEachBoneIndex())Mesh->BoneSpaceTransforms[Mesh->RequiredBones.MakeMeshPoseIndex(Index).GetInt()]=Base[Index];
    Mesh->MarkRefreshTransformDirty();Mesh->RefreshBoneTransforms();
    return NeutralDirection;
}
}

void AExplorerCharacter::BeginGrabAnimation() {
    auto* Source=ClimbPose->IsVisible()?static_cast<USkinnedMeshComponent*>(ClimbPose):static_cast<USkinnedMeshComponent*>(GetMesh());
    GrabHands[0]=Source->GetSocketLocation(TEXT("hand_l"));GrabHands[1]=Source->GetSocketLocation(TEXT("hand_r"));
    GrabFeet[0]=Source->GetSocketLocation(TEXT("foot_l"));GrabFeet[1]=Source->GetSocketLocation(TEXT("foot_r"));
    GrabTime=0;FireAnimationTime=-1;ArmAnimationAlpha=0;
}
void AExplorerCharacter::UpdateClimbPose() {
    const bool Climbing=Traversal!=ETraversalState::Walking;
    const bool Show=Climbing||ArmAnimationAlpha>.002f;
    GetMesh()->SetVisibility(!Show);ClimbPose->SetVisibility(Show);
    USceneComponent* WeaponParent=Show?static_cast<USceneComponent*>(ClimbPose):static_cast<USceneComponent*>(GetMesh());
    if(WeaponMesh->GetAttachParent()!=WeaponParent)WeaponMesh->AttachToComponent(WeaponParent,FAttachmentTransformRules::KeepRelativeTransform,TEXT("HandGrip_R"));
    WeaponMesh->SetVisibility(!Climbing&&!bCompleted);
    if(!Show)return;
    ClimbPose->CopyPoseFromSkeletalComponent(GetMesh());
    if(!Climbing) {
        const FVector NeutralDirection=ApplyFirearmPose(ClimbPose,WeaponIdleAnimations[Weapon],WeaponFireAnimations[Weapon],AnimationClock,FireAnimationTime*(Weapon==0?1.5f:2.f),ArmAnimationAlpha);
        // Aim follows the camera while lower-body locomotion stays intact.
        const FRotator AimOffset=(Camera->GetForwardVector().Rotation()-NeutralDirection.Rotation()).GetNormalized();
        const float Pitch=FMath::Clamp(AimOffset.Pitch,-65.f,65.f)*ArmAnimationAlpha;
        RotateBone(ClimbPose,TEXT("spine_01"),FVector::UpVector,FMath::Clamp(AimOffset.Yaw,-90.f,90.f)*ArmAnimationAlpha);
        RotateBone(ClimbPose,TEXT("spine_02"),GetActorRightVector(),-Pitch*.55f);
        RotateBone(ClimbPose,TEXT("spine_03"),GetActorRightVector(),-Pitch*.45f);
        ClimbPose->RefreshBoneTransforms();return;
    }
    const FVector Side=FVector::CrossProduct(FVector::UpVector,-WallNormal),Body=GetActorLocation();
    const float Breath=FMath::Sin(AnimationClock*2.6f);
    FVector Hands[2]={Ledge-Side*24+WallNormal*3,Ledge+Side*24+WallNormal*3};
    auto FeetAt=[&](const FVector& At,int Index){return At-WallNormal*83+Side*(Index==0?-24:24)-FVector(0,0,Index==0?62:76);};
    FVector Feet[2]={FeetAt(Body,0),FeetAt(Body,1)};
    float Lean=5,LeanSide=Breath*1.2f,RootLower=0;
    if(Traversal==ETraversalState::Reaching&&!bEnteringFromRoof) {
        const float T=FMath::Clamp(ReachTime/GripTransferDuration,0.f,1.f);
        const FVector From=ExpeditionTower::Grip(CurrentGrip),To=ExpeditionTower::Grip(TargetGrip);
        for(int Index=0;Index<2;Index++) {
            const bool Lead=Index==(bLeadRight?1:0);const float H=Phase(Lead?0.f:.32f,Lead?.65f:.88f,T);
            const FVector Offset=Side*(Index==0?-24:24)+WallNormal*3;
            Hands[Index]=ReachArc(From+Offset,To+Offset,H,WallNormal,14);
            const float F=Phase(Lead?.12f:.32f,Lead?.66f:.96f,T);
            Feet[Index]=ReachArc(FeetAt(ExpeditionTower::HangPosition(CurrentGrip),Index),FeetAt(ExpeditionTower::HangPosition(TargetGrip),Index),F,WallNormal,35);
        }
        LeanSide=(bLeadRight?1:-1)*7*FMath::Sin(PI*T);Lean=5+9*FMath::Sin(PI*T);
    } else if(Traversal==ETraversalState::Mantling) {
        const float T=FMath::Clamp(MantleTime/MantleDuration,0.f,1.f);
        const float Crouch=FMath::Sin(PI*T);Lean=65*Crouch;RootLower=-80*Crouch;
        for(int Index=0;Index<2;Index++) {
            const float H=Phase(Index==0?.50f:.62f,.98f,T);
            Hands[Index]=FMath::Lerp(Hands[Index],GetMesh()->GetSocketLocation(Index==0?TEXT("hand_l"):TEXT("hand_r")),H);
            FVector End=MantleEnd-WallNormal*20+Side*(Index==0?-24:24);End.Z=Ledge.Z+6;
            const float F=Phase(Index==0?.18f:.38f,Index==0?.65f:.84f,T);
            Feet[Index]=ReachArc(FeetAt(MantleStart,Index),End,F,WallNormal,65);
        }
    } else if(bEnteringFromRoof) {
        const float T=FMath::Clamp(ReachTime/RoofEntryDuration,0.f,1.f);
        Lean=55*FMath::Sin(PI*T);RootLower=-35*FMath::Sin(PI*T);
        for(int Index=0;Index<2;Index++) {
            Hands[Index]=FMath::Lerp(GrabHands[Index],Hands[Index],Phase(.04f,.44f,T));
            Feet[Index]=FMath::Lerp(GrabFeet[Index],Feet[Index],Phase(.38f,.95f,T));
        }
    } else if(GrabTime<.26f) {
        const float T=Phase(0,.26f,GrabTime);
        for(int Index=0;Index<2;Index++) {
            Hands[Index]=ReachArc(GrabHands[Index],Hands[Index],T,WallNormal,12);
            Feet[Index]=FMath::Lerp(GrabFeet[Index],Feet[Index],T);
        }
    }
    FTransform Root=ClimbPose->GetBoneTransformByName(TEXT("root"),EBoneSpaces::WorldSpace);
    Root.AddToTranslation(FVector(0,0,RootLower+Breath*.6f));ClimbPose->SetBoneTransformByName(TEXT("root"),Root,EBoneSpaces::WorldSpace);
    RotateBone(ClimbPose,TEXT("spine_01"),Side,Lean);
    RotateBone(ClimbPose,TEXT("spine_02"),-WallNormal,LeanSide);
    SolveLimb(ClimbPose,TEXT("upperarm_l"),TEXT("lowerarm_l"),TEXT("hand_l"),Hands[0],Body-Side*65+WallNormal*45+FVector(0,0,45));
    SolveLimb(ClimbPose,TEXT("upperarm_r"),TEXT("lowerarm_r"),TEXT("hand_r"),Hands[1],Body+Side*65+WallNormal*45+FVector(0,0,45));
    SolveLimb(ClimbPose,TEXT("thigh_l"),TEXT("calf_l"),TEXT("foot_l"),Feet[0],Body-WallNormal*90-Side*36-FVector(0,0,15));
    SolveLimb(ClimbPose,TEXT("thigh_r"),TEXT("calf_r"),TEXT("foot_r"),Feet[1],Body-WallNormal*90+Side*36-FVector(0,0,28));
    for(int Index=0;Index<2;Index++){AnimatedHands[Index]=Hands[Index];AnimatedFeet[Index]=Feet[Index];}
    ClimbPose->RefreshBoneTransforms();
}
