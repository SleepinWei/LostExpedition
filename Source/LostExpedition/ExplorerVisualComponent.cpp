#include "ExplorerVisualComponent.h"
#include "Retargeter/IKRetargetProcessor.h"
#include "Retargeter/IKRetargeter.h"
#include "Engine/SkeletalMesh.h"

struct FExplorerRetargetState { FIKRetargetProcessor Processor; FRetargetProfile Profile; };

UExplorerVisualComponent::UExplorerVisualComponent() {
    PrimaryComponentTick.bCanEverTick=false;
    Retarget=MakeShared<FExplorerRetargetState>();
}
UExplorerVisualComponent::~UExplorerVisualComponent()=default;
USkeletalMesh* UExplorerVisualComponent::GetCharacterMesh() const {return Cast<USkeletalMesh>(GetSkinnedAsset());}
bool UExplorerVisualComponent::IsRetargetReady() const {return Retarget&&Retarget->Processor.IsInitialized();}
void UExplorerVisualComponent::Present(UPoseableMeshComponent* Source,float DeltaTime) {
    if(!Source||!GetCharacterMesh()||!RetargetAsset)return;
    if(!Retarget->Processor.IsInitialized()) {
        FRetargetInitParameters Init;Init.SourceSkeletalMesh=Cast<USkeletalMesh>(Source->GetSkinnedAsset());Init.TargetSkeletalMesh=GetCharacterMesh();Init.RetargeterAsset=RetargetAsset;Init.CustomProfile=&Retarget->Profile;
        Retarget->Processor.Initialize(Init);
        CalibrateHands();
    }
    if(!IsRetargetReady())return;
    TArray<FTransform> Input=Source->GetComponentSpaceTransforms();
    Retarget->Processor.ApplySourceScaleToPose(Input);
    FRetargetRunParameters Params;Params.SourceGlobalPose=&Input;Params.Profile=&Retarget->Profile;Params.DeltaTime=FMath::Max(DeltaTime,0.f);
    const TArray<FTransform>& Output=Retarget->Processor.RunRetargeter(Params);
    const FReferenceSkeleton& Ref=GetCharacterMesh()->GetRefSkeleton();
    if(Output.Num()!=Ref.GetNum())return;
    for(int32 I=0;I<Output.Num();I++) {
        const int32 Parent=Ref.GetParentIndex(I);
        BoneSpaceTransforms[I]=Parent<0?Output[I]:Output[I].GetRelativeTransform(Output[Parent]);
        BoneSpaceTransforms[I].NormalizeRotation();
    }
    // Paragon includes skinned guns and an ultimate-weapon assembly. Their
    // gameplay bones are not part of Manny's motion; keep them collapsed while
    // the adventure's separate weapon component owns gun rendering/attachment.
    for(const TCHAR* Name:{TEXT("weapon_l"),TEXT("weapon_r"),TEXT("ult_weapon_l"),TEXT("ult_weapon_r"),TEXT("ult_root"),TEXT("grenade")}) {
        const int32 Index=Ref.FindBoneIndex(Name);
        if(Index!=INDEX_NONE)BoneSpaceTransforms[Index].SetScale3D(FVector::ZeroVector);
    }
    MarkRefreshTransformDirty();RefreshBoneTransforms();
}

void UExplorerVisualComponent::ResetSmoothing(){WallSpring.Reset();LastPresented.Reset();PreviousStage=-1;bWasWall=false;ExitBlendRemaining=0;}
void UExplorerVisualComponent::SmoothWallPose(float DeltaTime,bool Wall,int32 Stage) {
    if(Wall)ExitBlendRemaining=.25f;else ExitBlendRemaining=FMath::Max(0.f,ExitBlendRemaining-DeltaTime);
    if(Wall||ExitBlendRemaining>0) {
        if((Wall!=bWasWall||Stage!=PreviousStage)&&LastPresented.Num()==BoneSpaceTransforms.Num())WallSpring.Seed(LastPresented);
        WallSpring.Evaluate(BoneSpaceTransforms,DeltaTime,30);
        MarkRefreshTransformDirty();RefreshBoneTransforms();
    } else WallSpring.Reset();
    bWasWall=Wall;PreviousStage=Stage;
}
void UExplorerVisualComponent::StoreVisualPose(const FQuat* UnconstrainedWrists) {
    LastPresented=BoneSpaceTransforms;
    if(!UnconstrainedWrists)return;
    // A stage transition seeds the outgoing IK positions. Store wrists before
    // contact rotation, so partial contact is not applied a second time on entry.
    const FReferenceSkeleton& Ref=GetCharacterMesh()->GetRefSkeleton();
    for(int32 Side=0;Side<2;Side++) {
        const int32 Hand=GetBoneIndex(ResolveBone(Side==0?TEXT("hand_l"):TEXT("hand_r")));
        if(Hand<0)continue;
        const int32 Parent=Ref.GetParentIndex(Hand);
        const FQuat ParentWorld=GetComponentQuat()*GetComponentSpaceTransforms()[Parent].GetRotation();
        LastPresented[Hand].SetRotation(ParentWorld.Inverse()*UnconstrainedWrists[Side]);
        LastPresented[Hand].NormalizeRotation();
    }
}

