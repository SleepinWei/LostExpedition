#include "ExplorerVisualComponent.h"
#include "Retargeter/IKRetargetProcessor.h"
#include "Retargeter/IKRetargeter.h"
#include "Engine/SkeletalMesh.h"

struct FExplorerRetargetState { FIKRetargetProcessor Processor; FRetargetProfile Profile; };

UExplorerVisualComponent::UExplorerVisualComponent() {
    PrimaryComponentTick.bCanEverTick=false;
    Retarget=MakeShared<FExplorerRetargetState>();
    RetargetAsset=LoadObject<UIKRetargeter>(nullptr,TEXT("/Game/Explorer/RTG_Explorer_Diesel"));
}
UExplorerVisualComponent::~UExplorerVisualComponent()=default;
USkeletalMesh* UExplorerVisualComponent::GetCharacterMesh() const {return Cast<USkeletalMesh>(GetSkinnedAsset());}
bool UExplorerVisualComponent::IsRetargetReady() const {return Retarget&&Retarget->Processor.IsInitialized();}
void UExplorerVisualComponent::Present(UPoseableMeshComponent* Source,float DeltaTime) {
    if(!Source||!GetCharacterMesh()||!RetargetAsset)return;
    if(!Retarget->Processor.IsInitialized()) {
        FRetargetInitParameters Init;Init.SourceSkeletalMesh=Cast<USkeletalMesh>(Source->GetSkinnedAsset());Init.TargetSkeletalMesh=GetCharacterMesh();Init.RetargeterAsset=RetargetAsset;Init.CustomProfile=&Retarget->Profile;
        Retarget->Processor.Initialize(Init);
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
    MarkRefreshTransformDirty();RefreshBoneTransforms();
}
