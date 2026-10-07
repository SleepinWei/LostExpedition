#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/TrajectoryTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ExplorerMotionMatching.generated.h"

// The generated AnimGraph evaluates Epic's real Motion Matching node. This class
// supplies game-thread trajectory data and reads the evaluated selection for diagnostics.
UCLASS(Transient,Blueprintable)
class LOSTEXPEDITION_API UExplorerMotionMatching : public UAnimInstance {
    GENERATED_BODY()
public:
    UExplorerMotionMatching();
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    virtual void NativePostEvaluateAnimation() override;
    UPROPERTY(BlueprintReadOnly,Category="Motion Matching") FTransformTrajectory MotionTrajectory;
    UPROPERTY(BlueprintReadOnly,Category="Motion Matching") TObjectPtr<class UPoseSearchDatabase> ActiveDatabase;
    UPROPERTY(BlueprintReadOnly,Category="Motion Matching") bool bUseAirPose=false;
    UPROPERTY(BlueprintReadOnly,Category="Motion Matching") TObjectPtr<class UAnimSequence> AirSequence;
    UPROPERTY(BlueprintReadOnly,Category="Motion Matching") float AirPoseTime=0;
    UPROPERTY(BlueprintReadOnly,Category="Motion Matching") bool bHasMatchedPose=false;
    UPROPERTY(BlueprintReadOnly,Category="Motion Matching") FString MatchedAnimation;
    UPROPERTY(BlueprintReadOnly,Category="Motion Matching") FString MatchedDatabase;
    UPROPERTY(BlueprintReadOnly,Category="Motion Matching") float MatchedTime=0,MatchCost=0;
    UPROPERTY(BlueprintReadOnly,Category="Motion Matching") int32 SelectionChanges=0,EvaluatedFrames=0;
    UPROPERTY() TArray<TObjectPtr<class UPoseSearchDatabase>> Databases;
    UPROPERTY() TObjectPtr<class UAnimSequence> JumpSequence;
    UPROPERTY() TObjectPtr<class UAnimSequence> FallSequence;
    UPROPERTY() TObjectPtr<class UAnimSequence> LandSequence;
    void ResetTrajectory();
private:
    TArray<FTransformTrajectorySample> History;
    float TrajectoryClock=0,LandingRemaining=0;
    bool bWasFalling=false;
};

UCLASS()
class LOSTEXPEDITION_API UExpeditionMotionMatchingLibrary : public UBlueprintFunctionLibrary {
    GENERATED_BODY()
public:
    // Creates only the project's generated animation content; template assets remain intact.
    UFUNCTION(BlueprintCallable,Category="Expedition|Setup",meta=(DevelopmentOnly))
    static FString BuildMotionMatchingContent();
    UFUNCTION(BlueprintCallable,Category="Expedition|Setup",meta=(DevelopmentOnly))
    static FString BuildWallClimbContent();
};