FName UExplorerVisualComponent::ResolveBone(FName Canonical) const {
    if(GetBoneIndex(Canonical)!=INDEX_NONE)return Canonical;
    static const TMap<FName,FName> Legacy={
        {TEXT("pelvis"),TEXT("mixamorig_Hips")},{TEXT("spine_03"),TEXT("mixamorig_Spine2")},{TEXT("head"),TEXT("mixamorig_Head")},
        {TEXT("upperarm_l"),TEXT("mixamorig_LeftArm")},{TEXT("lowerarm_l"),TEXT("mixamorig_LeftForeArm")},{TEXT("hand_l"),TEXT("mixamorig_LeftHand")},
        {TEXT("upperarm_r"),TEXT("mixamorig_RightArm")},{TEXT("lowerarm_r"),TEXT("mixamorig_RightForeArm")},{TEXT("hand_r"),TEXT("mixamorig_RightHand")},
        {TEXT("thigh_l"),TEXT("mixamorig_LeftUpLeg")},{TEXT("calf_l"),TEXT("mixamorig_LeftLeg")},{TEXT("foot_l"),TEXT("mixamorig_LeftFoot")},
        {TEXT("thigh_r"),TEXT("mixamorig_RightUpLeg")},{TEXT("calf_r"),TEXT("mixamorig_RightLeg")},{TEXT("foot_r"),TEXT("mixamorig_RightFoot")}};
    if(const FName* Found=Legacy.Find(Canonical))return *Found;
    return Canonical;
}
void UExplorerVisualComponent::CalibrateHands() {
    const FReferenceSkeleton& Ref=GetCharacterMesh()->GetRefSkeleton();
    TArray<FTransform> Global=Ref.GetRefBonePose();
    for(int32 I=1;I<Global.Num();I++)Global[I]=Global[I]*Global[Ref.GetParentIndex(I)];
    const bool Legacy=GetBoneIndex(TEXT("mixamorig_Hips"))!=INDEX_NONE;
    FingerAxes.Reset();
    for(int32 Side=0;Side<2;Side++) {
        auto FingerName=[&](const TCHAR* Finger,int32 Joint) {
            if(Legacy)return FName(*FString::Printf(TEXT("mixamorig_%sHand%s%d"),Side==0?TEXT("Left"):TEXT("Right"),Finger,Joint));
            return FName(*FString::Printf(TEXT("%s_%02d_%s"),*FString(Finger).ToLower(),Joint,Side==0?TEXT("l"):TEXT("r")));
        };
        const int32 Hand=Ref.FindBoneIndex(ResolveBone(Side==0?TEXT("hand_l"):TEXT("hand_r")));
        const int32 Index=Ref.FindBoneIndex(FingerName(TEXT("Index"),1)),Pinky=Ref.FindBoneIndex(FingerName(TEXT("Pinky"),1));
        if(Hand<0||Index<0||Pinky<0)continue;
        FVector Forward=Global[Hand].InverseTransformVectorNoScale((Global[Index].GetLocation()+Global[Pinky].GetLocation())*.5-Global[Hand].GetLocation()).GetSafeNormal();
        const FVector Across=Global[Hand].InverseTransformVectorNoScale(Global[Pinky].GetLocation()-Global[Index].GetLocation()).GetSafeNormal();
        FVector Palm=FVector::CrossProduct(Forward,Across).GetSafeNormal()*(Side==0?1:-1);
        if(Legacy){Forward=FVector::YAxisVector;Palm=FVector::ZAxisVector;}
        HandFrames[Side]=FRotationMatrix::MakeFromXZ(Forward,Palm).ToQuat();
        for(const TCHAR* Finger:{TEXT("Index"),TEXT("Middle"),TEXT("Ring"),TEXT("Pinky"),TEXT("Thumb")})for(int32 Joint=1;Joint<=3;Joint++) {
            const int32 Bone=Ref.FindBoneIndex(FingerName(Finger,Joint));if(Bone<0)continue;
            const FVector LocalPalm=Global[Bone].InverseTransformVectorNoScale(Global[Hand].TransformVectorNoScale(Palm));
            const int32 Child=Ref.FindBoneIndex(FingerName(Finger,Joint+1));
            const FVector LocalForward=Child>=0?Ref.GetRefBonePose()[Child].GetTranslation().GetSafeNormal():Global[Bone].InverseTransformVectorNoScale(Global[Hand].TransformVectorNoScale(Forward));
            FingerAxes.Add(Bone,Legacy?FVector::XAxisVector:FVector::CrossProduct(LocalForward,LocalPalm).GetSafeNormal());
        }
    }
}
FQuat UExplorerVisualComponent::ContactWrist(int32 Side,const FVector& Forward,const FVector& Palm) const {
    return FRotationMatrix::MakeFromXZ(Forward,Palm).ToQuat()*HandFrames[Side].Inverse();
}
void UExplorerVisualComponent::CurlFingers(int32 Side,float Degrees,float Weight) {
    const FReferenceSkeleton& Ref=GetCharacterMesh()->GetRefSkeleton();
    const int32 Hand=Ref.FindBoneIndex(ResolveBone(Side==0?TEXT("hand_l"):TEXT("hand_r")));
    for(const auto& Pair:FingerAxes) {
        int32 Ancestor=Pair.Key;while(Ancestor>0&&Ancestor!=Hand)Ancestor=Ref.GetParentIndex(Ancestor);
        if(Ancestor!=Hand)continue;
        const FString Name=Ref.GetBoneName(Pair.Key).ToString();
        const float Angle=Degrees*(Name.Contains(TEXT("thumb"),ESearchCase::IgnoreCase)?.45f:(Name.EndsWith(TEXT("3"))||Name.Contains(TEXT("_03_")))?.65f:1.f);
        const FQuat Grip=Ref.GetRefBonePose()[Pair.Key].GetRotation()*FQuat(Pair.Value,FMath::DegreesToRadians(Angle));
        FTransform& Local=BoneSpaceTransforms[Pair.Key];Local.SetRotation(FQuat::Slerp(Local.GetRotation(),Grip,Weight));Local.NormalizeRotation();
    }
}
