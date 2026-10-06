#include "ExpeditionGameMode.h"
#include "ExplorerCharacter.h"
#include "ExpeditionActors.h"
#include "ExpeditionTower.h"
#include "ExpeditionWorld.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

// Fixed animation timesteps produce repeatable visual clips on slower editor machines.
void AExpeditionGameMode::StartAnimationReview() {
    if(FParse::Param(FCommandLine::Get(),TEXT("AnimationReviewFirearmsOnly")))AnimationReviewFrame=160;
    IFileManager::Get().MakeDirectory(*(FPaths::ProjectDir()/TEXT("Docs/AnimationFrames")),true);
    for(TActorIterator<AExpeditionGuard> It(GetWorld());It;++It)It->bTrainingTarget=true;
    GEngine->Exec(GetWorld(),TEXT("DisableAllScreenMessages"));
    // Remove near-camera palm fronds only in this isolated capture run.
    for(TActorIterator<AExpeditionWorld> It(GetWorld());It;++It)for(auto Part:It->Pieces)
        if(Part&&Part->GetName().Contains(TEXT("IslandPalms")))Part->SetVisibility(false);
    AnimationReviewCamera=GetWorld()->SpawnActor<ACameraActor>();
    AnimationReviewCamera->GetCameraComponent()->FieldOfView=65;
    if(TActorIterator<AExplorerCharacter> It(GetWorld());It) {
        It->NoticeRemaining=0;It->SetActorTickEnabled(false);It->GetCharacterMovement()->SetComponentTickEnabled(false);
        if(auto* PC=Cast<APlayerController>(It->GetController())){PC->SetViewTarget(AnimationReviewCamera);if(PC->GetHUD())PC->GetHUD()->bShowHUD=false;}
    }
    CaptureAnimationFrame();
}
void AExpeditionGameMode::CaptureAnimationFrame() {
    if(TActorIterator<AExplorerCharacter> It(GetWorld());It) {
        auto* P=*It;const int32 Frame=AnimationReviewFrame;
        auto Grip=[&](int32 Index){
            P->Drop();P->Traversal=ETraversalState::Walking;P->SetActorLocation(ExpeditionTower::HangPosition(Index)+ExpeditionTower::WallNormal*40);
            P->SetActorRotation(FRotator::ZeroRotator);P->LedgeCooldown=0;P->BeginHang();
        };
        if(Frame==0)Grip(4);
        if(Frame==8){P->GripCooldown=0;P->MoveWallGrip(0,1);}
        if(Frame==40)Grip(8);
        if(Frame==48){P->GripCooldown=0;P->MoveWallGrip(1,0);}
        if(Frame==80)Grip(ExpeditionTower::Steps-1);
        if(Frame==88)P->BeginMantle();
        if(Frame==120) {
            P->Drop();P->Traversal=ETraversalState::Walking;
            P->SetActorLocation(ExpeditionTower::Grip(ExpeditionTower::Steps-1)+FVector(90,0,99));
            P->SetActorRotation(FRotator(0,180,0));P->LedgeCooldown=0;P->BeginHang();
        }
        if(Frame==150){P->GripCooldown=0;P->MoveWallGrip(0,-1);}
        if(Frame==160||Frame==200) {
            P->Drop();P->Traversal=ETraversalState::Walking;P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            P->SetActorLocation(ExpeditionTower::Start()+FVector(-300,0,0));P->SetActorRotation(FRotator::ZeroRotator);
            P->Weapon=Frame==160?0:1;Frame==160?P->Pistol():P->Rifle();P->Magazine[P->Weapon]=P->Capacity[P->Weapon];
            P->ShotCooldown=0;P->FireAnimationTime=-1;P->bAim=true;P->bFiring=false;P->LedgeCooldown=100;
            if(auto* PC=Cast<APlayerController>(P->GetController()))PC->SetControlRotation(FRotator::ZeroRotator);
        }
        if(Frame==168||Frame==180||Frame==192)P->FireShot();
        if(Frame==208)P->StartFire();if(Frame==236)P->StopFire();
        P->Tick(1.f/24);P->UpdateClimbPose();
        const FVector Body=P->GetActorLocation();
        const FVector Center=Frame<160&&P->ClimbPose->IsVisible()?P->ClimbPose->GetBoneLocationByName(TEXT("pelvis"),EBoneSpaces::WorldSpace)+FVector(0,0,20):Body+FVector(0,0,10);
        const FVector Offset=Frame<160?FVector(-340,-260,90):FVector(340,-260,80);
        AnimationReviewCamera->SetActorLocation(Center+Offset);
        AnimationReviewCamera->SetActorRotation((-Offset).Rotation());
        FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/FString::Printf(TEXT("Docs/AnimationFrames/%04d.png"),Frame)),true,false);
    }
    AnimationReviewFrame++;
    if(AnimationReviewFrame>=240) {
        GetWorldTimerManager().SetTimer(AnimationReviewTimer,FTimerDelegate::CreateWeakLambda(this,[](){FPlatformMisc::RequestExit(false);}),1.f,false);
    } else GetWorldTimerManager().SetTimer(AnimationReviewTimer,this,&AExpeditionGameMode::CaptureAnimationFrame,.16f,false);
}
