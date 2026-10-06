#include "ExplorerCharacter.h"
#include "ExplorerPoseComponent.h"
#include "ExpeditionTower.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
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
void ApplyDirectionalLocomotion(UPoseableMeshComponent* Mesh,const TArray<TObjectPtr<UAnimSequence>>& Clips,int32 Weapon,const FVector& LocalVelocity,float Cycle,float Weight) {
    if(Weight<.002f||Clips.Num()!=32||Mesh->RequiredBones.GetNumBones()==0)return;
    const float Speed=LocalVelocity.Size2D(),Gait=FMath::SmoothStep(240.f,430.f,Speed);
    const float Direction=FMath::Fmod(FMath::RadiansToDegrees(FMath::Atan2(LocalVelocity.Y,LocalVelocity.X))+360.f,360.f)/45.f;
    const int32 First=FMath::FloorToInt(Direction),Second=(First+1)%8;const float DirectionWeight=Direction-First;
    FMemMark Mark(FMemStack::Get());
    FCompactPose Samples[4];FBlendedCurve Curves[4];UE::Anim::FStackAttributeContainer Attributes[4];
    for(int32 I=0;I<4;I++) {
        UAnimSequence* Clip=Clips[Weapon*16+(I/2)*8+(I%2==0?First:Second)];if(!Clip)return;
        Samples[I].SetBoneContainer(&Mesh->RequiredBones);Samples[I].ResetToRefPose();Curves[I].InitFrom(Mesh->RequiredBones);
        FAnimationPoseData Data(Samples[I],Curves[I],Attributes[I]);Clip->GetAnimationPose(Data,FAnimExtractContext(double(Cycle*Clip->GetPlayLength()),true));
    }
    for(FCompactPoseBoneIndex Index:Samples[0].ForEachBoneIndex()) {
        const int32 Bone=Mesh->RequiredBones.MakeMeshPoseIndex(Index).GetInt();if(Bone==0)continue;
        FTransform Walk,Jog,Target;Walk.Blend(Samples[0][Index],Samples[1][Index],DirectionWeight);Jog.Blend(Samples[2][Index],Samples[3][Index],DirectionWeight);Target.Blend(Walk,Jog,Gait);
        FTransform& Base=Mesh->BoneSpaceTransforms[Bone];Base.Blend(Base,Target,Weight);Base.NormalizeRotation();
    }
    Mesh->MarkRefreshTransformDirty();Mesh->RefreshBoneTransforms();
}
// UE's firearm clips use mesh-space additive rotations; preserve that convention.
FVector ApplyFirearmPose(UPoseableMeshComponent* Mesh,UAnimSequence* Idle,UAnimSequence* Fire,float Clock,float ShotTime,float PreviousShotTime,float ShotBlend,float Weight,UAnimSequence* Action,float ActionTime,float ActionWeight) {
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
    if(Action&&ActionWeight>0) {
        FCompactPose ActionPose;ActionPose.SetBoneContainer(&Mesh->RequiredBones);ActionPose.ResetToRefPose();
        FBlendedCurve ActionCurve;ActionCurve.InitFrom(Mesh->RequiredBones);
        UE::Anim::FStackAttributeContainer ActionAttributes;FAnimationPoseData ActionData(ActionPose,ActionCurve,ActionAttributes);
        Action->GetAnimationPose(ActionData,FAnimExtractContext(double(FMath::Clamp(ActionTime,0.f,Action->GetPlayLength()))));
        for(FCompactPoseBoneIndex Index:Pose.ForEachBoneIndex())Pose[Index].Blend(Pose[Index],ActionPose[Index],ActionWeight);
    }
    for(int32 Shot=0;Shot<2&&Fire&&ShotTime>=0;Shot++) {
        const float SampleTime=Shot==0?PreviousShotTime:ShotTime,SampleWeight=Shot==0?1-ShotBlend:ShotBlend;
        if(SampleTime<0||SampleWeight<=0)continue;
        FCompactPose Additive;Additive.SetBoneContainer(&Mesh->RequiredBones);Additive.ResetToAdditiveIdentity();
        FBlendedCurve AdditiveCurve;AdditiveCurve.InitFrom(Mesh->RequiredBones);
        UE::Anim::FStackAttributeContainer AdditiveAttributes;
        FAnimationPoseData AdditiveData(Additive,AdditiveCurve,AdditiveAttributes);
        const float Time=FMath::Clamp(SampleTime,0.f,Fire->GetPlayLength());
        Fire->GetAnimationPose(AdditiveData,FAnimExtractContext(double(Time)));
        const float Fade=1-Phase(Fire->GetPlayLength()*.8f,Fire->GetPlayLength(),Time);
        FAnimationRuntime::AccumulateAdditivePose(Data,AdditiveData,Fade*SampleWeight,Fire->GetAdditiveAnimType());
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
    // One presentation mesh and a permanent weapon socket avoid visibility and attachment pops.
    GetMesh()->SetVisibility(false);ClimbPose->SetVisibility(true);
    WeaponMesh->SetVisibility(!Climbing&&!bCompleted);
    auto* Presentation=CastChecked<UExplorerPoseComponent>(ClimbPose);
    ClimbPose->CopyPoseFromSkeletalComponent(GetMesh());
    if(!Climbing) {
        if(GetCharacterMovement()->IsMovingOnGround())ApplyDirectionalLocomotion(ClimbPose,ArmedLocomotionAnimations,Weapon,GetActorQuat().UnrotateVector(GetVelocity()),LocomotionPhase,ArmAnimationAlpha*Phase(15,60,GetVelocity().Size2D()));
        UAnimSequence* Action=nullptr;float ActionTime=0,ActionWeight=0;
        if(bReloading) {
            Action=WeaponReloadAnimations[Weapon];const float Elapsed=ReloadDuration-ReloadRemaining;
            ActionTime=Action?Elapsed/ReloadDuration*Action->GetPlayLength():0;
            ActionWeight=Phase(0,.12f,Elapsed)*(1-Phase(ReloadDuration-.18f,ReloadDuration,Elapsed));
        } else if(EquipAnimationTime>=0) {
            Action=WeaponEquipAnimations[Weapon];ActionTime=EquipAnimationTime;
            ActionWeight=Action?Phase(0,.12f,ActionTime)*(1-Phase(Action->GetPlayLength()-.2f,Action->GetPlayLength(),ActionTime)):0;
        }
        const float FireRate=Weapon==0?1.5f:2.f;
        const FVector NeutralDirection=ApplyFirearmPose(ClimbPose,WeaponIdleAnimations[Weapon],WeaponFireAnimations[Weapon],AnimationClock,FireAnimationTime*FireRate,PreviousFireAnimationTime*FireRate,Phase(0,.055f,FireBlendTime),ArmAnimationAlpha,Action,ActionTime,ActionWeight);
        // Aim follows the camera while lower-body locomotion stays intact.
        const FRotator AimOffset=(Camera->GetForwardVector().Rotation()-NeutralDirection.Rotation()).GetNormalized();
        const float AimWeight=ArmAnimationAlpha*(1-ActionWeight);
        const float Pitch=FMath::Clamp(AimOffset.Pitch,-65.f,65.f)*AimWeight;
        RotateBone(ClimbPose,TEXT("spine_01"),FVector::UpVector,FMath::Clamp(AimOffset.Yaw,-90.f,90.f)*AimWeight);
        RotateBone(ClimbPose,TEXT("spine_02"),GetActorRightVector(),-Pitch*.55f);
        RotateBone(ClimbPose,TEXT("spine_03"),GetActorRightVector(),-Pitch*.45f);
        Presentation->BlendActionTransition(AnimationClock,0);ClimbPose->RefreshBoneTransforms();Presentation->StorePresentedPose(AnimationClock);return;
    }
    const FVector Side=FVector::CrossProduct(FVector::UpVector,-WallNormal),Body=GetActorLocation();
    const float Breath=FMath::Sin(AnimationClock*2.6f);
    FVector Hands[2]={Ledge-Side*24+WallNormal*3,Ledge+Side*24+WallNormal*3};
    auto FeetAt=[&](const FVector& At,int Index){
        const FVector Origin=At+Side*(Index==0?-24:24)-FVector(0,0,Index==0?30:36);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ClimbFoot),false,this);FHitResult Hit;
        if(GetWorld()->LineTraceSingleByChannel(Hit,Origin+WallNormal*15,Origin-WallNormal*150,ECC_Visibility,Query)&&Hit.Component.IsValid()&&Hit.Component->ComponentHasTag(TEXT("Climbable")))return Hit.ImpactPoint+WallNormal*6;
        return Origin-WallNormal*75;
    };
    FVector Feet[2]={FeetAt(Body,0),FeetAt(Body,1)};
    float Lean=5,LeanSide=Breath*1.2f,RootLower=0;
    if(Traversal==ETraversalState::Reaching&&!bEnteringFromRoof) {
        const float T=FMath::Clamp(ReachTime/ReachDuration,0.f,1.f);
        const FVector From=ExpeditionTower::Grip(CurrentGrip),To=ExpeditionTower::Grip(TargetGrip);
        for(int Index=0;Index<2;Index++) {
            const bool Lead=Index==(bLeadRight?1:0);const float H=Phase(Lead?0.f:.40f,Lead?.82f:1.f,T);
            const FVector Offset=Side*(Index==0?-24:24)+WallNormal*3;
            Hands[Index]=ReachArc(From+Offset,To+Offset,H,WallNormal,14);
            const float F=Phase(Lead?.08f:.18f,Lead?.80f:1.f,T);
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
    const float WallWeight=Traversal==ETraversalState::Mantling?1-Phase(.55f,1.f,MantleTime/MantleDuration):1;
    Root.AddToTranslation(-WallNormal*40*WallWeight+FVector(0,0,RootLower+8*WallWeight+Breath*.6f));ClimbPose->SetBoneTransformByName(TEXT("root"),Root,EBoneSpaces::WorldSpace);
    RotateBone(ClimbPose,TEXT("spine_01"),Side,Lean);
    RotateBone(ClimbPose,TEXT("spine_02"),-WallNormal,LeanSide);
    Presentation->BlendActionTransition(AnimationClock,Traversal==ETraversalState::Mantling?2:1);
    // Bound the body correction by actual limb lengths. The planted hands drive
    // the torso towards the contact manifold before the four independent IK solves.
    for(int Pass=0;Pass<3;Pass++) {
        FVector Correction=FVector::ZeroVector;int32 Count=0;
        for(int Index=0;Index<2;Index++) {
            const FName Upper=Index==0?TEXT("upperarm_l"):TEXT("upperarm_r"),Lower=Index==0?TEXT("lowerarm_l"):TEXT("lowerarm_r"),End=Index==0?TEXT("hand_l"):TEXT("hand_r");
            const FVector A=ClimbPose->GetBoneLocationByName(Upper,EBoneSpaces::WorldSpace),B=ClimbPose->GetBoneLocationByName(Lower,EBoneSpaces::WorldSpace),C=ClimbPose->GetBoneLocationByName(End,EBoneSpaces::WorldSpace);
            const float Reach=FVector::Dist(A,B)+FVector::Dist(B,C)-1,Distance=FVector::Dist(A,Hands[Index]);
            if(Distance>Reach){Correction+=(Hands[Index]-A).GetSafeNormal()*(Distance-Reach);Count++;}
        }
        if(Count==0)break;
        FTransform Adjust=ClimbPose->GetBoneTransformByName(TEXT("root"),EBoneSpaces::WorldSpace);Adjust.AddToTranslation((Correction/Count).GetClampedToMaxSize(12));ClimbPose->SetBoneTransformByName(TEXT("root"),Adjust,EBoneSpaces::WorldSpace);
    }
    SolveLimb(ClimbPose,TEXT("upperarm_l"),TEXT("lowerarm_l"),TEXT("hand_l"),Hands[0],Body-Side*65+WallNormal*45+FVector(0,0,45));
    SolveLimb(ClimbPose,TEXT("upperarm_r"),TEXT("lowerarm_r"),TEXT("hand_r"),Hands[1],Body+Side*65+WallNormal*45+FVector(0,0,45));
    SolveLimb(ClimbPose,TEXT("thigh_l"),TEXT("calf_l"),TEXT("foot_l"),Feet[0],Body-WallNormal*90-Side*36-FVector(0,0,15));
    SolveLimb(ClimbPose,TEXT("thigh_r"),TEXT("calf_r"),TEXT("foot_r"),Feet[1],Body-WallNormal*90+Side*36-FVector(0,0,28));
    for(int Index=0;Index<2;Index++){AnimatedHands[Index]=Hands[Index];AnimatedFeet[Index]=Feet[Index];}
    ClimbPose->RefreshBoneTransforms();Presentation->StorePresentedPose(AnimationClock);
}
