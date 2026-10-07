#include "ExplorerCharacter.h"
#include "ExplorerPoseComponent.h"
#include "ExplorerMotionMatching.h"
#include "ExplorerVisualComponent.h"
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
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#if WITH_EDITOR
#include "Animation/Skeleton.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#endif

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
void ApplyWallClip(UPoseableMeshComponent* Mesh,UAnimSequence* Clip,float Time) {
    if(!Clip||Mesh->RequiredBones.GetNumBones()==0)return;
    FMemMark Mark(FMemStack::Get());FCompactPose Pose;Pose.SetBoneContainer(&Mesh->RequiredBones);Pose.ResetToRefPose();
    FBlendedCurve Curve;Curve.InitFrom(Mesh->RequiredBones);UE::Anim::FStackAttributeContainer Attributes;
    FAnimationPoseData Data(Pose,Curve,Attributes);Clip->GetAnimationPose(Data,FAnimExtractContext(double(FMath::Clamp(Time,0.f,Clip->GetPlayLength()))));
    for(FCompactPoseBoneIndex Index:Pose.ForEachBoneIndex())Mesh->BoneSpaceTransforms[Mesh->RequiredBones.MakeMeshPoseIndex(Index).GetInt()]=Pose[Index];
    Mesh->MarkRefreshTransformDirty();Mesh->RefreshBoneTransforms();
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
    auto* Source=static_cast<USkinnedMeshComponent*>(ClimbPose);
    GrabHands[0]=Source->GetSocketLocation(TEXT("hand_l"));GrabHands[1]=Source->GetSocketLocation(TEXT("hand_r"));
    GrabFeet[0]=Source->GetSocketLocation(TEXT("foot_l"));GrabFeet[1]=Source->GetSocketLocation(TEXT("foot_r"));
    GrabTime=0;FireAnimationTime=-1;ArmAnimationAlpha=0;
}
void AExplorerCharacter::UpdateClimbPose() {
    const bool Climbing=Traversal!=ETraversalState::Walking;
    // One presentation mesh and a permanent weapon socket avoid visibility and attachment pops.
    GetMesh()->SetVisibility(false);ClimbPose->SetVisibility(!CharacterVisual->GetCharacterMesh());
    WeaponMesh->SetVisibility(!Climbing&&!bCompleted);
    auto* Presentation=CastChecked<UExplorerPoseComponent>(ClimbPose);
    ClimbPose->CopyPoseFromSkeletalComponent(GetMesh());
    if(!Climbing) {
        if(!Cast<UExplorerMotionMatching>(GetMesh()->GetAnimInstance())&&GetCharacterMovement()->IsMovingOnGround())ApplyDirectionalLocomotion(ClimbPose,ArmedLocomotionAnimations,Weapon,GetActorQuat().UnrotateVector(GetVelocity()),LocomotionPhase,ArmAnimationAlpha*Phase(15,60,GetVelocity().Size2D()));
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
        Presentation->BlendActionTransition(AnimationClock,0);ClimbPose->RefreshBoneTransforms();Presentation->StorePresentedPose(AnimationClock);UpdateCharacterVisual();return;
    }
    int32 ClipIndex=6;float ClipTime=FMath::Fmod(AnimationClock,1.5f);
    if(Traversal==ETraversalState::Probing){ClipIndex=bGroundProbe?0:(bLeadRight?2:1);ClipTime=ProbeTime;}
    else if(Traversal==ETraversalState::GripJump){ClipIndex=bLeadRight?4:3;ClipTime=ReachTime/ReachDuration*.72f;}
    else if(Traversal==ETraversalState::Catching){ClipIndex=5;ClipTime=CatchTime;}
    if(WallClimbAnimations.IsValidIndex(ClipIndex))ApplyWallClip(ClimbPose,WallClimbAnimations[ClipIndex],ClipTime);
    const FVector Side=FVector::CrossProduct(FVector::UpVector,-WallNormal),Body=GetActorLocation();
    const float Breath=FMath::Sin(AnimationClock*2.6f);
    FVector Hands[2]={Ledge-Side*24+WallNormal*3+FVector::UpVector*10,Ledge+Side*24+WallNormal*3+FVector::UpVector*10};
    auto FeetAt=[&](const FVector& At,int Index){
        const FVector Origin=At+Side*(Index==0?-24:24)-FVector(0,0,Index==0?30:36);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ClimbFoot),false,this);FHitResult Hit;
        if(GetWorld()->LineTraceSingleByChannel(Hit,Origin+WallNormal*15,Origin-WallNormal*150,ECC_Visibility,Query)&&Hit.Component.IsValid()&&Hit.Component->ComponentHasTag(TEXT("Climbable")))return Hit.ImpactPoint+WallNormal*6;
        return Origin-WallNormal*75;
    };
    FVector Feet[2]={FeetAt(Body,0),FeetAt(Body,1)};
    float Lean=5,LeanSide=Breath*1.2f,RootLower=0;
    if(Traversal==ETraversalState::Probing) {
        const float T=Phase(0,.45f,ProbeTime);
        const FVector To=ExpeditionTower::Grip(TargetGrip);
        const FVector From=bGroundProbe?To:ExpeditionTower::Grip(CurrentGrip);
        for(int Index=0;Index<2;Index++) {
            const FVector Offset=Side*(Index==0?-24:24)+WallNormal*3+FVector::UpVector*10;
            if(bGroundProbe) {
                // The exploratory reach deliberately stops short of a secure catch.
                const FVector Test=FMath::Lerp(GrabHands[Index],To+Offset,.70f);
                Hands[Index]=ReachArc(GrabHands[Index],Test,T,WallNormal,8);
                Feet[Index]=GrabFeet[Index];
            } else {
                const bool Lead=Index==(bLeadRight?1:0);
                Hands[Index]=Lead?ReachArc(From+Offset,FMath::Lerp(From+Offset,To+Offset,.52f)+WallNormal*9,T,WallNormal,7):From+Offset;
            }
        }
        LeanSide=(bLeadRight?1:-1)*4*T;Lean=bGroundProbe?3:8;
    } else if(Traversal==ETraversalState::GripJump) {
        const float T=FMath::Clamp(ReachTime/ReachDuration,0.f,1.f);
        const FVector To=ExpeditionTower::Grip(TargetGrip);
        for(int Index=0;Index<2;Index++) {
            const bool Lead=Index==(bLeadRight?1:0);const float H=Phase(Lead?.10f:.28f,Lead?.88f:1.f,T);
            const FVector Offset=Side*(Index==0?-24:24)+WallNormal*3+FVector::UpVector*10;
            Hands[Index]=ReachArc(GrabHands[Index],To+Offset,H,WallNormal,18);
            const float F=Phase(.16f,Index==0?.95f:1.f,T);
            Feet[Index]=ReachArc(GrabFeet[Index],FeetAt(ExpeditionTower::HangPosition(TargetGrip),Index),F,WallNormal,26);
        }
        LeanSide=(bLeadRight?1:-1)*7*FMath::Sin(PI*T);Lean=5+9*FMath::Sin(PI*T);
    } else if(Traversal==ETraversalState::Catching) {
        Lean=5+12*FMath::Sin(PI*FMath::Clamp(CatchTime/CatchDuration,0.f,1.f));
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
    const bool HasClips=WallClimbAnimations.IsValidIndex(ClipIndex)&&WallClimbAnimations[ClipIndex];
    Root.AddToTranslation(-WallNormal*(HasClips?12:40)*WallWeight+FVector(0,0,RootLower+(HasClips?0:8)*WallWeight+Breath*.6f));ClimbPose->SetBoneTransformByName(TEXT("root"),Root,EBoneSpaces::WorldSpace);
    RotateBone(ClimbPose,TEXT("spine_01"),Side,Lean);
    RotateBone(ClimbPose,TEXT("spine_02"),-WallNormal,LeanSide);
    Presentation->BlendActionTransition(AnimationClock,int32(Traversal));
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
    ClimbPose->RefreshBoneTransforms();Presentation->StorePresentedPose(AnimationClock);UpdateCharacterVisual();
}

