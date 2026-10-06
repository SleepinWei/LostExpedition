#include "ExplorerCharacter.h"
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
    auto Advance=[&](float Duration){for(float Time=0;Time<Duration;Time+=.016f){Tick(.016f);UpdateClimbPose();}};
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
    SetActorRotation(FRotator::ZeroRotator);Check(BeginHang()&&Traversal==ETraversalState::Clinging,TEXT("Grab the first stone handle from the ground"));
    Check(!BeginMantle(),TEXT("Intermediate wall handles cannot be used as walkable platforms"));
    Advance(.28f);Check(ClimbPose->IsVisible()&&ClimbPose->GetBoneLocationByName(TEXT("hand_l"),EBoneSpaces::WorldSpace).Z>GetActorLocation().Z+65,TEXT("Procedural wall pose lifts the hands to the stone grip"));
    GripCooldown=0;
    auto* Obstacle=GetWorld()->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Obstacle);Obstacle->SetRootComponent(Box);Box->SetBoxExtent(FVector(45,55,45));Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->RegisterComponent();Obstacle->SetActorLocation(ExpeditionTower::HangPosition(1));
    Check(!MoveWallGrip(0,1),TEXT("Obstructed wall reach is rejected"));Obstacle->Destroy();
    auto ResetGrip=[&](int32 Index){
        Drop();Traversal=ETraversalState::Walking;GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        SetActorLocation(ExpeditionTower::HangPosition(Index)+FVector(-40,0,0));SetActorRotation(FRotator::ZeroRotator);LedgeCooldown=0;BeginWallGrip();Advance(.4f);GripCooldown=0;
    };
    FVector RatePositions[3];float LargestContactError=0,LargestFootError=0;
    const int32 Rates[3]={30,60,120};
    for(int R=0;R<3;R++) {
        ResetGrip(0);Forward(1);bool NoIdle=true;
        for(int F=0;F<Rates[R]*2.4f;F++) {
            Tick(1.f/Rates[R]);UpdateClimbPose();
            if(F>0&&Traversal!=ETraversalState::Reaching)NoIdle=false;
            if(Traversal==ETraversalState::Reaching&&ReachTime/ReachDuration<.18f) {
                const int Support=bLeadRight?0:1;
                const float Error=FVector::Dist(ClimbPose->GetBoneLocationByName(Support==0?TEXT("foot_l"):TEXT("foot_r"),EBoneSpaces::WorldSpace),AnimatedFeet[Support]);
                LargestFootError=FMath::Max(LargestFootError,Error);
            }
            if(Traversal==ETraversalState::Reaching&&ReachTime/ReachDuration<.35f) {
                const int Support=bLeadRight?0:1;
                LargestContactError=FMath::Max(LargestContactError,float(FVector::Dist(ClimbPose->GetBoneLocationByName(Support==0?TEXT("hand_l"):TEXT("hand_r"),EBoneSpaces::WorldSpace),AnimatedHands[Support])));
            }
        }
        RatePositions[R]=GetActorLocation();
        Check(NoIdle&&CurrentGrip==3&&TargetGrip==4,FString::Printf(TEXT("Held climb chains four transfers without an idle frame at %d Hz"),Rates[R]));
        Forward(0);const int32 StopGrip=TargetGrip;Advance(.9f);
        Check(Traversal==ETraversalState::Clinging&&CurrentGrip==StopGrip,TEXT("Release finishes the current reach and cancels further held movement"));
    }
    UE_LOG(LogTemp,Display,TEXT("ACTION_CONTACT maxSupportError=%.3f maxFootError=%.3f rateDelta=%.3f"),LargestContactError,LargestFootError,FVector::Dist(RatePositions[0],RatePositions[2]));
    Check(FVector::Dist(RatePositions[0],RatePositions[2])<4,TEXT("30 and 120 Hz traversal paths agree within four centimetres"));
    Check(LargestFootError<8,TEXT("Solved planted foot remains within eight centimetres of its wall contact"));
    Check(LargestContactError<8,TEXT("Solved support hand stays within eight centimetres of its planted contact"));
    ResetGrip(0);MoveWallGrip(0,1);Forward(1);Tick(.05f);Forward(0);Advance(ReachDuration+.05f);
    Check(CurrentGrip==1&&TargetGrip==2&&Traversal==ETraversalState::Reaching,TEXT("A brief direction tap during a reach buffers one next transfer"));
    Advance(.9f);Check(CurrentGrip==2&&Traversal==ETraversalState::Clinging,TEXT("Buffered tap executes once and does not repeat"));
    ResetGrip(0);MoveWallGrip(0,1);Forward(-1);Advance(ReachDuration+.04f);
    Check(CurrentGrip==1&&TargetGrip==0&&Traversal==ETraversalState::Reaching,TEXT("Reverse input queues a descent at the next contact"));Forward(0);Advance(.9f);
    Check(CurrentGrip==0&&Traversal==ETraversalState::Clinging,TEXT("Queued reversal returns to the previous handhold"));
    ResetGrip(0);MoveWallGrip(0,1);Forward(1);Tick(.05f);Journal();Advance(.9f);
    Check(CurrentGrip==1&&Traversal==ETraversalState::Clinging&&QueuedGrip<0,TEXT("Journal cancels buffered and held traversal commands"));Journal();
    ResetGrip(0);Forward(1);Advance(.25f);Forward(0);
    const FVector ReleasePosition=GetActorLocation(),ReleaseVelocity=TraversalVelocity;Tick(.001f);UpdateClimbPose();
    Check(FVector::Dist(GetActorLocation(),ReleasePosition+ReleaseVelocity*.001f)<.1f,TEXT("Releasing mid-reach preserves the body position and velocity when replanning"));Advance(.9f);
    ResetGrip(ExpeditionTower::Steps-2);MoveWallGrip(0,1);JumpOrClimb();Advance(ReachDuration+.08f);
    Check(Traversal==ETraversalState::Mantling&&MantleTime>0&&MantleTime<.12f,TEXT("Buffered Space enters the rooftop mantle at contact and carries only the remaining timestep"));
    ResetGrip(0);
    bool Climbed=Traversal==ETraversalState::Clinging;
    for(int I=1;I<ExpeditionTower::Steps&&Climbed;I++) {
        FVector Delta=ExpeditionTower::Grip(I)-ExpeditionTower::Grip(I-1);GripCooldown=0;
        if(FMath::Abs(Delta.Z)<1)Check(!MoveWallGrip(0,1),TEXT("Horizontal gap requires a deliberate sideways reach"));
        bool Reaching=MoveWallGrip(FMath::Abs(Delta.Z)<1?FMath::Sign(Delta.Y):0,Delta.Z>0?1:0);
        if(Reaching&&I==1) {
            Advance(.22f);
            const int Lead=bLeadRight?1:0,Support=1-Lead;
            const FVector Hand=ClimbPose->GetBoneLocationByName(Lead==0?TEXT("hand_l"):TEXT("hand_r"),EBoneSpaces::WorldSpace);
            const FVector Other=ClimbPose->GetBoneLocationByName(Support==0?TEXT("hand_l"):TEXT("hand_r"),EBoneSpaces::WorldSpace);
            UE_LOG(LogTemp,Display,TEXT("CLIMB_PHASE lead=%d time=%.3f body=%s hand=%s other=%s targets=%s/%s"),Lead,ReachTime,*GetActorLocation().ToString(),*Hand.ToString(),*Other.ToString(),*AnimatedHands[Lead].ToString(),*AnimatedHands[Support].ToString());
            Check(Hand.Z>Other.Z+20,TEXT("Climb animation moves one hand ahead of the supporting hand"));
            Check(ClimbPose->IsVisible()&&!GetMesh()->IsVisible()&&!WeaponMesh->IsVisible(),TEXT("Reaching shows the animated climbing mesh and stows the weapon"));
            Advance(GripTransferDuration-.22f+.03f);
        } else if(Reaching)Advance(GripTransferDuration+.03f);
        Climbed=Reaching&&CurrentGrip==I&&Traversal==ETraversalState::Clinging&&GetActorLocation().Equals(ExpeditionTower::HangPosition(I),2);
        Check(Climbed,FString::Printf(TEXT("Collision-safe handhold transfer %02d / %02d"),I,ExpeditionTower::Steps-1));
    }
    Check(Climbed&&BeginMantle(),TEXT("Start the final mantle from the top stone handle"));
    if(Traversal==ETraversalState::Mantling){Advance(.38f);Check(ClimbPose->IsVisible()&&Traversal==ETraversalState::Mantling,TEXT("Mantle keeps the crouched climbing pose through the pull-up"));Advance(MantleDuration-.38f+.03f);}
    Check(Traversal==ETraversalState::Walking&&FMath::Abs(GetActorLocation().Z-(ExpeditionTower::SummitZ()+99))<3,TEXT("Stand on the ruined tower roof after climbing the entire wall"));
    FHitResult SummitHit;SetActorLocation(ExpeditionTower::Base+FVector(0,0,ExpeditionTower::Height+99),true,&SummitHit);RefreshLoot();
    Check(!SummitHit.bBlockingHit&&Nearby.IsValid()&&Nearby->ItemId==TEXT("TowerSummitCheckpoint"),TEXT("Summit checkpoint can be reached and interacted with"));
    SetActorLocation(ExpeditionTower::Grip(ExpeditionTower::Steps-1)+FVector(90,0,99));SetActorRotation(FRotator(0,180,0));LedgeCooldown=0;
    Check(BeginHang()&&Traversal==ETraversalState::Reaching,TEXT("Enter the top handhold from the roof edge to climb back down"));
    if(Traversal==ETraversalState::Reaching)Advance(RoofEntryDuration+.03f);
    Check(Traversal==ETraversalState::Clinging&&CurrentGrip==ExpeditionTower::Steps-1,TEXT("Reverse mantle clears the roof collision before lowering the character"));
    GripCooldown=0;bool Down=MoveWallGrip(0,-1);if(Down)Advance(GripTransferDuration+.03f);
    Check(Down&&CurrentGrip==ExpeditionTower::Steps-2,TEXT("Descend between wall grips with S"));
    // Releasing a handle must restore gravity and the walking mesh.
    SetActorLocation(ExpeditionTower::HangPosition(0)+FVector(-40,0,0));SetActorRotation(FRotator::ZeroRotator);Traversal=ETraversalState::Walking;LedgeCooldown=0;BeginWallGrip();Drop();
    Check(Traversal==ETraversalState::Walking&&GetCharacterMovement()->IsFalling()&&ClimbPose->IsVisible()&&WeaponMesh->GetAttachParent()==ClimbPose,TEXT("Drop restores gravity while retaining one presentation mesh and weapon socket"));
    SetActorLocation(FVector(3000,0,2299));GetCharacterMovement()->SetMovementMode(MOVE_Walking);LedgeCooldown=1;
    auto* Guard=GetWorld()->SpawnActor<AExpeditionGuard>(FVector(3400,0,2200),FRotator(0,-90,0));Guard->bTrainingTarget=true;
    Camera->SetWorldLocation(FVector(3100,0,2340));Camera->SetWorldRotation((FVector(3400,0,2295)-Camera->GetComponentLocation()).Rotation());
    Check(WeaponIdleAnimations.Num()==2&&WeaponFireAnimations.Num()==2&&WeaponIdleAnimations[0]&&WeaponIdleAnimations[1]&&WeaponFireAnimations[0]&&WeaponFireAnimations[1],TEXT("Official pistol and rifle pose and firing sequences load"));
    AimStart();Weapon=0;Advance(.4f);Magazine[0]=12;ShotCooldown=0;float OldHealth=Guard->Health;FireShot();
    Check(Magazine[0]==11,TEXT("Fire consumes one round"));Check(Guard->Health<OldHealth,TEXT("Hitscan damages skeletal guard"));
    const FQuat IdleArm=ClimbPose->GetBoneTransformByName(TEXT("upperarm_r"),EBoneSpaces::ComponentSpace).GetRotation();
    Advance(.075f);
    const FQuat RecoilArm=ClimbPose->GetBoneTransformByName(TEXT("upperarm_r"),EBoneSpaces::ComponentSpace).GetRotation();
    Check(FireAnimationTime>0&&ClimbPose->IsVisible()&&IdleArm.AngularDistance(RecoilArm)>.01f,TEXT("Successful pistol shot animates the upper body over the aiming pose"));
    Check(WeaponMesh->GetAttachParent()==ClimbPose,TEXT("Weapon follows the animated hand socket"));
    const float ShotPhase=FireAnimationTime;int32 Before=Magazine[0];FireShot();Check(Magazine[0]==Before&&FireAnimationTime==ShotPhase,TEXT("Fire rate blocks both extra rounds and animation retriggers"));
    AimStop();Advance(.9f);Check(FireAnimationTime<0&&ArmAnimationAlpha<.01f&&ClimbPose->IsVisible(),TEXT("Firing animation recovers into locomotion without switching meshes"));
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
    const FVector IdleFoot=ClimbPose->GetBoneLocationByName(TEXT("foot_r"),EBoneSpaces::ComponentSpace);
    GetCharacterMovement()->Velocity=GetActorRightVector()*180;Advance(.25f);
    const FVector StrafeFoot=ClimbPose->GetBoneLocationByName(TEXT("foot_r"),EBoneSpaces::ComponentSpace);
    GetCharacterMovement()->Velocity=-GetActorForwardVector()*180;Advance(.25f);
    Check(FVector::Dist(IdleFoot,StrafeFoot)>5&&FVector::Dist(StrafeFoot,ClimbPose->GetBoneLocationByName(TEXT("foot_r"),EBoneSpaces::ComponentSpace))>5,TEXT("Moving aim selects distinct strafe and backward foot poses"));
    GetCharacterMovement()->Velocity=FVector::ZeroVector;
    Magazine[1]=30;ShotCooldown=0;StartFire();Advance(.30f);StopFire();Check(Magazine[1]<=27&&FireAnimationTime>=0&&ClimbPose->IsVisible(),TEXT("Automatic rifle fire repeatedly triggers the rifle recoil animation"));const FQuat BeforeRetrigger=ClimbPose->GetBoneTransformByName(TEXT("upperarm_r"),EBoneSpaces::ComponentSpace).GetRotation();ShotCooldown=0;FireShot();
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
    FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectDir()/TEXT("Docs/runtime-test.txt")));
    UE_LOG(LogTemp,Display,TEXT("ADVENTURE_SMOKE_COMPLETE checks=%d failures=%d"),Results.Num(),Failed);
    FPlatformMisc::RequestExitWithStatus(false,Failed==0?0:1);
}
