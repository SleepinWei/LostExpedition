#include "ExplorerCharacter.h"
#include "ExplorerMotionMatching.h"
#include "ExplorerVisualComponent.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "GameFramework/PlayerController.h"
#if WITH_EDITOR
#include "PoseSearch/PoseSearchDerivedData.h"
#endif
#include "ExpeditionActors.h"
#include "ExpeditionWorld.h"
#include "ExpeditionTower.h"
#include "IslandTerrain.h"
#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"

void AExplorerCharacter::RunSmokeTest() {
    TArray<FString> Results;int32 Failed=0;
    auto Check=[&](bool Pass,const FString& Name){Results.Add(FString::Printf(TEXT("%s: %s"),Pass?TEXT("PASS"):TEXT("FAIL"),*Name));if(!Pass)Failed++;UE_LOG(LogTemp,Display,TEXT("ADVENTURE_TEST %s %s"),Pass?TEXT("PASS"):TEXT("FAIL"),*Name);};
    Invulnerability=1000;
    auto Advance=[&](float Duration){for(float Time=0;Time<Duration;Time+=.016f){Tick(.016f);GetMesh()->TickAnimation(.016f,false);GetMesh()->RefreshBoneTransforms();UpdateClimbPose();}};
    int32 WorldCount=0;
    for(TActorIterator<AExpeditionWorld> It(GetWorld());It;++It){
        WorldCount++;Check(It->Terrain&&It->Terrain->GetNumSections()>0,TEXT("Continuous island terrain generated with collision"));
        int32 Palms=0,Trees=0,Grass=0,Grips=0;
        for(auto Part:It->Pieces)if(Part){
            if(Part->ComponentHasTag(TEXT("WallGrip")))Grips++;
            if(auto* Instance=Cast<UHierarchicalInstancedStaticMeshComponent>(Part)) {
                if(Part->GetName().Contains(TEXT("IslandPalms")))Palms+=Instance->GetInstanceCount();
                if(Part->GetName().Contains(TEXT("JungleCanopy")))Trees+=Instance->GetInstanceCount();
                if(Part->GetName().Contains(TEXT("JungleGrass")))Grass+=Instance->GetInstanceCount();
            }
        }
        Check(Palms>=40&&Trees>=30&&Grass>=500,TEXT("Tropical palms, canopy trees and dense grass are instanced"));
        Check(Grips==ExpeditionTower::Steps,TEXT("All tower stone handles are placed"));
    }
    Check(WorldCount==1,TEXT("Exactly one island environment actor"));
    auto* Matching=Cast<UExplorerMotionMatching>(GetMesh()->GetAnimInstance());
    Check(Matching!=nullptr,TEXT("Character evaluates the compiled Epic Motion Matching AnimBlueprint"));
    TArray<FString> MotionSelections;
    if(Matching) {
        bool Ready=Matching->Databases.Num()==3;
        for(auto Database:Matching->Databases) {
#if WITH_EDITOR
            if(Database)UE::PoseSearch::FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database,UE::PoseSearch::ERequestAsyncBuildFlag::ContinueRequest|UE::PoseSearch::ERequestAsyncBuildFlag::WaitForCompletion);
#endif
            Ready=Ready&&Database&&Database->GetNumAnimationAssets()==17&&Database->GetSearchIndex().GetNumPoses()>400;
        }
        Check(Ready,TEXT("All three Pose Search databases contain 17 clips and built searchable pose indexes"));
        SetActorLocation(FVector(3000,0,2299));SetActorRotation(FRotator::ZeroRotator);
        GetCharacterMovement()->SetMovementMode(MOVE_Walking);GetCharacterMovement()->StopMovementImmediately();
        if(auto* PC=Cast<APlayerController>(GetController()))PC->SetControlRotation(FRotator::ZeroRotator);
        Matching->ResetTrajectory();
        auto Simulate=[&](FVector2D Input,float Duration) {
            for(float Time=0;Time<Duration;Time+=1.f/60) {
                Forward(Input.X);Right(Input.Y);Tick(1.f/60);
                GetCharacterMovement()->TickComponent(1.f/60,LEVELTICK_All,nullptr);
                GetMesh()->TickAnimation(1.f/60,false);GetMesh()->RefreshBoneTransforms();UpdateClimbPose();
            }
            MotionSelections.Add(FString::Printf(TEXT("%s / %s @ %.3f s; cost %.3f; speed %.1f; requested %s"),*Matching->MatchedDatabase,*Matching->MatchedAnimation,Matching->MatchedTime,Matching->MatchCost,GetVelocity().Size2D(),*GetNameSafe(Matching->ActiveDatabase)));
            UE_LOG(LogTemp,Display,TEXT("MOTION_MATCH_SELECTION %s"),*MotionSelections.Last());
        };
        bAim=false;bSprint=true;FireAnimationTime=EquipAnimationTime=-1;ArmAnimationAlpha=0;
        Simulate(FVector2D(1,0),.8f);
        Check(Matching->bHasMatchedPose&&Matching->MatchedDatabase==TEXT("PSD_Unarmed")&&Matching->MatchedAnimation.Contains(TEXT("Jog_Fwd")),TEXT("Actual sprint input selects an unarmed forward jog pose"));
        Check(Matching->MotionTrajectory.Samples.Num()==8&&Matching->MotionTrajectory.Samples[0].TimeInSeconds<0&&Matching->MotionTrajectory.Samples.Last().TimeInSeconds>.8f,TEXT("Matching query receives recorded history and future movement samples"));
        bSprint=false;Simulate(FVector2D::ZeroVector,.8f);
        Check(Matching->bHasMatchedPose&&Matching->MatchedAnimation.Contains(TEXT("Idle")),TEXT("Braking returns the real Motion Matching selection to idle"));
        bAim=true;Weapon=0;Simulate(FVector2D(0,1),.8f);
        Check(Matching->MatchedDatabase==TEXT("PSD_Pistol")&&Matching->MatchedAnimation.Contains(TEXT("Right")),TEXT("Pistol aim selects a right strafe through Pose Search"));
        Weapon=1;Simulate(FVector2D(-1,0),.8f);
        Check(Matching->MatchedDatabase==TEXT("PSD_Rifle")&&Matching->MatchedAnimation.Contains(TEXT("Bwd")),TEXT("Rifle swap and reversal select the rifle backward database pose"));
        const FVector BeforeEvaluation=GetActorLocation();Advance(.1f);
        Check(GetActorLocation().Equals(BeforeEvaluation,.001f),TEXT("Animation root extraction never displaces the collision capsule"));
        GetCharacterMovement()->SetMovementMode(MOVE_Falling);GetCharacterMovement()->Velocity=FVector(0,0,400);Advance(.1f);
        Check(Matching->bUseAirPose&&Matching->AirSequence==Matching->JumpSequence,TEXT("Jump ascent blends from matched locomotion to the authored jump pose"));
        GetCharacterMovement()->Velocity=FVector(0,0,-400);Advance(.1f);
        Check(Matching->bUseAirPose&&Matching->AirSequence==Matching->FallSequence,TEXT("Jump descent evaluates the fall loop"));
        GetCharacterMovement()->SetMovementMode(MOVE_Walking);GetCharacterMovement()->Velocity=FVector::ZeroVector;Advance(.1f);
        Check(Matching->bUseAirPose&&Matching->AirSequence==Matching->LandSequence,TEXT("Ground contact blends through the authored landing pose"));
        Advance(.4f);Check(!Matching->bUseAirPose&&Matching->bHasMatchedPose&&Matching->SelectionChanges>=4&&Matching->EvaluatedFrames>100,TEXT("Landing resumes evaluated Motion Matching with recorded selection changes"));
        bAim=false;Weapon=0;ArmAnimationAlpha=0;Forward(0);Right(0);ConsumeMovementInputVector();GetCharacterMovement()->StopMovementImmediately();
    }
    Check(IslandTerrain::Height(-9200,-2200)<400&&IslandTerrain::Height(1600,900)>2100,TEXT("Low sandy coast and elevated central plateau"));
    for(TActorIterator<AExpeditionGuard> It(GetWorld());It;++It)It->bTrainingTarget=true;
    auto SupportedPosition=[&](float X,float Y,FVector& Out){
        FHitResult Hit;FCollisionQueryParams Query;Query.AddIgnoredActor(this);float H=IslandTerrain::Height(X,Y);
        if(!GetWorld()->LineTraceSingleByChannel(Hit,FVector(X,Y,H+300),FVector(X,Y,H-300),ECC_Visibility,Query)||Hit.ImpactNormal.Z<.75f)return false;
        Out=Hit.ImpactPoint+FVector(0,0,105);return true;
    };
    FVector Start;SupportedPosition(-7200,-4500,Start);SetActorLocation(Start);GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    auto WalkTo=[&](float X,float Y){
        FVector Initial=GetActorLocation();int32 Count=FMath::CeilToInt(FVector2D(X-Initial.X,Y-Initial.Y).Size()/20);
        for(int I=1;I<=Count;I++) {
            FVector Target;float T=float(I)/Count;
            if(!SupportedPosition(FMath::Lerp(Initial.X,double(X),T),FMath::Lerp(Initial.Y,double(Y),T),Target))return false;
            FHitResult Hit;SetActorLocation(Target,true,&Hit);if(Hit.bBlockingHit)return false;
        }return true;
    };
    bool Trail=WalkTo(-6100,IslandTerrain::TrailY(-6100));
    for(float X=-6000;X<=1000&&Trail;X+=100)Trail=WalkTo(X,IslandTerrain::TrailY(X));
    Check(Trail,TEXT("Walk continuously from beach through jungle trail onto the highland"));
    bool TowerApproach=Trail&&WalkTo(820,-1100)&&WalkTo(820,640)&&WalkTo(967,640);
    Check(TowerApproach,TEXT("Walk from highland to the first tower handhold"));
    SetActorRotation(FRotator(0,180,0));LedgeCooldown=0;Check(!BeginWallGrip(),TEXT("Cannot grab a handhold while facing away"));
    SetActorRotation(FRotator::ZeroRotator);const FVector GroundStart=GetActorLocation();
    Check(BeginHang()&&Traversal==ETraversalState::Probing,TEXT("Approaching the first handle starts the exploratory reach stage"));
    Advance(.6f);
    Check(GetActorLocation().Equals(GroundStart,.01f)&&Traversal==ETraversalState::Probing,TEXT("Ground probing never teleports the capsule or automatically jumps"));
    Check(CharacterVisual->GetCharacterMesh()&&CharacterVisual->IsVisible()&&!ClimbPose->IsVisible()&&!GetMesh()->IsVisible()&&CharacterVisual->IsRetargetReady(),TEXT("The clothed Diesel character is visible and uses the runtime IK retargeter"));
    Check(CharacterVisual->GetBoneLocationByName(TEXT("mixamorig_Hips"),EBoneSpaces::WorldSpace).Z>GetActorLocation().Z-20,TEXT("Retargeting preserves pelvis height instead of replacing it with the ground root"));
    bool ClipsReady=WallClimbAnimations.Num()==8;
    for(auto Clip:WallClimbAnimations)ClipsReady=ClipsReady&&Clip&&Clip->GetPlayLength()>.2f;
    Check(ClipsReady,TEXT("Eight full body sequences cover ground reach/leap, mirrored wall probe/leap, catch and hang"));
    Check(!BeginMantle(),TEXT("The exploratory reach cannot top out"));
    JumpOrClimb();Check(Traversal==ETraversalState::GripJump,TEXT("Space explicitly launches the jump-to-grab stage"));
    Advance(ReachDuration+.03f);Check(Traversal==ETraversalState::Catching,TEXT("Landing on a handhold enters the secure catch animation"));
    Advance(CatchDuration+.05f);Check(Traversal==ETraversalState::Clinging&&CurrentGrip==0,TEXT("The catch settles into a supported hang on the first handle"));
    Check(!BeginMantle(),TEXT("Intermediate wall handles cannot be used as walkable platforms"));
    auto ResetGrip=[&](int32 Index){
        Drop();Traversal=ETraversalState::Walking;GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        SetActorLocation(ExpeditionTower::HangPosition(Index)+FVector(-40,0,0));SetActorRotation(FRotator::ZeroRotator);LedgeCooldown=0;BeginWallGrip();JumpOrClimb();Advance(1.4f);GripCooldown=0;
    };
    // Measure the visible rig, including retargeting and final contact IK, at 60 Hz.
    // Keep the same input trace for the before/after comparison.
    FString Continuity=TEXT("scenario,frame,stage,bone,x,y,z,step_cm,rotation_deg\n");
    float MaxJointStep=0,MaxJointRotation=0,MaxBoundaryStep=0,MaxRetargetStep=0,MaxPlantedFootStep=0;
    const FName ReviewBones[]={TEXT("mixamorig_Hips"),TEXT("mixamorig_Spine2"),TEXT("mixamorig_Head"),TEXT("mixamorig_LeftArm"),TEXT("mixamorig_LeftForeArm"),TEXT("mixamorig_LeftHand"),TEXT("mixamorig_RightArm"),TEXT("mixamorig_RightForeArm"),TEXT("mixamorig_RightHand"),TEXT("mixamorig_LeftLeg"),TEXT("mixamorig_LeftFoot"),TEXT("mixamorig_RightLeg"),TEXT("mixamorig_RightFoot")};
    for(int32 Scenario=0;Scenario<3;Scenario++) {
        ResetGrip(Scenario==1?8:1);
        TArray<FTransform> Previous;for(FName Bone:ReviewBones)Previous.Add(CharacterVisual->GetBoneTransformByName(Bone,EBoneSpaces::WorldSpace));
        ETraversalState PreviousStage=Traversal;
        for(int32 Frame=0;Frame<180;Frame++) {
            if(Frame==0)MoveWallGrip(Scenario==1?1:0,Scenario==1?0:1);
            if(Scenario==2&&Frame==12)MoveWallGrip(0,-1);
            if(Scenario==2&&Frame==24)MoveWallGrip(0,1);
            if(Frame==36)JumpOrClimb();
            if(Frame==48){MoveWallGrip(Scenario==1?1:0,Scenario==1?0:1);JumpOrClimb();}
            Tick(1.f/60);GetMesh()->TickAnimation(1.f/60,false);GetMesh()->RefreshBoneTransforms();UpdateClimbPose();
            for(int32 Bone=0;Bone<UE_ARRAY_COUNT(ReviewBones);Bone++) {
                const FTransform Current=CharacterVisual->GetBoneTransformByName(ReviewBones[Bone],EBoneSpaces::WorldSpace);
                const float Step=FVector::Dist(Current.GetLocation(),Previous[Bone].GetLocation());
                const float Angle=FMath::RadiansToDegrees(Current.GetRotation().AngularDistance(Previous[Bone].GetRotation()));
                MaxJointStep=FMath::Max(MaxJointStep,Step);MaxJointRotation=FMath::Max(MaxJointRotation,Angle);
                if(Traversal!=PreviousStage)MaxBoundaryStep=FMath::Max(MaxBoundaryStep,Step);
                if(Scenario==2&&(Frame==12||Frame==24))MaxRetargetStep=FMath::Max(MaxRetargetStep,Step);
                if(Frame<36&&(Bone==10||Bone==12))MaxPlantedFootStep=FMath::Max(MaxPlantedFootStep,Step);
                const FVector V=Current.GetLocation();
                Continuity+=FString::Printf(TEXT("%d,%d,%d,%s,%.4f,%.4f,%.4f,%.4f,%.4f\n"),Scenario,Frame,int32(Traversal),*ReviewBones[Bone].ToString(),V.X,V.Y,V.Z,Step,Angle);
                Previous[Bone]=Current;
            }
            PreviousStage=Traversal;
        }
    }
    FFileHelper::SaveStringToFile(Continuity,*(FPaths::ProjectDir()/TEXT("Docs/climb-continuity.csv")));
    UE_LOG(LogTemp,Display,TEXT("CLIMB_CONTINUITY maxJointStep=%.3f maxJointRotation=%.3f maxBoundaryStep=%.3f"),MaxJointStep,MaxJointRotation,MaxBoundaryStep);
    Check(MaxRetargetStep<1,TEXT("Changing probe direction preserves the visible pose within one centimetre at 60 Hz"));
    Check(MaxJointRotation<20,TEXT("Vertical, lateral and interrupted-probe traces have no visible joint rotation over 20 degrees per 60 Hz frame"));
    Check(MaxBoundaryStep<3,TEXT("Probe, leap, catch and buffered transitions stay below three centimetres per boundary frame"));
    Check(MaxPlantedFootStep<.2f,TEXT("Planted boots drift less than two millimetres per frame while probing"));
    UE_LOG(LogTemp,Display,TEXT("CLIMB_CONTINUITY probeRetargetStep=%.4f plantedFootStep=%.4f"),MaxRetargetStep,MaxPlantedFootStep);
    // A smooth capsule path alone is not a jump. Require a visible preload,
    // animated limb travel and an actual interval with all four contacts released.
    ResetGrip(1);MoveWallGrip(0,1);Advance(.4f);
    const float StartPelvis=CharacterVisual->GetBoneLocationByName(TEXT("mixamorig_Hips"),EBoneSpaces::WorldSpace).Z;
    JumpOrClimb();
    float LowestPelvis=StartPelvis,MaxFreeHandTravel=0,MaxFreeFootTravel=0,MaxFreeWristOverride=0;
    int FreeFrames=0,FreeWristSamples=0;bool AnatomicalSides=true;FVector FirstFreeHand,FirstFreeFoot;
    for(int Frame=0;Frame<60;Frame++) {
        Tick(1.f/60);GetMesh()->TickAnimation(1.f/60,false);GetMesh()->RefreshBoneTransforms();UpdateClimbPose();
        if(Traversal!=ETraversalState::GripJump)continue;
        const float T=ReachTime/ReachDuration;
        if(T<.20f)LowestPelvis=FMath::Min(LowestPelvis,float(CharacterVisual->GetBoneLocationByName(TEXT("mixamorig_Hips"),EBoneSpaces::WorldSpace).Z));
        if(HandContact[0]+HandContact[1]+FootContact[0]+FootContact[1]<.001f) {
            const FVector Hand=CharacterVisual->GetBoneLocationByName(TEXT("mixamorig_RightHand"),EBoneSpaces::WorldSpace)-GetActorLocation();
            const FVector Foot=CharacterVisual->GetBoneLocationByName(TEXT("mixamorig_LeftFoot"),EBoneSpaces::WorldSpace)-GetActorLocation();
            for(int I=0;I<2;I++) {
                // Position/rotation curves share exact zero-contact intervals,
                // but have different tiny nonzero weights near the endpoints.
                if(HandContact[I]==0) {
                    const FQuat WristRotation=CharacterVisual->GetBoneTransformByName(I==0?TEXT("mixamorig_LeftHand"):TEXT("mixamorig_RightHand"),EBoneSpaces::WorldSpace).GetRotation();
                    MaxFreeWristOverride=FMath::Max(MaxFreeWristOverride,float(FMath::RadiansToDegrees(WristRotation.AngularDistance(UnconstrainedVisualWrist[I]))));
                    FreeWristSamples++;
                }
                const FVector Wrist=CharacterVisual->GetBoneLocationByName(I==0?TEXT("mixamorig_LeftHand"):TEXT("mixamorig_RightHand"),EBoneSpaces::WorldSpace);
                AnatomicalSides=AnatomicalSides&&FVector::DotProduct(Wrist-GetActorLocation(),GetActorRightVector())*(I==0?-1:1)>0;
            }
            if(FreeFrames++==0){FirstFreeHand=Hand;FirstFreeFoot=Foot;}
            MaxFreeHandTravel=FMath::Max(MaxFreeHandTravel,float(FVector::Dist(Hand,FirstFreeHand)));
            MaxFreeFootTravel=FMath::Max(MaxFreeFootTravel,float(FVector::Dist(Foot,FirstFreeFoot)));
        }
    }
    UE_LOG(LogTemp,Display,TEXT("CLIMB_PERFORMANCE preload=%.3f freeFrames=%d armSwing=%.3f legSwing=%.3f"),StartPelvis-LowestPelvis,FreeFrames,MaxFreeHandTravel,MaxFreeFootTravel);
    Check(StartPelvis-LowestPelvis>2,TEXT("Wall jump visibly lowers the pelvis before push-off"));
    Check(FreeFrames>=12,TEXT("Wall jump releases all four contact constraints for at least 0.2 seconds"));
    Check(MaxFreeHandTravel>12&&MaxFreeFootTravel>12,TEXT("Unconstrained flight contains arm swing and leg tuck beyond capsule translation"));
    Check(AnatomicalSides,TEXT("Free-flight hands remain on their anatomical sides after retargeting"));
    Check(FreeWristSamples>=24&&MaxFreeWristOverride<.01f,TEXT("Zero-contact flight preserves both animated wrist rotations within 0.01 degrees"));
    UE_LOG(LogTemp,Display,TEXT("CLIMB_FREE_WRIST samples=%d maxOverrideDegrees=%.6f"),FreeWristSamples,MaxFreeWristOverride);
    ResetGrip(0);
    auto* Obstacle=GetWorld()->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Obstacle);Obstacle->SetRootComponent(Box);Box->SetBoxExtent(FVector(45,55,45));Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->RegisterComponent();Obstacle->SetActorLocation(ExpeditionTower::HangPosition(1));
    Check(!MoveWallGrip(0,1),TEXT("An obstructed handhold cannot be selected for a reach"));Obstacle->Destroy();
    ResetGrip(0);const FVector SupportedStart=GetActorLocation();Forward(1);Advance(.7f);
    Check(Traversal==ETraversalState::Probing&&CurrentGrip==0&&TargetGrip==1&&GetActorLocation().Equals(SupportedStart,.01f),TEXT("Held direction probes the next grip while the body remains supported"));
    Forward(0);Advance(.2f);Check(Traversal==ETraversalState::Probing,TEXT("The selected handhold waits for Space after direction release"));
    Journal();Advance(.03f);Check(Traversal==ETraversalState::Clinging&&!bProbeJumpRequested,TEXT("Opening the journal cancels a probe without launching a jump"));Journal();
    FVector RatePositions[3],RatePelvis[3];float LargestContactError=0,LargestFootError=0;
    const int32 Rates[3]={30,60,120};
    for(int R=0;R<3;R++) {
        ResetGrip(0);MoveWallGrip(0,1);Advance(.3f);JumpOrClimb();
        for(int F=0;F<Rates[R]*.4f;F++){Tick(1.f/Rates[R]);UpdateClimbPose();}
        RatePositions[R]=GetActorLocation();RatePelvis[R]=CharacterVisual->GetBoneLocationByName(TEXT("mixamorig_Hips"),EBoneSpaces::WorldSpace);
        Check(Traversal==ETraversalState::GripJump,FString::Printf(TEXT("Space leap evaluates between grips at %d Hz"),Rates[R]));
        Advance(.9f);Check(Traversal==ETraversalState::Clinging&&CurrentGrip==1,TEXT("A jump command catches exactly one selected grip"));
        for(int Index=0;Index<2;Index++) {
            const FName Hand=Index==0?TEXT("mixamorig_LeftHand"):TEXT("mixamorig_RightHand"),Foot=Index==0?TEXT("mixamorig_LeftFoot"):TEXT("mixamorig_RightFoot");
            LargestContactError=FMath::Max(LargestContactError,float(FVector::Dist(CharacterVisual->GetBoneLocationByName(Hand,EBoneSpaces::WorldSpace),AnimatedHands[Index])));
            LargestFootError=FMath::Max(LargestFootError,float(FVector::Dist(CharacterVisual->GetBoneLocationByName(Foot,EBoneSpaces::WorldSpace),AnimatedFeet[Index])));
        }
    }
    UE_LOG(LogTemp,Display,TEXT("ACTION_CONTACT maxSupportError=%.3f maxFootError=%.3f rateDelta=%.3f"),LargestContactError,LargestFootError,FVector::Dist(RatePositions[0],RatePositions[2]));
    Check(FVector::Dist(RatePositions[0],RatePositions[2])<1,TEXT("30 and 120 Hz jump paths agree within one centimetre"));
    Check(FVector::Dist(RatePelvis[0],RatePelvis[2])<4,TEXT("The visible pelvis at 30 and 120 Hz agrees within four centimetres during flight"));
    Check(LargestFootError<8,TEXT("The clothed character's planted feet match the wall within eight centimetres"));
    Check(LargestContactError<8,TEXT("The clothed character's hands match the stone holds within eight centimetres"));
    ResetGrip(0);MoveWallGrip(0,1);JumpOrClimb();Check(Traversal==ETraversalState::Probing,TEXT("An early Space press preserves a minimum visible anticipation"));
    Advance(.22f);Check(Traversal==ETraversalState::GripJump,TEXT("The buffered Space press launches after anticipation"));
    MoveWallGrip(0,1);JumpOrClimb();Advance(2.5f);
    Check(CurrentGrip==2&&Traversal==ETraversalState::Clinging,TEXT("One Space press during a leap buffers exactly one subsequent grip jump"));
    ResetGrip(0);MoveWallGrip(0,1);Advance(.3f);JumpOrClimb();Advance(.2f);
    Obstacle=GetWorld()->SpawnActor<AActor>();Box=NewObject<UBoxComponent>(Obstacle);Obstacle->SetRootComponent(Box);Box->SetBoxExtent(FVector(45,55,45));Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->RegisterComponent();Obstacle->SetActorLocation(ExpeditionTower::HangPosition(1));
    Advance(.8f);Check(Traversal==ETraversalState::Walking&&GetCharacterMovement()->IsFalling(),TEXT("A new obstacle during flight safely interrupts the grab and restores gravity"));Obstacle->Destroy();
    ResetGrip(0);bool Climbed=Traversal==ETraversalState::Clinging;
    for(int I=1;I<ExpeditionTower::Steps&&Climbed;I++) {
        FVector Delta=ExpeditionTower::Grip(I)-ExpeditionTower::Grip(I-1);GripCooldown=0;
        if(FMath::Abs(Delta.Z)<1)Check(!MoveWallGrip(0,1),TEXT("Horizontal gap requires a deliberate sideways reach"));
        bool Selected=MoveWallGrip(FMath::Abs(Delta.Z)<1?FMath::Sign(Delta.Y):0,Delta.Z>0?1:0);
        if(Selected){Advance(.3f);JumpOrClimb();Advance(ReachDuration+CatchDuration+.05f);}
        Climbed=Selected&&CurrentGrip==I&&Traversal==ETraversalState::Clinging&&GetActorLocation().Equals(ExpeditionTower::HangPosition(I),2);
        Check(Climbed,FString::Printf(TEXT("Probe / Space leap / secure catch %02d / %02d"),I,ExpeditionTower::Steps-1));
    }
    Check(Climbed&&BeginMantle(),TEXT("Start the final mantle from the top stone handle"));
    if(Traversal==ETraversalState::Mantling){Advance(.38f);Check(CharacterVisual->IsVisible()&&Traversal==ETraversalState::Mantling,TEXT("The clothed character stays animated through the rooftop pull-up"));Advance(MantleDuration-.38f+.03f);}
    Check(Traversal==ETraversalState::Walking&&FMath::Abs(GetActorLocation().Z-(ExpeditionTower::SummitZ()+99))<3,TEXT("Stand on the ruined tower roof after climbing the entire wall"));
    FHitResult SummitHit;SetActorLocation(ExpeditionTower::Base+FVector(0,0,ExpeditionTower::Height+99),true,&SummitHit);RefreshLoot();
    Check(!SummitHit.bBlockingHit&&Nearby.IsValid()&&Nearby->ItemId==TEXT("TowerSummitCheckpoint"),TEXT("Summit checkpoint can be reached and interacted with"));
    SetActorLocation(ExpeditionTower::Grip(ExpeditionTower::Steps-1)+FVector(90,0,99));SetActorRotation(FRotator(0,180,0));LedgeCooldown=0;
    Check(BeginHang()&&Traversal==ETraversalState::Reaching,TEXT("Enter the top handhold from the roof edge to climb back down"));
    if(Traversal==ETraversalState::Reaching)Advance(RoofEntryDuration+.03f);
    Check(Traversal==ETraversalState::Clinging&&CurrentGrip==ExpeditionTower::Steps-1,TEXT("Reverse mantle clears the roof collision before lowering the character"));
    GripCooldown=0;bool Down=MoveWallGrip(0,-1);if(Down){Advance(.3f);JumpOrClimb();Advance(GripTransferDuration+CatchDuration+.05f);}
    Check(Down&&CurrentGrip==ExpeditionTower::Steps-2,TEXT("Select a descending grip with S and catch it with Space"));
    Drop();Check(Traversal==ETraversalState::Walking&&GetCharacterMovement()->IsFalling()&&CharacterVisual->IsVisible(),TEXT("Drop restores gravity and preserves the clothed presentation mesh"));
    SetActorLocation(ExpeditionTower::HangPosition(0)+FVector(-40,0,0));SetActorRotation(FRotator::ZeroRotator);GetCharacterMovement()->SetMovementMode(MOVE_Walking);LedgeCooldown=0;BeginWallGrip();const FVector CancelStart=GetActorLocation();Drop();
    Check(Traversal==ETraversalState::Walking&&GetCharacterMovement()->IsMovingOnGround()&&GetActorLocation().Equals(CancelStart,.01f),TEXT("Cancelling a ground probe preserves its standing location and walking mode"));
    SetActorLocation(FVector(3000,0,2299));GetCharacterMovement()->SetMovementMode(MOVE_Walking);LedgeCooldown=1;
    auto* Guard=GetWorld()->SpawnActor<AExpeditionGuard>(FVector(3400,0,2200),FRotator(0,-90,0));Guard->bTrainingTarget=true;
    Camera->SetWorldLocation(FVector(3100,0,2340));Camera->SetWorldRotation((FVector(3400,0,2295)-Camera->GetComponentLocation()).Rotation());
    Check(WeaponIdleAnimations.Num()==2&&WeaponFireAnimations.Num()==2&&WeaponIdleAnimations[0]&&WeaponIdleAnimations[1]&&WeaponFireAnimations[0]&&WeaponFireAnimations[1],TEXT("Official pistol and rifle pose and firing sequences load"));
    AimStart();Weapon=0;Advance(.4f);Magazine[0]=12;ShotCooldown=0;float OldHealth=Guard->Health;FireShot();
    Check(Magazine[0]==11,TEXT("Fire consumes one round"));Check(Guard->Health<OldHealth,TEXT("Hitscan damages skeletal guard"));
    const FQuat IdleArm=ClimbPose->GetBoneTransformByName(TEXT("upperarm_r"),EBoneSpaces::ComponentSpace).GetRotation();
    Advance(.075f);
    const FQuat RecoilArm=ClimbPose->GetBoneTransformByName(TEXT("upperarm_r"),EBoneSpaces::ComponentSpace).GetRotation();
    Check(FireAnimationTime>0&&CharacterVisual->IsVisible()&&IdleArm.AngularDistance(RecoilArm)>.01f,TEXT("Successful pistol shot animates the upper body over the aiming pose"));
    Check(WeaponMesh->GetAttachParent()==ClimbPose,TEXT("Weapon follows the animated hand socket"));
    const float ShotPhase=FireAnimationTime;int32 Before=Magazine[0];FireShot();Check(Magazine[0]==Before&&FireAnimationTime==ShotPhase,TEXT("Fire rate blocks both extra rounds and animation retriggers"));
    AimStop();Advance(.9f);Check(FireAnimationTime<0&&ArmAnimationAlpha<.01f&&CharacterVisual->IsVisible(),TEXT("Firing animation recovers into locomotion without switching meshes"));
    Magazine[0]=0;Reserve[0]=5;Reload();
    const FQuat ReloadStartArm=ClimbPose->GetBoneTransformByName(TEXT("lowerarm_l"),EBoneSpaces::ComponentSpace).GetRotation();
    Advance(.55f);Check(bReloading&&ReloadStartArm.AngularDistance(ClimbPose->GetBoneTransformByName(TEXT("lowerarm_l"),EBoneSpaces::ComponentSpace).GetRotation())>.1f,TEXT("Reload plays the official left-hand action while the weapon socket stays attached"));
    Advance(.75f);Check(Magazine[0]==5&&Reserve[0]==0&&!bReloading,TEXT("Reload clamps to available reserve"));
    Magazine[0]=0;Reserve[0]=0;Reload();Check(!bReloading,TEXT("Empty reserve rejects reload"));
    Rifle();Check(Weapon==1&&WeaponMesh->GetStaticMesh()&&WeaponMesh->GetStaticMesh()->GetPathName().Contains(TEXT("SM_Rifle")),TEXT("Weapon swap uses official rifle mesh"));Check(EquipAnimationTime==0&&WeaponEquipAnimations[1],TEXT("Rifle selection starts the official equip action"));AimStart();Advance(1.9f);
    UE_LOG(LogTemp,Display,TEXT("RIFLE_AIM barrel=%s camera=%s"),*WeaponMesh->GetRightVector().ToString(),*Camera->GetForwardVector().ToString());
    // Both official weapon meshes have their barrel along local +Y.
    Check(FVector::DotProduct(WeaponMesh->GetRightVector(),Camera->GetForwardVector())>.97f,TEXT("Rifle aiming animation keeps the barrel aligned with the camera"));
    Check(FVector::Dist(ClimbPose->GetBoneLocationByName(TEXT("foot_r"),EBoneSpaces::WorldSpace),GetMesh()->GetSocketLocation(TEXT("foot_r")))<.5f,TEXT("Upper-body firearm animation preserves the locomotion foot pose"));
    bool AllDirections=ArmedLocomotionAnimations.Num()==32;
    for(auto Clip:ArmedLocomotionAnimations)AllDirections=AllDirections&&Clip!=nullptr;
    Check(AllDirections,TEXT("Both weapons load eight official walk and jog directions"));
    auto MoveAim=[&](FVector Direction){
        const FVector InitialFoot=ClimbPose->GetBoneLocationByName(TEXT("foot_r"),EBoneSpaces::ComponentSpace);float Excursion=0;
        for(int32 Frame=0;Frame<48;Frame++) {
            AddMovementInput(Direction,1);Tick(1.f/60);GetCharacterMovement()->TickComponent(1.f/60,LEVELTICK_All,nullptr);
            GetMesh()->TickAnimation(1.f/60,false);GetMesh()->RefreshBoneTransforms();UpdateClimbPose();
            Excursion=FMath::Max(Excursion,float(FVector::Dist(InitialFoot,ClimbPose->GetBoneLocationByName(TEXT("foot_r"),EBoneSpaces::ComponentSpace))));
        }
                if(Matching)UE_LOG(LogTemp,Display,TEXT("MOVING_AIM_QUERY location=%s velocity=%s acceleration=%s falling=%d air=%d facing=%s futureDelta=%s cost=%.3f"),*GetActorLocation().ToString(),*GetVelocity().ToString(),*GetCharacterMovement()->GetCurrentAcceleration().ToString(),GetCharacterMovement()->IsFalling(),Matching->bUseAirPose,*GetActorRotation().ToString(),* (Matching->MotionTrajectory.Samples.Last().Position-Matching->MotionTrajectory.Samples[2].Position).ToString(),Matching->MatchCost);
        return Excursion;
    };
    const float StrafeFootMotion=MoveAim(GetActorRightVector());
    const FString StrafeAnimation=Matching?Matching->MatchedAnimation:TEXT("FallbackStrafe");
    const float BackFootMotion=MoveAim(-GetActorForwardVector());
    const FString BackAnimation=Matching?Matching->MatchedAnimation:TEXT("FallbackBackward");
    UE_LOG(LogTemp,Display,TEXT("MOVING_AIM strafe=%s footMotion=%.2f back=%s footMotion=%.2f speed=%.1f"),*StrafeAnimation,StrafeFootMotion,*BackAnimation,BackFootMotion,GetVelocity().Size2D());
    Check(StrafeFootMotion>5&&BackFootMotion>5&&StrafeAnimation!=BackAnimation,TEXT("Moving aim evaluates distinct strafe and backward foot motion over a cycle"));
    GetCharacterMovement()->Velocity=FVector::ZeroVector;
    Magazine[1]=30;ShotCooldown=0;StartFire();Advance(.30f);StopFire();Check(Magazine[1]<=27&&FireAnimationTime>=0&&CharacterVisual->IsVisible(),TEXT("Automatic rifle fire repeatedly triggers the rifle recoil animation"));const FQuat BeforeRetrigger=ClimbPose->GetBoneTransformByName(TEXT("upperarm_r"),EBoneSpaces::ComponentSpace).GetRotation();ShotCooldown=0;FireShot();
    Check(BeforeRetrigger.AngularDistance(ClimbPose->GetBoneTransformByName(TEXT("upperarm_r"),EBoneSpaces::ComponentSpace).GetRotation())<.005f,TEXT("Rifle recoil retrigger preserves the outgoing arm pose before crossfading"));
    Pistol();Check(Weapon==0&&WeaponMesh->GetStaticMesh()&&WeaponMesh->GetStaticMesh()->GetPathName().Contains(TEXT("SM_Pistol")),TEXT("Pistol selection uses official pistol mesh"));
    Health=25;Medkits=1;Heal();Check(Health==85&&Medkits==0,TEXT("Medkit heals and is consumed"));Heal();Check(Health==85,TEXT("No free healing with empty inventory"));
    auto SpawnLoot=[&](ELootKind Kind,FName Id){auto* L=GetWorld()->SpawnActor<AExpeditionLoot>(GetActorLocation()+FVector(100,0,0),FRotator::ZeroRotator);L->Kind=Kind;L->ItemId=Id;return L;};
    auto* Gate=SpawnLoot(ELootKind::Gate,TEXT("test_gate"));bHasKey=false;bGateOpen=false;Check(!ReceiveLoot(Gate)&&!bGateOpen,TEXT("Gate rejects missing key"));
    auto* Key=SpawnLoot(ELootKind::Key,TEXT("test_key"));Key->Interact(this);Check(bHasKey&&Key->bUsed&&Collected.Contains(TEXT("test_key")),TEXT("Key pickup tracked"));
    Gate->Interact(this);Check(bGateOpen&&Gate->bUsed,TEXT("Key opens gate"));
    auto* Relic=SpawnLoot(ELootKind::Relic,TEXT("test_relic"));Relics=0;Relic->Interact(this);Relic->Interact(this);Check(Relics==1,TEXT("Relic cannot be collected twice"));
    auto* Ammo=SpawnLoot(ELootKind::Ammo,TEXT("test_ammo"));int32 R=Reserve[1];Ammo->Interact(this);Check(Reserve[1]==R+60,TEXT("Ammo supply replenishes inventory"));
    auto* Med=SpawnLoot(ELootKind::Medkit,TEXT("test_med"));Medkits=5;Med->Interact(this);Check(!Med->bUsed&&Medkits==5,TEXT("Full inventory leaves supply available"));
    auto* Exit=SpawnLoot(ELootKind::Exit,TEXT("test_exit"));Check(!ReceiveLoot(Exit)&&!bCompleted,TEXT("Exit requires three relics"));Relics=3;Check(ReceiveLoot(Exit)&&bCompleted,TEXT("Complete expedition"));bCompleted=false;
    auto* Save=Cast<UExpeditionSave>(UGameplayStatics::CreateSaveGameObject(UExpeditionSave::StaticClass()));Save->Checkpoint=FVector(820,640,2305);Save->Relics=3;Save->bHasKey=true;Save->Collected=Collected;
    bool Saved=UGameplayStatics::SaveGameToSlot(Save,TEXT("LostExpedition_SmokeTest"),0);auto* Loaded=Cast<UExpeditionSave>(UGameplayStatics::LoadGameFromSlot(TEXT("LostExpedition_SmokeTest"),0));
    Check(Saved&&Loaded&&Loaded->Checkpoint.Equals(Save->Checkpoint)&&Loaded->Relics==3&&Loaded->Collected.Contains(TEXT("test_key")),TEXT("Checkpoint save roundtrip"));UGameplayStatics::DeleteGameInSlot(TEXT("LostExpedition_SmokeTest"),0);
    Checkpoint=FVector(820,640,2305);Health=1;Respawn();Check(GetActorLocation().Equals(Checkpoint)&&Health==100&&Invulnerability>0,TEXT("Death returns to checkpoint"));
    Guard->Health=90;Guard->bDead=false;
    auto* Grenade=GetWorld()->SpawnActor<AExpeditionGrenade>(FVector(3320,0,2320),FRotator::ZeroRotator);Grenade->Explode();Check(Guard->Health<90,TEXT("Grenade applies radial damage"));
    FString Report=FString::Printf(TEXT("Lost Expedition runtime smoke test\n%d checks; %d failures\n\n"),Results.Num(),Failed)+FString::Join(Results,TEXT("\n"))+FString::Printf(TEXT("\n\nFixed-step contact metrics: support hand %.3f cm; planted foot %.3f cm; 30/120 Hz body path difference %.3f cm.\n"),LargestContactError,LargestFootError,FVector::Dist(RatePositions[0],RatePositions[2]));
    Report+=TEXT("\nEvaluated Motion Matching selections:\n")+FString::Join(MotionSelections,TEXT("\n"))+TEXT("\n");
    FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectDir()/TEXT("Docs/runtime-test.txt")));
    UE_LOG(LogTemp,Display,TEXT("ADVENTURE_SMOKE_COMPLETE checks=%d failures=%d"),Results.Num(),Failed);
    FPlatformMisc::RequestExitWithStatus(false,Failed==0?0:1);
}
