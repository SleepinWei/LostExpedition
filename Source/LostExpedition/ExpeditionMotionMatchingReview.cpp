#include "ExpeditionGameMode.h"
#include "ExplorerCharacter.h"
#include "ExplorerMotionMatching.h"
#include "IslandTerrain.h"
#include "Camera/CameraActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "TimerManager.h"

void AExpeditionGameMode::CaptureMotionMatchingFrame() {
    constexpr float DT=1.f/24;const int32 F=AnimationReviewFrame;
    if(TActorIterator<AExplorerCharacter> It(GetWorld());It) {
        auto* P=*It;auto* PC=Cast<APlayerController>(P->GetController());
        if(F==0) {
            IFileManager::Get().MakeDirectory(*(FPaths::ProjectDir()/TEXT("Docs/MotionMatchingFrames")),true);
            P->Drop();P->Traversal=ETraversalState::Walking;P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            P->SetActorLocation(FVector(2200,-1200,IslandTerrain::Height(2200,-1200)+99));P->SetActorRotation(FRotator::ZeroRotator);
            P->bAim=false;P->bFiring=false;P->FireAnimationTime=P->EquipAnimationTime=-1;P->bReloading=false;P->LedgeCooldown=100;
            if(PC)PC->SetControlRotation(FRotator::ZeroRotator);
            if(auto* MM=Cast<UExplorerMotionMatching>(P->GetMesh()->GetAnimInstance()))MM->ResetTrajectory();
        }
        FVector2D Input=FVector2D::ZeroVector;
        if(F>=12&&F<60)Input.X=1;
        if(F==24)P->SprintStart();if(F==60)P->SprintStop();
        if(F==60&&PC)PC->SetControlRotation(FRotator(0,90,0));
        if(F>=60&&F<96)Input.X=1;
        if(F==120){P->Pistol();P->bAim=true;if(PC)PC->SetControlRotation(FRotator::ZeroRotator);}
        if(F>=128&&F<168)Input.Y=1;
        if(F==140||F==152){P->Magazine[0]=12;P->ShotCooldown=0;P->FireShot();}
        if(F==168)P->Rifle();if(F>=176&&F<216)Input.X=-1;
        if(F==186){P->Magazine[1]=30;P->StartFire();}if(F==210)P->StopFire();
        if(F==224){P->bAim=false;P->GetCharacterMovement()->StopMovementImmediately();P->Jump();}
        P->Forward(Input.X);P->Right(Input.Y);P->Tick(DT);
        P->GetCharacterMovement()->TickComponent(DT,LEVELTICK_All,nullptr);
        P->GetMesh()->TickAnimation(DT,false);P->GetMesh()->RefreshBoneTransforms();P->UpdateClimbPose();
        if(F%12==0)if(auto* MM=Cast<UExplorerMotionMatching>(P->GetMesh()->GetAnimInstance()))
            UE_LOG(LogTemp,Display,TEXT("MM_VISUAL frame=%d selected=%s db=%s time=%.3f cost=%.3f speed=%.1f air=%d"),F,*MM->MatchedAnimation,*MM->MatchedDatabase,MM->MatchedTime,MM->MatchCost,P->GetVelocity().Size2D(),MM->bUseAirPose);
        const FVector Center=P->GetActorLocation()+FVector(0,0,5),Offset(-400,-320,180);
        AnimationReviewCamera->SetActorLocation(Center+Offset);AnimationReviewCamera->SetActorRotation((-Offset).Rotation());
        FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/FString::Printf(TEXT("Docs/MotionMatchingFrames/%04d.png"),F)),true,false);
    }
    AnimationReviewFrame++;
    if(AnimationReviewFrame>=288)GetWorldTimerManager().SetTimer(AnimationReviewTimer,FTimerDelegate::CreateWeakLambda(this,[](){FPlatformMisc::RequestExit(false);}),1.f,false);
    else GetWorldTimerManager().SetTimer(AnimationReviewTimer,this,&AExpeditionGameMode::CaptureAnimationFrame,.16f,false);
}
