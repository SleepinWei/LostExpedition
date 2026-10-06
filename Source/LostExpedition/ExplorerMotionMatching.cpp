#include "ExplorerMotionMatching.h"
#include "ExplorerCharacter.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PoseSearch/AnimNode_MotionMatching.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "UObject/UnrealType.h"

UExplorerMotionMatching::UExplorerMotionMatching() {
    // Extract animation root motion for pose evaluation, but let CharacterMovement
    // and collision-safe traversal remain the only capsule movement authorities.
    RootMotionMode=ERootMotionMode::IgnoreRootMotion;
    for(const TCHAR* Type:{TEXT("Unarmed"),TEXT("Pistol"),TEXT("Rifle")})
        Databases.Add(LoadObject<UPoseSearchDatabase>(nullptr,*FString::Printf(TEXT("/Game/Animation/MotionMatching/PSD_%s"),Type)));
    JumpSequence=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Jump"));
    FallSequence=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Fall_Loop"));
    LandSequence=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Land"));
}
void UExplorerMotionMatching::ResetTrajectory() {
    History.Reset();MotionTrajectory.Samples.Reset();TrajectoryClock=0;
    LandingRemaining=0;bWasFalling=false;AirPoseTime=0;
}
void UExplorerMotionMatching::NativeInitializeAnimation() {
    Super::NativeInitializeAnimation();ResetTrajectory();
    ActiveDatabase=Databases.IsValidIndex(0)?Databases[0]:nullptr;
}
void UExplorerMotionMatching::NativeUpdateAnimation(float DT) {
    Super::NativeUpdateAnimation(DT);
    auto* Character=Cast<AExplorerCharacter>(TryGetPawnOwner());if(!Character||DT<=0)return;
    auto* Movement=Character->GetCharacterMovement();auto* Mesh=GetSkelMeshComponent();
    const FTransform Current=Mesh->GetComponentTransform();
    if(History.Num()>0&&FVector::Dist(History.Last().Position,Current.GetLocation())>800)ResetTrajectory();
    const bool Traversing=Character->Traversal!=ETraversalState::Walking;
    const bool Armed=!Traversing&&(Character->bAim||Character->FireAnimationTime>=0||Character->bReloading||Character->EquipAnimationTime>=0||Character->ArmAnimationAlpha>.03f);
    ActiveDatabase=Databases.IsValidIndex(Armed?Character->Weapon+1:0)?Databases[Armed?Character->Weapon+1:0]:nullptr;
    // A matching result may continue in a database no longer allowed unless the
    // database switch explicitly interrupts it. Keep armed/unarmed pose domains strict.
    if(const IAnimClassInterface* Interface=IAnimClassInterface::GetFromClass(GetClass()))
        for(const FStructProperty* Property:Interface->GetAnimNodeProperties())
            if(Property->Struct==FAnimNode_MotionMatching::StaticStruct())
                Property->ContainerPtrToValuePtr<FAnimNode_MotionMatching>(this)->SetDatabaseToSearch(ActiveDatabase,EPoseSearchInterruptMode::InterruptOnDatabaseChange);
    const bool Falling=!Traversing&&Movement->IsFalling();
    if(bWasFalling&&!Falling&&!Traversing)LandingRemaining=.25f;
    if(Traversing)LandingRemaining=0;
    UAnimSequence* NewAir=Falling?(Movement->Velocity.Z>80?JumpSequence.Get():FallSequence.Get()):LandSequence.Get();
    if(AirSequence!=NewAir)AirPoseTime=0;else AirPoseTime+=DT;
    AirSequence=NewAir;bUseAirPose=Falling||LandingRemaining>0;
    if(AirSequence)AirPoseTime=AirSequence==FallSequence?FMath::Fmod(AirPoseTime,AirSequence->GetPlayLength()):FMath::Min(AirPoseTime,AirSequence->GetPlayLength());
    if(!Falling&&LandingRemaining>0&&LandSequence)AirPoseTime=(.25f-LandingRemaining)/.25f*LandSequence->GetPlayLength();
    LandingRemaining=FMath::Max(0.f,LandingRemaining-DT);bWasFalling=Falling;

    TrajectoryClock+=DT;
    FTransformTrajectorySample Recorded;Recorded.SetTransform(Current);Recorded.TimeInSeconds=TrajectoryClock;History.Add(Recorded);
    while(History.Num()>2&&History[1].TimeInSeconds<TrajectoryClock-.65f)History.RemoveAt(0);
    const float AnchorTime=FMath::Max(0.f,TrajectoryClock-DT);
    auto Historical=[&](float Time){
        FTransformTrajectorySample Result=History[0];
        for(int32 I=1;I<History.Num();I++) {
            if(History[I].TimeInSeconds>=Time) {
                const float Span=History[I].TimeInSeconds-History[I-1].TimeInSeconds;
                Result=History[I-1].Lerp(History[I],FMath::Clamp((Time-History[I-1].TimeInSeconds)/FMath::Max(Span,SMALL_NUMBER),0.f,1.f));return Result;
            }
        }
        return History.Last();
    };
    // A zero-time sample describes the previous evaluated character pose, matching
    // the collector's query convention. Negative samples use actual recorded movement.
    MotionTrajectory.Samples.Reset();
    for(float Offset:{-.3f,-.15f,0.f}) {
        auto Sample=Historical(AnchorTime+Offset);Sample.TimeInSeconds=Offset;MotionTrajectory.Samples.Add(Sample);
    }
    FVector Position=MotionTrajectory.Samples.Last().Position;
    FVector Velocity=Traversing?FVector::ZeroVector:Movement->Velocity;Velocity.Z=0;
    FVector Acceleration=Traversing?FVector::ZeroVector:Movement->GetCurrentAcceleration();Acceleration.Z=0;
    FRotator Facing=(MotionTrajectory.Samples.Last().Facing*Mesh->GetRelativeRotation().Quaternion().Inverse()).Rotator();Facing.Pitch=Facing.Roll=0;
    const float MaxSpeed=Movement->MaxWalkSpeed,MaxAcceleration=Movement->GetMaxAcceleration();
    Acceleration=Acceleration.GetClampedToMaxSize(MaxAcceleration);
    float SimTime=0;
    for(float Offset:{.1f,.25f,.4f,.6f,.85f}) {
        while(SimTime<Offset-SMALL_NUMBER) {
            const float Step=FMath::Min(1.f/60,Offset-SimTime);
            if(Acceleration.IsNearlyZero()) {
                const float Speed=Velocity.Size();
                Velocity=Velocity.GetSafeNormal()*FMath::Max(0.f,Speed-(Movement->BrakingDecelerationWalking+Movement->GroundFriction*Speed)*Step);
            } else Velocity=(Velocity+Acceleration*Step).GetClampedToMaxSize(MaxSpeed);
            Position+=Velocity*Step;
            float DesiredYaw=Facing.Yaw;
            if(Armed&&Character->GetController())DesiredYaw=Character->GetController()->GetControlRotation().Yaw;
            else if(!Velocity.IsNearlyZero())DesiredYaw=Velocity.Rotation().Yaw;
            Facing.Yaw=FMath::FixedTurn(Facing.Yaw,DesiredYaw,Movement->RotationRate.Yaw*Step);
            SimTime+=Step;
        }
        FTransformTrajectorySample Sample;Sample.Position=Position;Sample.Facing=Facing.Quaternion()*Mesh->GetRelativeRotation().Quaternion();Sample.TimeInSeconds=Offset;MotionTrajectory.Samples.Add(Sample);
    }
}
void UExplorerMotionMatching::NativePostEvaluateAnimation() {
    Super::NativePostEvaluateAnimation();EvaluatedFrames++;bHasMatchedPose=false;
    if(const IAnimClassInterface* Interface=IAnimClassInterface::GetFromClass(GetClass())) {
        for(const FStructProperty* Property:Interface->GetAnimNodeProperties()) {
            if(Property->Struct!=FAnimNode_MotionMatching::StaticStruct())continue;
            const auto* Node=Property->ContainerPtrToValuePtr<FAnimNode_MotionMatching>(this);
            const auto& Result=Node->GetMotionMatchingState().SearchResult;
            bHasMatchedPose=Result.SelectedAnim!=nullptr&&Result.SelectedDatabase!=nullptr;
            const FString Name=GetNameSafe(Result.SelectedAnim.Get());
            if(Name!=MatchedAnimation&&bHasMatchedPose)SelectionChanges++;
            MatchedAnimation=Name;MatchedDatabase=GetNameSafe(Result.SelectedDatabase.Get());MatchedTime=Result.SelectedTime;MatchCost=Result.SearchCost;
            break;
        }
    }
}