void AExplorerCharacter::UpdateCharacterVisual() {
    if(!CharacterVisual||!CharacterVisual->GetCharacterMesh())return;
    const float DeltaTime=LastVisualAnimationClock<0?0:FMath::Max(0.f,AnimationClock-LastVisualAnimationClock);LastVisualAnimationClock=AnimationClock;
    CharacterVisual->Present(ClimbPose,DeltaTime);
    if(!CharacterVisual->IsRetargetReady())return;
    const FVector Side=GetActorRightVector(),Body=GetActorLocation();
    const FName Upper[2]={TEXT("mixamorig_LeftArm"),TEXT("mixamorig_RightArm")},Lower[2]={TEXT("mixamorig_LeftForeArm"),TEXT("mixamorig_RightForeArm")},Hand[2]={TEXT("mixamorig_LeftHand"),TEXT("mixamorig_RightHand")};
    const bool Wall=Traversal!=ETraversalState::Walking;
    if(Wall&&(!bGroundProbe||Traversal==ETraversalState::Catching))for(int32 Pass=0;Pass<4;Pass++) {
        FVector Correction=FVector::ZeroVector;int32 Count=0;
        for(int Index=0;Index<2;Index++) {
            const FVector Shoulder=CharacterVisual->GetBoneLocationByName(Upper[Index],EBoneSpaces::WorldSpace),Elbow=CharacterVisual->GetBoneLocationByName(Lower[Index],EBoneSpaces::WorldSpace),Wrist=CharacterVisual->GetBoneLocationByName(Hand[Index],EBoneSpaces::WorldSpace);
            const FVector Target=ClimbPose->GetBoneLocationByName(Index==0?TEXT("hand_l"):TEXT("hand_r"),EBoneSpaces::WorldSpace);
            const float Excess=FVector::Dist(Shoulder,Target)-(FVector::Dist(Shoulder,Elbow)+FVector::Dist(Elbow,Wrist)-1);
            if(Excess>0){Correction+=(Target-Shoulder).GetSafeNormal()*Excess;Count++;}
        }
        if(!Count)break;
        FTransform Root=CharacterVisual->GetBoneTransformByName(CharacterVisual->GetBoneName(0),EBoneSpaces::WorldSpace);Root.AddToTranslation((Correction/Count).GetClampedToMaxSize(12));CharacterVisual->SetBoneTransformByName(CharacterVisual->GetBoneName(0),Root,EBoneSpaces::WorldSpace);
    }
    for(int Index=0;Index<2;Index++) {
        const FName SourceHand=Index==0?TEXT("hand_l"):TEXT("hand_r");
        const FVector Target=ClimbPose->GetBoneLocationByName(SourceHand,EBoneSpaces::WorldSpace);
        // Match source contact/weapon landmarks after proportions are retargeted.
        const FVector Elbow=Wall?Body+Side*(Index==0?-65:65)+WallNormal*45+FVector(0,0,45):ClimbPose->GetBoneLocationByName(Index==0?TEXT("lowerarm_l"):TEXT("lowerarm_r"),EBoneSpaces::WorldSpace);
        if(Wall||ArmAnimationAlpha>.01f)SolveLimb(CharacterVisual,Upper[Index],Lower[Index],Hand[Index],Target,Elbow);
        if(Wall)SolveLimb(CharacterVisual,Index==0?TEXT("mixamorig_LeftUpLeg"):TEXT("mixamorig_RightUpLeg"),Index==0?TEXT("mixamorig_LeftLeg"):TEXT("mixamorig_RightLeg"),Index==0?TEXT("mixamorig_LeftFoot"):TEXT("mixamorig_RightFoot"),ClimbPose->GetBoneLocationByName(Index==0?TEXT("foot_l"):TEXT("foot_r"),EBoneSpaces::WorldSpace),Body-WallNormal*90+Side*(Index==0?-36:36)-FVector(0,0,20));
        if(Wall) {
            const float Weight=Traversal==ETraversalState::Mantling?1-Phase(.55f,1.f,MantleTime/MantleDuration):bGroundProbe&&Traversal==ETraversalState::Probing?Phase(0,.4f,ProbeTime):1;
            FTransform Wrist=CharacterVisual->GetBoneTransformByName(Hand[Index],EBoneSpaces::WorldSpace);
            // Diesel's fingers point along local Y; palm Z faces down onto the
            // stone's top surface. Flex around local X to hook the front edge.
            const FQuat Overhand=FRotationMatrix::MakeFromYZ(-WallNormal,-FVector::UpVector).ToQuat();
            Wrist.SetRotation(FQuat::Slerp(Wrist.GetRotation(),Overhand,Weight));CharacterVisual->SetBoneTransformByName(Hand[Index],Wrist,EBoneSpaces::WorldSpace);
            const bool Lead=Index==(bLeadRight?1:0);
            float Curl=65;
            if(Traversal==ETraversalState::Probing&&(bGroundProbe||Lead))Curl=12;
            else if(Traversal==ETraversalState::GripJump)Curl=FMath::Lerp(12.f,65.f,Phase(.65f,1.f,ReachTime/ReachDuration));
            const FReferenceSkeleton& Ref=CharacterVisual->GetCharacterMesh()->GetRefSkeleton();
            for(const TCHAR* Finger:{TEXT("Index"),TEXT("Middle"),TEXT("Ring"),TEXT("Pinky"),TEXT("Thumb")})for(int32 Joint=1;Joint<=3;Joint++) {
                const FName Bone(*FString::Printf(TEXT("mixamorig_%sHand%s%d"),Index==0?TEXT("Left"):TEXT("Right"),Finger,Joint));const int32 BoneIndex=Ref.FindBoneIndex(Bone);if(BoneIndex<0)continue;
                const float Angle=Finger==FString(TEXT("Thumb"))?Curl*.45f:Curl*(Joint==3?.65f:1.f);
                const FQuat GripRotation=Ref.GetRefBonePose()[BoneIndex].GetRotation()*FQuat(FVector::XAxisVector,FMath::DegreesToRadians(Angle));
                FTransform& Local=CharacterVisual->BoneSpaceTransforms[BoneIndex];Local.SetRotation(FQuat::Slerp(Local.GetRotation(),GripRotation,Weight));Local.NormalizeRotation();
            }
            CharacterVisual->MarkRefreshTransformDirty();CharacterVisual->RefreshBoneTransforms();
        }
    }
    CharacterVisual->RefreshBoneTransforms();
    FTransform Grip=ClimbPose->GetSocketTransform(TEXT("HandGrip_R"),RTS_World);
    Grip.AddToTranslation(CharacterVisual->GetBoneLocationByName(Hand[1],EBoneSpaces::WorldSpace)-ClimbPose->GetBoneLocationByName(TEXT("hand_r"),EBoneSpaces::WorldSpace));
    WeaponMesh->SetWorldTransform(Grip);
}

