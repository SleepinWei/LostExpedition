#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "ExpeditionGameMode.generated.h"
UCLASS()
class LOSTEXPEDITION_API AExpeditionGameMode : public AGameModeBase {
    GENERATED_BODY()
public:
    AExpeditionGameMode();
    virtual void BeginPlay() override;
    void RunTestWhenReady();
    FTimerHandle TestTimer;
private:
    void StartAnimationReview();
    void CaptureAnimationFrame();
    void CaptureMotionMatchingFrame();
    FTimerHandle AnimationReviewTimer;
    int32 AnimationReviewFrame=0;
    UPROPERTY() TObjectPtr<class ACameraActor> AnimationReviewCamera;
};
UCLASS()
class LOSTEXPEDITION_API AExpeditionHUD : public AHUD {
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};
