#include "ExplorerCharacter.h"
#include "ExpeditionActors.h"
#include "ExpeditionWorld.h"
#include "ExpeditionTower.h"
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
    int32 WorldCount=0;for(TActorIterator<AExpeditionWorld> It(GetWorld());It;++It){WorldCount++;Check(It->Pieces.Num()>300,TEXT("Authored ruins and foliage generated"));
        int32 OfficialTrees=0,Grass=0,PBR=0;
        for(auto Part:It->Pieces)if(Part&&Part->GetStaticMesh()){
            FString Path=Part->GetStaticMesh()->GetPathName();
            if(Path.Contains(TEXT("ArchVis/SampleScene/Tree/HillTree_02")))OfficialTrees++;
            if(Path.Contains(TEXT("SM_TownGrass")))Grass++;
            if(Part->GetMaterial(0)&&Part->GetMaterial(0)->GetPathName().Contains(TEXT("M_ExpeditionStone")))PBR++;
        }
        Check(OfficialTrees>0,TEXT("Official UE tree integrated in inland slope"));
        Check(Grass>100,TEXT("Textured free grass replaces placeholder foliage"));
        Check(PBR>10,TEXT("PBR stone material applied to ruins"));}
    Check(WorldCount==1,TEXT("Exactly one environment actor"));
    bool BridgeContinuous=true;
    for(float X=3915;X<5110;X+=20){FHitResult Hit;FCollisionQueryParams Params;Params.AddIgnoredActor(this);bool Found=GetWorld()->SweepSingleByChannel(Hit,FVector(X,0,1000),FVector(X,0,0),FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(38),Params);if(!Found||Hit.ImpactPoint.Z<540)BridgeContinuous=false;}
    Check(BridgeContinuous,TEXT("Suspension bridge supports character-sized collision"));
    bool StairsAccessible=true;float Previous=620;
    for(float X=7440;X<8010;X+=15){FHitResult Hit;FCollisionQueryParams Params;Params.AddIgnoredActor(this);bool Found=GetWorld()->LineTraceSingleByChannel(Hit,FVector(X,0,1300),FVector(X,0,400),ECC_Visibility,Params);if(!Found||Hit.ImpactPoint.Z-Previous>45)StairsAccessible=false;if(Found)Previous=Hit.ImpactPoint.Z;}
    Check(StairsAccessible,TEXT("Sanctuary stairs remain within character step height"));
    for(TActorIterator<AExpeditionGuard> It(GetWorld());It;++It)It->bTrainingTarget=true;
    for(int32 Step=0;Step<3;Step++) {
        const float X[3]={-780,-90,670};const float Z[3]={98,288,498};const float Top[3]={190,400,620};
        SetActorLocation(FVector(X[Step],0,Z[Step]));SetActorRotation(FRotator::ZeroRotator);Traversal=ETraversalState::Walking;LedgeCooldown=0;GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        Check(BeginHang(),FString::Printf(TEXT("Grab ledge %d"),Step+1));
        if(Traversal==ETraversalState::Hanging) {
            if(Step==1){FVector Old=GetActorLocation();Right(1);Tick(.1);Right(0);Check(FMath::Abs(GetActorLocation().Y-Old.Y)>5,TEXT("Shimmy along ledge"));}
            Check(BeginMantle(),FString::Printf(TEXT("Start mantle %d"),Step+1));
            for(int32 I=0;I<45;I++)Tick(.016f);
            Check(Traversal==ETraversalState::Walking&&FMath::Abs(GetActorLocation().Z-(Top[Step]+99))<3,FString::Printf(TEXT("Complete collision-safe mantle %d"),Step+1));
        }
    }
    // Walk between every riser with capsule sweeps, then use the real grab/mantle state machine.
    // There is only one initial teleport; the entire tower route must connect from there.
    Traversal=ETraversalState::Walking;LedgeCooldown=0;Right(0);
    SetActorLocation(FVector(6530,-1090,719));GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    auto WalkTo=[&](FVector Target){
        const FVector Start=GetActorLocation();const int32 Samples=FMath::CeilToInt(FVector::Dist(Start,Target)/20.f);
        for(int32 I=1;I<=Samples;I++) {
            FHitResult Hit;SetActorLocation(FMath::Lerp(Start,Target,float(I)/Samples),true,&Hit);
            if(Hit.bBlockingHit)return false;
            FHitResult Floor;FCollisionQueryParams Query;Query.AddIgnoredActor(this);
            if(!GetWorld()->LineTraceSingleByChannel(Floor,GetActorLocation(),GetActorLocation()-FVector(0,0,115),ECC_Visibility,Query))return false;
        }
        return GetActorLocation().Equals(Target,2.f);
    };
    Check(WalkTo(ExpeditionTower::Terrace(0)+FVector(0,0,99)),TEXT("Walk from courtyard through wall gap onto tower approach"));
    bool TowerRoute=true;
    for(int32 Stage=1;Stage<=ExpeditionTower::Steps;Stage++) {
        const FVector Top=ExpeditionTower::Terrace(Stage),D=ExpeditionTower::Direction(Stage);
        const FVector PreviousTop=ExpeditionTower::Terrace(Stage-1);
        bool Walked=WalkTo(PreviousTop+FVector(0,0,99))&&WalkTo(Top-D*280-FVector(0,0,101));
        SetActorRotation(D.Rotation());LedgeCooldown=0;GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        bool Grabbed=Walked&&BeginHang();bool Mantled=Grabbed&&BeginMantle();
        if(Mantled)for(int32 I=0;I<45;I++)Tick(.016f);
        bool Finished=Mantled&&Traversal==ETraversalState::Walking&&FMath::Abs(GetActorLocation().Z-(Top.Z+99))<3;
        Check(Finished,FString::Printf(TEXT("Tower continuous approach, grab and mantle %02d / 21"),Stage));
        if(!Finished){TowerRoute=false;break;}
    }
    bool SummitWalk=TowerRoute&&WalkTo(ExpeditionTower::Terrace(21)+FVector(0,0,99))&&WalkTo(FVector(6930,-3400,4919))&&WalkTo(FVector(7330,-3000,4919));
    Check(SummitWalk&&FMath::Abs(GetActorLocation().Z-4919)<3,TEXT("Reach the open summit deck 42 metres above the entrance"));
    RefreshLoot();Check(SummitWalk&&Nearby.IsValid()&&Nearby->ItemId==TEXT("TowerSummitCheckpoint"),TEXT("Summit checkpoint is visible and interactable"));
    int32 TowerCheckpoints=0;
    for(TActorIterator<AExpeditionLoot> It(GetWorld());It;++It)if(It->Kind==ELootKind::Checkpoint&&It->ItemId.ToString().StartsWith(TEXT("Tower"))) {
        FHitResult Floor;FCollisionQueryParams Query;Query.AddIgnoredActor(this);Query.AddIgnoredActor(*It);
        if(GetWorld()->LineTraceSingleByChannel(Floor,It->GetActorLocation(),It->GetActorLocation()-FVector(0,0,80),ECC_Visibility,Query)&&Floor.ImpactNormal.Z>.9)TowerCheckpoints++;
    }
    Check(TowerCheckpoints==4,TEXT("All four tower checkpoints stand on collision-supported landings"));
    SetActorLocation(FVector(-90,0,288));SetActorRotation(FRotator::ZeroRotator);LedgeCooldown=0;Traversal=ETraversalState::Walking;BeginHang();
    auto* Obstacle=GetWorld()->SpawnActor<AActor>();
    auto* Box=NewObject<UBoxComponent>(Obstacle);Obstacle->SetRootComponent(Box);Box->SetBoxExtent(FVector(100));Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->RegisterComponent();Obstacle->SetActorLocation(FVector(75,0,499));
    Check(!BeginMantle(),TEXT("Blocked mantle rejected"));Obstacle->Destroy();Drop();Check(Traversal==ETraversalState::Walking&&GetCharacterMovement()->IsFalling(),TEXT("Drop releases ledge"));
    SetActorLocation(FVector(2600,0,719));GetCharacterMovement()->SetMovementMode(MOVE_Walking);LedgeCooldown=1;
    auto* Guard=GetWorld()->SpawnActor<AExpeditionGuard>(FVector(3000,0,620),FRotator(0,-90,0));Guard->bTrainingTarget=true;
    Camera->SetWorldLocation(FVector(2700,0,760));Camera->SetWorldRotation((FVector(3000,0,715)-Camera->GetComponentLocation()).Rotation());
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
    auto* Save=Cast<UExpeditionSave>(UGameplayStatics::CreateSaveGameObject(UExpeditionSave::StaticClass()));Save->Checkpoint=FVector(1700,0,720);Save->Relics=3;Save->bHasKey=true;Save->Collected=Collected;
    bool Saved=UGameplayStatics::SaveGameToSlot(Save,TEXT("LostExpedition_SmokeTest"),0);auto* Loaded=Cast<UExpeditionSave>(UGameplayStatics::LoadGameFromSlot(TEXT("LostExpedition_SmokeTest"),0));
    Check(Saved&&Loaded&&Loaded->Checkpoint.Equals(Save->Checkpoint)&&Loaded->Relics==3&&Loaded->Collected.Contains(TEXT("test_key")),TEXT("Checkpoint save roundtrip"));UGameplayStatics::DeleteGameInSlot(TEXT("LostExpedition_SmokeTest"),0);
    Checkpoint=FVector(1700,0,720);Health=1;Respawn();Check(GetActorLocation().Equals(Checkpoint)&&Health==100&&Invulnerability>0,TEXT("Death returns to checkpoint"));
    Guard->Health=90;Guard->bDead=false;
    auto* Grenade=GetWorld()->SpawnActor<AExpeditionGrenade>(FVector(2920,0,740),FRotator::ZeroRotator);Grenade->Explode();Check(Guard->Health<90,TEXT("Grenade applies radial damage"));
    FString Report=FString::Printf(TEXT("Lost Expedition runtime smoke test\n%d checks; %d failures\n\n"),Results.Num(),Failed)+FString::Join(Results,TEXT("\n"))+TEXT("\n");
    FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectDir()/TEXT("Docs/runtime-test.txt")));
    UE_LOG(LogTemp,Display,TEXT("ADVENTURE_SMOKE_COMPLETE checks=%d failures=%d"),Results.Num(),Failed);
    FPlatformMisc::RequestExitWithStatus(false,Failed==0?0:1);
}