FString UExpeditionMotionMatchingLibrary::BuildWallClimbContent() {
#if WITH_EDITOR
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"));
    if(!Mesh||!GEditor)return TEXT("ERROR: mannequin source or editor unavailable");
    auto* Pose=NewObject<UPoseableMeshComponent>();Pose->SetSkinnedAssetAndUpdate(Mesh);Pose->RegisterComponentWithWorld(GEditor->GetEditorWorldContext().World());
    const FReferenceSkeleton& Ref=Mesh->GetRefSkeleton();
    const TCHAR* Names[]={TEXT("Probe_Ground"),TEXT("Probe_Left"),TEXT("Probe_Right"),TEXT("Jump_Left"),TEXT("Jump_Right"),TEXT("Catch"),TEXT("Hang")};
    const int32 Frames[]={18,18,18,22,22,10,45};
    FString Report;
    for(int32 Clip=0;Clip<7;Clip++) {
        TArray<TArray<FTransform>> Keys;Keys.SetNum(Ref.GetNum());
        for(int32 Frame=0;Frame<=Frames[Clip];Frame++) {
            const float T=float(Frame)/Frames[Clip],Breath=FMath::Sin(T*2*PI);
            const bool Ground=Clip==0,Probe=Clip==1||Clip==2,Leap=Clip==3||Clip==4,Right=Clip==2||Clip==4;
            const float Reach=Probe?FMath::SmoothStep(0.f,1.f,T):0;
            const float Compression=Leap?FMath::Sin(PI*FMath::Clamp(T/.3f,0.f,1.f)):(Clip==5?FMath::Sin(PI*T):0);
            Pose->BoneSpaceTransforms=Ref.GetRefBonePose();Pose->MarkRefreshTransformDirty();Pose->RefreshBoneTransforms();
            FTransform Root=Pose->GetBoneTransformByName(TEXT("root"),EBoneSpaces::ComponentSpace);
            Root.AddToTranslation(FVector(0,Ground?10:25,Ground?0:8-Compression*12+(Clip==6?Breath*.5f:0)));
            Pose->SetBoneTransformByName(TEXT("root"),Root,EBoneSpaces::ComponentSpace);
            RotateBone(Pose,TEXT("spine_01"),FVector::RightVector,Ground?7:10+Compression*12);
            RotateBone(Pose,TEXT("spine_03"),FVector::UpVector,(Right?1:-1)*(Probe?Reach*10:Leap?FMath::Sin(PI*T)*8:0));
            RotateBone(Pose,TEXT("neck_01"),FVector::RightVector,Ground?-8:-14);
            RotateBone(Pose,TEXT("head"),FVector::UpVector,Probe?(Right?1:-1)*Reach*12:0);
            for(int32 Side=0;Side<2;Side++) {
                const bool Lead=Side==(Right?1:0);const float X=Side==0?-24:24;
                FVector Hand(X,72,Ground?FMath::Lerp(115.f,173.f,FMath::SmoothStep(0.f,1.f,T)):185);
                if(Probe&&Lead)Hand+=FVector(0,5,30)*Reach;
                if(Leap)Hand+=FVector(0,0,Lead?24:12)*FMath::Sin(PI*T);
                FVector Foot(X,Ground?0:58,Ground?5:Side==0?65:59);
                if(Leap)Foot+=FVector(0,-12,12)*FMath::Sin(PI*T);
                SolveLimb(Pose,Side==0?TEXT("upperarm_l"):TEXT("upperarm_r"),Side==0?TEXT("lowerarm_l"):TEXT("lowerarm_r"),Side==0?TEXT("hand_l"):TEXT("hand_r"),Hand,FVector(Side==0?-65:65,-5,140));
                SolveLimb(Pose,Side==0?TEXT("thigh_l"):TEXT("thigh_r"),Side==0?TEXT("calf_l"):TEXT("calf_r"),Side==0?TEXT("foot_l"):TEXT("foot_r"),Foot,FVector(Side==0?-36:36,100,90));
                // Open fingertips while testing; curl around the grip on the catch.
                const float Curl=Ground?T*20:Probe&&Lead?20:Leap?FMath::Lerp(15.f,65.f,FMath::SmoothStep(.65f,1.f,T)):65;
                for(const TCHAR* Finger:{TEXT("index"),TEXT("middle"),TEXT("ring"),TEXT("pinky")})for(int32 Joint=1;Joint<=3;Joint++) {
                    const FName Bone(*FString::Printf(TEXT("%s_0%d_%s"),Finger,Joint,Side==0?TEXT("l"):TEXT("r")));
                    FTransform Local=Pose->GetBoneTransformByName(Bone,EBoneSpaces::ComponentSpace);
                    // Bone-local Y is the finger flexion axis on the template rig.
                    Local.SetRotation(Local.GetRotation()*FQuat(FVector::YAxisVector,FMath::DegreesToRadians(Curl)));
                    Pose->SetBoneTransformByName(Bone,Local,EBoneSpaces::ComponentSpace);
                }
            }
            for(int32 Bone=0;Bone<Ref.GetNum();Bone++)Keys[Bone].Add(Pose->BoneSpaceTransforms[Bone]);
        }
        const FString Path=FString(TEXT("/Game/Animation/WallClimb/AS_"))+Names[Clip];
        auto* Sequence=LoadObject<UAnimSequence>(nullptr,*Path);
        if(!Sequence){Sequence=NewObject<UAnimSequence>(CreatePackage(*Path),*FPackageName::GetShortName(Path),RF_Public|RF_Standalone);FAssetRegistryModule::AssetCreated(Sequence);}
        Sequence->SetSkeleton(Mesh->GetSkeleton());Sequence->SetPreviewMesh(Mesh);Sequence->bLoop=Clip==6;Sequence->bEnableRootMotion=false;
        IAnimationDataController& Controller=Sequence->GetController();Controller.OpenBracket(FText::FromString(TEXT("Author wall climb stages")),false);
        Controller.InitializeModel();Controller.SetFrameRate(FFrameRate(30,1),false);Controller.SetNumberOfFrames(FFrameNumber(Frames[Clip]),false);
        for(int32 Bone=0;Bone<Ref.GetNum();Bone++) {
            TArray<FVector3f> Positions,Scales;TArray<FQuat4f> Rotations;
            for(const FTransform& Key:Keys[Bone]){Positions.Add(FVector3f(Key.GetTranslation()));Rotations.Add(FQuat4f(Key.GetRotation()));Scales.Add(FVector3f(Key.GetScale3D()));}
            Controller.AddBoneCurve(Ref.GetBoneName(Bone),false);Controller.SetBoneTrackKeys(Ref.GetBoneName(Bone),Positions,Rotations,Scales,false);
        }
        Controller.NotifyPopulated();Controller.CloseBracket(false);Sequence->PostEditChange();
        auto* Package=Sequence->GetOutermost();Package->MarkPackageDirty();
        const FString Filename=FPackageName::LongPackageNameToFilename(Path,FPackageName::GetAssetPackageExtension());IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename),true);
        FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
        if(!UPackage::SavePackage(Package,Sequence,*Filename,Args)){Pose->DestroyComponent();return TEXT("ERROR: saving wall climb clip");}
        Report+=FString::Printf(TEXT("AS_%s: %d frames, %d full body tracks\n"),Names[Clip],Frames[Clip]+1,Ref.GetNum());
    }
    Pose->DestroyComponent();return Report+TEXT("WALL_CLIMB_SETUP_COMPLETE\n");
#else
    return TEXT("ERROR: setup requires an editor build");
#endif
}
