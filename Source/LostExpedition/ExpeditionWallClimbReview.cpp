#include "ExpeditionGameMode.h"
#include "ExplorerCharacter.h"
#include "ExplorerVisualComponent.h"
#include "ExplorerPoseComponent.h"
#include "ExpeditionTower.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

void AExpeditionGameMode::CaptureWallClimbFrame() {
    IFileManager::Get().MakeDirectory(*(FPaths::ProjectDir()/TEXT("Docs/WallClimbFrames")),true);
    if(TActorIterator<AExplorerCharacter> It(GetWorld());It) {
        auto* P=*It;const int32 F=AnimationReviewFrame;
        if(F==0) {
            P->Drop();P->Traversal=ETraversalState::Walking;P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            P->SetActorLocation(ExpeditionTower::Grip(0)+FVector(-88,0,-121));P->SetActorRotation(FRotator::ZeroRotator);P->LedgeCooldown=0;P->BeginWallGrip();
        }
        if(F==18)P->JumpOrClimb();
        if(F==54)P->Forward(1);
        if(F==74){P->JumpOrClimb();P->Forward(0);}
        if(F==112) {
            P->Drop();P->Traversal=ETraversalState::Clinging;P->CurrentGrip=P->TargetGrip=8;P->bGroundProbe=false;
            P->Ledge=ExpeditionTower::Grip(8);P->WallNormal=ExpeditionTower::WallNormal;P->SetActorLocation(ExpeditionTower::HangPosition(8));P->SetActorRotation(FRotator::ZeroRotator);P->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
            CastChecked<UExplorerPoseComponent>(P->ClimbPose)->ResetTransition();
        }
        if(F==120)P->Right(1);
        if(F==140){P->JumpOrClimb();P->Right(0);}
        P->Tick(1.f/30);P->GetMesh()->TickAnimation(1.f/30,false);P->GetMesh()->RefreshBoneTransforms();P->UpdateClimbPose();
        const FVector Center=P->CharacterVisual->GetBoneLocationByName(TEXT("mixamorig_Hips"),EBoneSpaces::WorldSpace)+FVector(0,0,28);
        const FVector Offset(-320,-240,70);AnimationReviewCamera->SetActorLocation(Center+Offset);AnimationReviewCamera->SetActorRotation((-Offset).Rotation());
        UE_LOG(LogTemp,Display,TEXT("WALL_CLIMB_REVIEW frame=%d stage=%d probe=%.3f leap=%.3f catch=%.3f current=%d target=%d"),F,int32(P->Traversal),P->ProbeTime,P->ReachTime,P->CatchTime,P->CurrentGrip,P->TargetGrip);
        FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/FString::Printf(TEXT("Docs/WallClimbFrames/%04d.png"),F)),true,false);
    }
    AnimationReviewFrame++;
    if(AnimationReviewFrame>=180)GetWorldTimerManager().SetTimer(AnimationReviewTimer,FTimerDelegate::CreateWeakLambda(this,[](){FPlatformMisc::RequestExit(false);}),1.f,false);
    else GetWorldTimerManager().SetTimer(AnimationReviewTimer,this,&AExpeditionGameMode::CaptureAnimationFrame,.16f,false);
}
