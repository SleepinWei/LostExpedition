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
        if(F==36)P->JumpOrClimb();
        if(F==110)P->Forward(1);
        if(F==132){P->JumpOrClimb();P->Forward(0);}
        if(F==148){P->MoveWallGrip(0,1);P->JumpOrClimb();}
        if(F==348){P->MoveWallGrip(0,1);P->JumpOrClimb();}
        if(F==300||F==480) {
            P->Drop();P->Traversal=ETraversalState::Clinging;P->CurrentGrip=P->TargetGrip=F==480?ExpeditionTower::Steps-1:8;P->bGroundProbe=false;
            P->Ledge=ExpeditionTower::Grip(P->CurrentGrip);P->WallNormal=ExpeditionTower::WallNormal;P->SetActorLocation(ExpeditionTower::HangPosition(P->CurrentGrip));P->SetActorRotation(FRotator::ZeroRotator);P->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
            for(int32 I=0;I<2;I++)P->PlantedFeet[I]=P->FindWallFoot(P->GetActorLocation(),I);
            CastChecked<UExplorerPoseComponent>(P->ClimbPose)->ResetTransition();P->CharacterVisual->ResetSmoothing();
        }
        if(F==510)P->BeginMantle();
        if(F==318)P->Right(1);
        if(F==332){P->JumpOrClimb();P->Right(0);}
        P->Tick(1.f/60);P->GetMesh()->TickAnimation(1.f/60,false);P->GetMesh()->RefreshBoneTransforms();P->UpdateClimbPose();
        const FVector Center=P->CharacterVisual->GetBoneLocationByName(P->CharacterVisual->ResolveBone(TEXT("pelvis")),EBoneSpaces::WorldSpace)+FVector(0,0,28);
        const FVector Offset(-320,-240,70);AnimationReviewCamera->SetActorLocation(Center+Offset);AnimationReviewCamera->SetActorRotation((-Offset).Rotation());
        UE_LOG(LogTemp,Display,TEXT("WALL_CLIMB_REVIEW frame=%d stage=%d probe=%.3f leap=%.3f catch=%.3f current=%d target=%d"),F,int32(P->Traversal),P->ProbeTime,P->ReachTime,P->CatchTime,P->CurrentGrip,P->TargetGrip);
        // A timer alone can replace a pending screenshot on a slow render.
        // Advance simulation only after the current screenshot was processed.
        auto Handle=MakeShared<FDelegateHandle>();
        *Handle=FScreenshotRequest::OnScreenshotRequestProcessed().AddWeakLambda(this,[this,Handle]() {
            FScreenshotRequest::OnScreenshotRequestProcessed().Remove(*Handle);
            if(AnimationReviewFrame>=630)GetWorldTimerManager().SetTimer(AnimationReviewTimer,FTimerDelegate::CreateWeakLambda(this,[](){FPlatformMisc::RequestExit(false);}),.3f,false);
            else GetWorldTimerManager().SetTimer(AnimationReviewTimer,this,&AExpeditionGameMode::CaptureAnimationFrame,.03f,false);
        });
        AnimationReviewFrame++;
        FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/FString::Printf(TEXT("Docs/WallClimbFrames/%04d.png"),F)),true,false);
    }
}
