#include "ExplorerCharacter.h"
#include "ExpeditionActors.h"
#include "ExpeditionWorld.h"
#include "ExpeditionTower.h"
#include "IslandTerrain.h"
#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Camera/CameraComponent.h"
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
    UpdateClimbPose();Check(ClimbPose->IsVisible()&&ClimbPose->GetBoneLocationByName(TEXT("hand_l"),EBoneSpaces::WorldSpace).Z>GetActorLocation().Z+65,TEXT("Procedural wall pose lifts the hands to the stone grip"));
    GripCooldown=0;
    auto* Obstacle=GetWorld()->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Obstacle);Obstacle->SetRootComponent(Box);Box->SetBoxExtent(FVector(45,55,45));Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->RegisterComponent();Obstacle->SetActorLocation(ExpeditionTower::HangPosition(1));
    Check(!MoveWallGrip(0,1),TEXT("Obstructed wall reach is rejected"));Obstacle->Destroy();
    bool Climbed=Traversal==ETraversalState::Clinging;
    for(int I=1;I<ExpeditionTower::Steps&&Climbed;I++) {
        FVector Delta=ExpeditionTower::Grip(I)-ExpeditionTower::Grip(I-1);GripCooldown=0;
        if(FMath::Abs(Delta.Z)<1)Check(!MoveWallGrip(0,1),TEXT("Horizontal gap requires a deliberate sideways reach"));
        bool Reaching=MoveWallGrip(FMath::Abs(Delta.Z)<1?FMath::Sign(Delta.Y):0,Delta.Z>0?1:0);
        if(Reaching)for(int K=0;K<34;K++)Tick(.016f);
        Climbed=Reaching&&CurrentGrip==I&&Traversal==ETraversalState::Clinging&&GetActorLocation().Equals(ExpeditionTower::HangPosition(I),2);
        Check(Climbed,FString::Printf(TEXT("Collision-safe handhold transfer %02d / %02d"),I,ExpeditionTower::Steps-1));
    }
    Check(Climbed&&BeginMantle(),TEXT("Start the final mantle from the top stone handle"));
    if(Traversal==ETraversalState::Mantling)for(int I=0;I<45;I++)Tick(.016f);
    Check(Traversal==ETraversalState::Walking&&FMath::Abs(GetActorLocation().Z-(ExpeditionTower::SummitZ()+99))<3,TEXT("Stand on the ruined tower roof after climbing the entire wall"));
    FHitResult SummitHit;SetActorLocation(ExpeditionTower::Base+FVector(0,0,ExpeditionTower::Height+99),true,&SummitHit);RefreshLoot();
    Check(!SummitHit.bBlockingHit&&Nearby.IsValid()&&Nearby->ItemId==TEXT("TowerSummitCheckpoint"),TEXT("Summit checkpoint can be reached and interacted with"));
    SetActorLocation(ExpeditionTower::Grip(ExpeditionTower::Steps-1)+FVector(90,0,99));SetActorRotation(FRotator(0,180,0));LedgeCooldown=0;
    Check(BeginHang()&&Traversal==ETraversalState::Reaching,TEXT("Enter the top handhold from the roof edge to climb back down"));
    if(Traversal==ETraversalState::Reaching)for(int K=0;K<34;K++)Tick(.016f);
    Check(Traversal==ETraversalState::Clinging&&CurrentGrip==ExpeditionTower::Steps-1,TEXT("Reverse mantle clears the roof collision before lowering the character"));
    GripCooldown=0;bool Down=MoveWallGrip(0,-1);if(Down)for(int K=0;K<34;K++)Tick(.016f);
    Check(Down&&CurrentGrip==ExpeditionTower::Steps-2,TEXT("Descend between wall grips with S"));
    // Releasing a handle must restore gravity and the walking mesh.
    SetActorLocation(ExpeditionTower::HangPosition(0)+FVector(-40,0,0));SetActorRotation(FRotator::ZeroRotator);Traversal=ETraversalState::Walking;LedgeCooldown=0;BeginWallGrip();Drop();
    Check(Traversal==ETraversalState::Walking&&GetCharacterMovement()->IsFalling()&&!ClimbPose->IsVisible(),TEXT("Drop releases the wall and restores the normal character mesh"));
    SetActorLocation(FVector(3000,0,2299));GetCharacterMovement()->SetMovementMode(MOVE_Walking);LedgeCooldown=1;
    auto* Guard=GetWorld()->SpawnActor<AExpeditionGuard>(FVector(3400,0,2200),FRotator(0,-90,0));Guard->bTrainingTarget=true;
    Camera->SetWorldLocation(FVector(3100,0,2340));Camera->SetWorldRotation((FVector(3400,0,2295)-Camera->GetComponentLocation()).Rotation());
    AimStart();Weapon=0;Magazine[0]=12;ShotCooldown=0;float OldHealth=Guard->Health;FireShot();
    Check(Magazine[0]==11,TEXT("Fire consumes one round"));Check(Guard->Health<OldHealth,TEXT("Hitscan damages skeletal guard"));
    int32 Before=Magazine[0];FireShot();Check(Magazine[0]==Before,TEXT("Fire rate enforced"));
    Magazine[0]=0;Reserve[0]=5;Reload();Tick(1.3);Check(Magazine[0]==5&&Reserve[0]==0&&!bReloading,TEXT("Reload clamps to available reserve"));
    Magazine[0]=0;Reserve[0]=0;Reload();Check(!bReloading,TEXT("Empty reserve rejects reload"));
    Rifle();Check(Weapon==1&&WeaponMesh->GetStaticMesh()&&WeaponMesh->GetStaticMesh()->GetPathName().Contains(TEXT("SM_Rifle")),TEXT("Weapon swap uses official rifle mesh"));Pistol();Check(Weapon==0&&WeaponMesh->GetStaticMesh()&&WeaponMesh->GetStaticMesh()->GetPathName().Contains(TEXT("SM_Pistol")),TEXT("Pistol selection uses official pistol mesh"));
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
    FString Report=FString::Printf(TEXT("Lost Expedition runtime smoke test\n%d checks; %d failures\n\n"),Results.Num(),Failed)+FString::Join(Results,TEXT("\n"))+TEXT("\n");
    FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectDir()/TEXT("Docs/runtime-test.txt")));
    UE_LOG(LogTemp,Display,TEXT("ADVENTURE_SMOKE_COMPLETE checks=%d failures=%d"),Results.Num(),Failed);
    FPlatformMisc::RequestExitWithStatus(false,Failed==0?0:1);
}
