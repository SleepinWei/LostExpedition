#include "ExplorerCharacter.h"
#include "ExpeditionTower.h"
#include "Components/PoseableMeshComponent.h"
#include "ExpeditionActors.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"

AExplorerCharacter::AExplorerCharacter() {
    PrimaryActorTick.bCanEverTick=true;
    GetCapsuleComponent()->InitCapsuleSize(40,96);
    bUseControllerRotationYaw=false;
    GetCharacterMovement()->bOrientRotationToMovement=true;
    GetCharacterMovement()->RotationRate=FRotator(0,540,0);
    GetCharacterMovement()->MaxWalkSpeed=430;GetCharacterMovement()->JumpZVelocity=540;
    GetCharacterMovement()->AirControl=.4f;GetCharacterMovement()->BrakingDecelerationWalking=1800;
    GetMesh()->SetRelativeLocation(FVector(0,0,-96));GetMesh()->SetRelativeRotation(FRotator(0,-90,0));
    GetMesh()->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")));
    GetMesh()->SetAnimInstanceClass(LoadClass<UAnimInstance>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C")));
    ClimbPose=CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("WallClimbingPose"));ClimbPose->SetupAttachment(RootComponent);
    ClimbPose->SetSkinnedAssetAndUpdate(GetMesh()->GetSkinnedAsset());ClimbPose->SetRelativeTransform(GetMesh()->GetRelativeTransform());
    ClimbPose->SetCollisionEnabled(ECollisionEnabled::NoCollision);ClimbPose->SetVisibility(false);
    GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Boom=CreateDefaultSubobject<USpringArmComponent>(TEXT("ShoulderBoom"));Boom->SetupAttachment(RootComponent);
    Boom->TargetArmLength=360;Boom->SocketOffset=FVector(0,55,65);Boom->bUsePawnControlRotation=true;Boom->bEnableCameraLag=true;Boom->CameraLagSpeed=12;
    Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("AdventureCamera"));Camera->SetupAttachment(Boom);Camera->FieldOfView=80;
    WeaponMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Weapon"));WeaponMesh->SetupAttachment(GetMesh(),TEXT("HandGrip_R"));
    WeaponMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Weapons/Pistol/Meshes/SM_Pistol.SM_Pistol")));
    WeaponMesh->SetRelativeLocation(FVector(0,0,0));WeaponMesh->SetRelativeRotation(FRotator::ZeroRotator);WeaponMesh->SetRelativeScale3D(FVector(1));WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void AExplorerCharacter::BeginPlay() {
    Super::BeginPlay();Checkpoint=GetActorLocation();
    if(!FParse::Param(FCommandLine::Get(),TEXT("AdventureSmokeTest"))) {
        if(auto* S=Cast<UExpeditionSave>(UGameplayStatics::LoadGameFromSlot(TEXT("LostExpedition_Island_Checkpoint"),0))) {
            Checkpoint=S->Checkpoint;Collected=S->Collected;bHasKey=S->bHasKey;bGateOpen=S->bGateOpen;Relics=S->Relics;
            Medkits=S->Medkits;Grenades=S->Grenades;
            Magazine[0]=S->PistolMagazine;Magazine[1]=S->RifleMagazine;Reserve[0]=S->PistolReserve;Reserve[1]=S->RifleReserve;
            SetActorLocation(Checkpoint,false,nullptr,ETeleportType::TeleportPhysics);
        }
    }
    for(TActorIterator<AExpeditionLoot> It(GetWorld());It;++It) if(Collected.Contains(It->ItemId)||(It->Kind==ELootKind::Gate&&bGateOpen)) {It->bUsed=true;It->SetActorHiddenInGame(true);It->SetActorEnableCollision(false);}
    if(auto* PC=Cast<APlayerController>(GetController())){PC->SetControlRotation(FRotator(8,28,0));PC->SetInputMode(FInputModeGameOnly());PC->bShowMouseCursor=false;}
    Notify(TEXT("PALM ISLAND / Follow the forest trail to the ruined tower"));
}
void AExplorerCharacter::SetupPlayerInputComponent(UInputComponent* I) {
    Super::SetupPlayerInputComponent(I);
    I->BindAxis(TEXT("Forward"),this,&AExplorerCharacter::Forward);I->BindAxis(TEXT("Right"),this,&AExplorerCharacter::Right);
    I->BindAxis(TEXT("Yaw"),this,&AExplorerCharacter::Yaw);I->BindAxis(TEXT("Pitch"),this,&AExplorerCharacter::Pitch);
    I->BindAction(TEXT("Jump"),IE_Pressed,this,&AExplorerCharacter::JumpOrClimb);
    I->BindAction(TEXT("Jump"),IE_Released,this,&ACharacter::StopJumping);
    I->BindAction(TEXT("Drop"),IE_Pressed,this,&AExplorerCharacter::Drop);
    I->BindAction(TEXT("Interact"),IE_Pressed,this,&AExplorerCharacter::Interact);
    I->BindAction(TEXT("Sprint"),IE_Pressed,this,&AExplorerCharacter::SprintStart);I->BindAction(TEXT("Sprint"),IE_Released,this,&AExplorerCharacter::SprintStop);
    I->BindAction(TEXT("Aim"),IE_Pressed,this,&AExplorerCharacter::AimStart);I->BindAction(TEXT("Aim"),IE_Released,this,&AExplorerCharacter::AimStop);
    I->BindAction(TEXT("Fire"),IE_Pressed,this,&AExplorerCharacter::StartFire);I->BindAction(TEXT("Fire"),IE_Released,this,&AExplorerCharacter::StopFire);
    I->BindAction(TEXT("Reload"),IE_Pressed,this,&AExplorerCharacter::Reload);
    I->BindAction(TEXT("Pistol"),IE_Pressed,this,&AExplorerCharacter::Pistol);I->BindAction(TEXT("Rifle"),IE_Pressed,this,&AExplorerCharacter::Rifle);
    I->BindAction(TEXT("Medkit"),IE_Pressed,this,&AExplorerCharacter::Heal);I->BindAction(TEXT("Grenade"),IE_Pressed,this,&AExplorerCharacter::ThrowGrenade);
    I->BindAction(TEXT("Journal"),IE_Pressed,this,&AExplorerCharacter::Journal);I->BindAction(TEXT("Restart"),IE_Pressed,this,&AExplorerCharacter::FreshStart);
}
void AExplorerCharacter::Forward(float V) {
    ForwardInput=V;
    if(Traversal==ETraversalState::Clinging&&!bJournal)MoveWallGrip(RightInput,V);
    if(Controller&&Traversal==ETraversalState::Walking&&!bJournal&&!bCompleted) AddMovementInput(FRotator(0,Controller->GetControlRotation().Yaw,0).Vector(),V);
}
void AExplorerCharacter::Right(float V) {
    RightInput=V;
    if(Traversal==ETraversalState::Clinging&&!bJournal)MoveWallGrip(V,ForwardInput);
    if(Controller&&Traversal==ETraversalState::Walking&&!bJournal&&!bCompleted) AddMovementInput(FRotationMatrix(FRotator(0,Controller->GetControlRotation().Yaw,0)).GetUnitAxis(EAxis::Y),V);
}
void AExplorerCharacter::Yaw(float V){if(!bJournal)AddControllerYawInput(V*.8f);}
void AExplorerCharacter::Pitch(float V){if(!bJournal)AddControllerPitchInput(V*.65f);}
void AExplorerCharacter::SprintStart(){bSprint=true;}void AExplorerCharacter::SprintStop(){bSprint=false;}
void AExplorerCharacter::AimStart(){bAim=true;}void AExplorerCharacter::AimStop(){bAim=false;}
void AExplorerCharacter::Journal(){bJournal=!bJournal;if(bJournal){StopFire();GetCharacterMovement()->StopMovementImmediately();}}
void AExplorerCharacter::Notify(const FString& T){Notice=T;NoticeRemaining=4;}
void AExplorerCharacter::Tick(float DT) {
    Super::Tick(DT);ShotCooldown=FMath::Max(0.f,ShotCooldown-DT);NoticeRemaining-=DT;DamageFlash=FMath::Max(0.f,DamageFlash-DT);Invulnerability-=DT;LedgeCooldown-=DT;GripCooldown-=DT;
    if(GetActorLocation().Z<-180){Respawn();return;}
    Camera->FieldOfView=FMath::FInterpTo(Camera->FieldOfView,bAim?58:80,DT,9);
    Boom->TargetArmLength=FMath::FInterpTo(Boom->TargetArmLength,bAim?200:360,DT,9);
    bUseControllerRotationYaw=bAim&&Traversal==ETraversalState::Walking;
    GetCharacterMovement()->bOrientRotationToMovement=!bUseControllerRotationYaw;
    GetCharacterMovement()->MaxWalkSpeed=bAim?240:(bSprint?650:430);
    WeaponMesh->SetVisibility(Traversal==ETraversalState::Walking&&!bCompleted);
    if(bReloading) {
        ReloadRemaining-=DT;
        if(ReloadRemaining<=0){int32 N=FMath::Min(Capacity[Weapon]-Magazine[Weapon],Reserve[Weapon]);Magazine[Weapon]+=N;Reserve[Weapon]-=N;bReloading=false;}
    }
    if(bFiring&&Weapon==1)FireShot();
    if(Traversal==ETraversalState::Reaching) {
        ReachTime+=DT;float T=FMath::Clamp(ReachTime/.48f,0.f,1.f),Ease=FMath::SmoothStep(0.f,1.f,T);
        FVector Target=FMath::Lerp(ReachStart,ExpeditionTower::HangPosition(TargetGrip),Ease);
        if(bEnteringFromRoof){FVector Above=ExpeditionTower::HangPosition(TargetGrip);Above.Z=ExpeditionTower::SummitZ()+110;Target=T<.45f?FMath::Lerp(ReachStart,Above,FMath::SmoothStep(0.f,.45f,T)):FMath::Lerp(Above,ExpeditionTower::HangPosition(TargetGrip),FMath::SmoothStep(.45f,1.f,T));}
        FHitResult Hit;SetActorLocation(Target,true,&Hit);
        Ledge=FMath::Lerp(ExpeditionTower::Grip(CurrentGrip),ExpeditionTower::Grip(TargetGrip),Ease);
        if(Hit.bBlockingHit){Drop();Notify(TEXT("No clear reach to that handhold"));}
        else if(T>=1){CurrentGrip=TargetGrip;bEnteringFromRoof=false;Traversal=ETraversalState::Clinging;GripCooldown=.12f;}
    } else if(Traversal==ETraversalState::Mantling) {
        MantleTime+=DT;float T=FMath::Clamp(MantleTime/.65f,0.f,1.f);
        FVector Above=MantleStart;Above.Z=MantleEnd.Z+15;
        FVector P=T<.55f?FMath::Lerp(MantleStart,Above,FMath::SmoothStep(0.f,.55f,T)):FMath::Lerp(Above,MantleEnd,FMath::SmoothStep(.55f,1.f,T));
        FHitResult Hit;SetActorLocation(P,true,&Hit);
        if(Hit.bBlockingHit){Drop();Notify(TEXT("Climb blocked"));}
        else if(T>=1){CurrentGrip=-1;Traversal=ETraversalState::Walking;GetCharacterMovement()->SetMovementMode(MOVE_Walking);LedgeCooldown=.4;}
    } else if(Traversal==ETraversalState::Hanging) {
        if(FMath::Abs(RightInput)>.1f&&!bJournal) {
            FVector Tangent=FVector::CrossProduct(FVector::UpVector,-WallNormal);
            FVector E,N,At=GetActorLocation()+Tangent*RightInput*DT*145;
            if(TryLedge(At,-WallNormal,E,N)&&FMath::Abs(E.Z-Ledge.Z)<45&&FVector::DotProduct(N,WallNormal)>.8f){Ledge=E;WallNormal=N;SetActorLocation(Ledge+WallNormal*48-FVector(0,0,100),true);}
        }
    } else if(GetCharacterMovement()->IsFalling()&&GetVelocity().Z<100&&LedgeCooldown<=0&&!bJournal) BeginHang();
    UpdateClimbPose();RefreshLoot();
}
bool AExplorerCharacter::TryLedge(const FVector& At,const FVector& D,FVector& Edge,FVector& Normal) const {
    FCollisionQueryParams Params(SCENE_QUERY_STAT(Climb),false,this);FHitResult Wall,Top;
    if(!GetWorld()->LineTraceSingleByChannel(Wall,At+FVector(0,0,45),At+FVector(0,0,45)+D*135,ECC_Visibility,Params))return false;
    if(!Wall.Component.IsValid()||!Wall.Component->ComponentHasTag(TEXT("Climbable"))||FMath::Abs(Wall.ImpactNormal.Z)>.25)return false;
    FVector Above=Wall.ImpactPoint-Wall.ImpactNormal*24;Above.Z=At.Z+265;
    if(!GetWorld()->LineTraceSingleByChannel(Top,Above,Above-FVector(0,0,300),ECC_Visibility,Params)||Top.ImpactNormal.Z<.75f)return false;
    float Height=Top.ImpactPoint.Z-At.Z;if(Height<35||Height>250)return false;
    Edge=FVector(Wall.ImpactPoint.X,Wall.ImpactPoint.Y,Top.ImpactPoint.Z);Normal=Wall.ImpactNormal;return true;
}
bool AExplorerCharacter::BeginHang() {
    if(Traversal!=ETraversalState::Walking||bCompleted||LedgeCooldown>0)return false;
    if(BeginWallGrip())return true;
    FVector Direction=GetActorForwardVector();Direction.Z=0;
    if(!TryLedge(GetActorLocation(),Direction,Ledge,WallNormal))return false;
    FVector Target=Ledge+WallNormal*48-FVector(0,0,100);
    if(GetCharacterMovement()->IsMovingOnGround())Target.Z=FMath::Max(Target.Z,GetActorLocation().Z);
    FCollisionQueryParams Params;Params.AddIgnoredActor(this);
    if(GetWorld()->OverlapBlockingTestByChannel(Target,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(38,94),Params))return false;
    Traversal=ETraversalState::Hanging;GetCharacterMovement()->StopMovementImmediately();GetCharacterMovement()->SetMovementMode(MOVE_Flying);
    SetActorLocation(Target);SetActorRotation((-WallNormal).Rotation());StopFire();bReloading=false;return true;
}
bool AExplorerCharacter::BeginMantle() {
    if(Traversal==ETraversalState::Clinging) {
        if(CurrentGrip!=ExpeditionTower::Steps-1){Notify(TEXT("W / S climb  A / D traverse  /  follow the stone handholds"));return false;}
        Ledge=ExpeditionTower::Grip(CurrentGrip);WallNormal=ExpeditionTower::WallNormal;
    } else if(Traversal!=ETraversalState::Hanging)return false;
    MantleEnd=Ledge-WallNormal*85+FVector(0,0,99);
    FCollisionQueryParams Params;Params.AddIgnoredActor(this);
    if(GetWorld()->OverlapBlockingTestByChannel(MantleEnd,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(38,94),Params)){Notify(TEXT("No room above this ledge"));return false;}
    MantleStart=GetActorLocation();MantleTime=0;Traversal=ETraversalState::Mantling;return true;
}
void AExplorerCharacter::JumpOrClimb() {
    if(bJournal||bCompleted)return;
    if(Traversal==ETraversalState::Clinging){if(CurrentGrip==ExpeditionTower::Steps-1)BeginMantle();else MoveWallGrip(0,1);return;}
    if(Traversal==ETraversalState::Reaching)return;
    if(Traversal==ETraversalState::Hanging){BeginMantle();return;}
    if(Traversal==ETraversalState::Mantling)return;
    if(BeginHang()){BeginMantle();return;}Jump();
}
void AExplorerCharacter::Drop() {
    if(Traversal==ETraversalState::Walking)return;
    CurrentGrip=-1;bEnteringFromRoof=false;Traversal=ETraversalState::Walking;GetCharacterMovement()->SetMovementMode(MOVE_Falling);LedgeCooldown=.7;UpdateClimbPose();
}
void AExplorerCharacter::RefreshLoot() {
    Nearby.Reset();float Best=230;
    for(TActorIterator<AExpeditionLoot> It(GetWorld());It;++It) {
        if(It->bUsed)continue;float D=FVector::Dist(GetActorLocation(),It->GetActorLocation());
        if(It->Kind==ELootKind::Gate)D=FVector::Dist2D(GetActorLocation(),It->GetActorLocation());
        if(D>=Best)continue;
        FHitResult Hit;FCollisionQueryParams Params;Params.AddIgnoredActor(this);Params.AddIgnoredActor(*It);
        if(GetWorld()->LineTraceSingleByChannel(Hit,GetActorLocation()+FVector(0,0,30),It->GetActorLocation(),ECC_Visibility,Params))continue;
        Best=D;Nearby=*It;
    }
}
void AExplorerCharacter::Interact() {
    if(bJournal||bCompleted)return;
    if(Traversal==ETraversalState::Clinging){BeginMantle();return;}
    if(Traversal==ETraversalState::Reaching)return;
    RefreshLoot();if(Nearby.IsValid()){Nearby->Interact(this);return;}
    if(Traversal==ETraversalState::Hanging)BeginMantle();else BeginHang();
}
bool AExplorerCharacter::ReceiveLoot(AExpeditionLoot* L) {
    switch(L->Kind) {
        case ELootKind::Ammo:Reserve[0]+=24;Reserve[1]+=60;break;
        case ELootKind::Medkit:if(Medkits>=5){Notify(TEXT("Medkits full (5)"));return false;}Medkits++;break;
        case ELootKind::Grenade:if(Grenades>=6){Notify(TEXT("Grenades full (6)"));return false;}Grenades++;break;
        case ELootKind::Key:bHasKey=true;break;
        case ELootKind::Relic:Relics++;break;
        case ELootKind::Checkpoint:Checkpoint=GetActorLocation();SaveCheckpoint();Health=100;Notify(TEXT("Checkpoint saved / health restored"));return true;
        case ELootKind::Gate:if(!bHasKey){Notify(TEXT("Find the tower key on the highland"));return false;}bGateOpen=true;break;
        case ELootKind::Exit:if(!bGateOpen||Relics<3){Notify(TEXT("Collect 3 relics and unlock the tower doorway"));return false;}bCompleted=true;StopFire();Notify(TEXT("EXPEDITION COMPLETE"));return true;
    }
    if(!L->ItemId.IsNone())Collected.AddUnique(L->ItemId);
    Notify(L->Prompt());return true;
}
void AExplorerCharacter::Heal() {
    if(bCompleted||bJournal||Health>=100||Medkits<=0)return;Medkits--;Health=FMath::Min(100.f,Health+60);Notify(TEXT("Medkit used / +60 health"));
}
void AExplorerCharacter::Equip(int32 I) {
    if(Traversal!=ETraversalState::Walking)return;Weapon=I;bReloading=false;bFiring=false;
    WeaponMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,I==0?TEXT("/Game/Weapons/Pistol/Meshes/SM_Pistol.SM_Pistol"):TEXT("/Game/Weapons/Rifle/Meshes/SM_Rifle.SM_Rifle")));
    WeaponMesh->SetRelativeScale3D(FVector(1));
}
void AExplorerCharacter::Pistol(){Equip(0);}void AExplorerCharacter::Rifle(){Equip(1);}
void AExplorerCharacter::Reload() {
    if(bReloading||Magazine[Weapon]>=Capacity[Weapon]||Reserve[Weapon]<=0||Traversal!=ETraversalState::Walking||bJournal||bCompleted)return;
    bReloading=true;ReloadRemaining=Weapon==0?1.2:1.8;
}
void AExplorerCharacter::StartFire(){bFiring=true;FireShot();}void AExplorerCharacter::StopFire(){bFiring=false;}
void AExplorerCharacter::FireShot() {
    if(ShotCooldown>0||bReloading||Traversal!=ETraversalState::Walking||bJournal||bCompleted)return;
    if(Magazine[Weapon]<=0){Reload();return;}
    Magazine[Weapon]--;ShotCooldown=Weapon==0?.24f:.105f;
    FVector Start=Camera->GetComponentLocation(),D=Camera->GetForwardVector();
    float Spread=bAim?.0025f:.016f;D=FMath::VRandCone(D,Spread);
    FHitResult AimHit,Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(Weapon),true,this);
    FVector End=Start+D*12000;
    if(GetWorld()->LineTraceSingleByChannel(AimHit,Start,End,ECC_Visibility,Params))End=AimHit.ImpactPoint;
    FVector Muzzle=GetMesh()->GetSocketLocation(TEXT("hand_r"))+GetActorForwardVector()*38;
    if(GetWorld()->LineTraceSingleByChannel(Hit,Muzzle,End+D*10,ECC_Visibility,Params)) {
        End=Hit.ImpactPoint;
        float Damage=Weapon==0?34:22;
        if(Hit.BoneName.ToString().Contains(TEXT("head")))Damage*=2;
        UGameplayStatics::ApplyPointDamage(Hit.GetActor(),Damage,(End-Muzzle).GetSafeNormal(),Hit,GetController(),this,UDamageType::StaticClass());
        DrawDebugPoint(GetWorld(),End,9,FColor(255,220,140),false,.25);
    }
    DrawDebugLine(GetWorld(),Muzzle,End,FColor(255,208,105),false,.09,0,1.6);
    AddControllerPitchInput(-.13f);
}
void AExplorerCharacter::ThrowGrenade() {
    if(Grenades<=0||bJournal||bCompleted||Traversal!=ETraversalState::Walking)return;
    FActorSpawnParameters P;P.Owner=this;P.Instigator=this;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
    auto* G=GetWorld()->SpawnActor<AExpeditionGrenade>(GetActorLocation()+GetActorForwardVector()*75+FVector(0,0,65),(Camera->GetForwardVector()+FVector(0,0,.25)).Rotation(),P);
    if(G)Grenades--;
}
float AExplorerCharacter::TakeDamage(float D,const FDamageEvent& E,AController* I,AActor* C) {
    if(Invulnerability>0||bCompleted||bJournal)return 0;float Applied=FMath::Max(0.f,D);
    Health=FMath::Max(0.f,Health-Applied);DamageFlash=.3;Invulnerability=.2;
    if(Health<=0)Respawn();return Applied;
}
void AExplorerCharacter::Landed(const FHitResult& Hit) {
    float Speed=-GetVelocity().Z;Super::Landed(Hit);if(Speed>1100)UGameplayStatics::ApplyDamage(this,(Speed-1100)*.07f,nullptr,this,UDamageType::StaticClass());
}
void AExplorerCharacter::SaveCheckpoint() {
    auto* S=Cast<UExpeditionSave>(UGameplayStatics::CreateSaveGameObject(UExpeditionSave::StaticClass()));
    S->Checkpoint=Checkpoint;S->Collected=Collected;S->bHasKey=bHasKey;S->bGateOpen=bGateOpen;S->Relics=Relics;
    S->Medkits=Medkits;S->Grenades=Grenades;S->PistolMagazine=Magazine[0];S->RifleMagazine=Magazine[1];S->PistolReserve=Reserve[0];S->RifleReserve=Reserve[1];
    if(!UGameplayStatics::SaveGameToSlot(S,TEXT("LostExpedition_Island_Checkpoint"),0))Notify(TEXT("Checkpoint could not be saved"));
}
void AExplorerCharacter::Respawn() {
    Drop();GetCharacterMovement()->StopMovementImmediately();SetActorLocation(Checkpoint,false,nullptr,ETeleportType::TeleportPhysics);
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);Health=100;Invulnerability=3;bReloading=false;bFiring=false;
    if(auto* PC=Cast<APlayerController>(GetController()))PC->SetControlRotation(FRotator(8,28,0));Notify(TEXT("Returned to checkpoint"));
}
void AExplorerCharacter::FreshStart() {
    UGameplayStatics::DeleteGameInSlot(TEXT("LostExpedition_Island_Checkpoint"),0);
    UGameplayStatics::OpenLevel(this,FName(TEXT("CliffSanctuary")));
}

bool AExplorerCharacter::BeginWallGrip() {
    if(Traversal!=ETraversalState::Walking||bCompleted||bJournal||LedgeCooldown>0)return false;
    const FVector RoofGrip=ExpeditionTower::Grip(ExpeditionTower::Steps-1),At=GetActorLocation();
    if(FMath::Abs(At.Z-(RoofGrip.Z+99))<25&&At.X>RoofGrip.X+40&&At.X<RoofGrip.X+190&&FMath::Abs(At.Y-RoofGrip.Y)<100&&FVector::DotProduct(GetActorForwardVector(),ExpeditionTower::WallNormal)>.6f) {
        CurrentGrip=TargetGrip=ExpeditionTower::Steps-1;Ledge=RoofGrip;WallNormal=ExpeditionTower::WallNormal;
        ReachStart=At;ReachTime=0;bEnteringFromRoof=true;Traversal=ETraversalState::Reaching;
        GetCharacterMovement()->StopMovementImmediately();GetCharacterMovement()->SetMovementMode(MOVE_Flying);SetActorRotation((-WallNormal).Rotation());StopFire();return true;
    }
    if(FVector::DotProduct(GetActorForwardVector(),-ExpeditionTower::WallNormal)<.6f)return false;
    int32 Best=-1;float Distance=160;
    for(int32 I=0;I<ExpeditionTower::Steps;I++) {
        const FVector P=ExpeditionTower::Grip(I),To=P-(GetActorLocation()+FVector(0,0,105));
        if(To.X<10||To.X>145||FMath::Abs(To.Z)>115)continue;
        if(To.Size()<Distance){Distance=To.Size();Best=I;}
    }
    if(Best<0)return false;
    FCollisionQueryParams Query;Query.AddIgnoredActor(this);
    const FVector Target=ExpeditionTower::HangPosition(Best);
    if(GetWorld()->OverlapBlockingTestByChannel(Target,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(38,94),Query))return false;
    FHitResult Hit;GetWorld()->SweepSingleByChannel(Hit,GetActorLocation(),Target,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(38,94),Query);
    if(Hit.bBlockingHit)return false;
    CurrentGrip=Best;WallNormal=ExpeditionTower::WallNormal;Ledge=ExpeditionTower::Grip(Best);
    GetCharacterMovement()->StopMovementImmediately();GetCharacterMovement()->SetMovementMode(MOVE_Flying);
    SetActorLocation(Target);SetActorRotation((-WallNormal).Rotation());Traversal=ETraversalState::Clinging;
    StopFire();bReloading=false;GripCooldown=.15f;UpdateClimbPose();return true;
}
bool AExplorerCharacter::MoveWallGrip(float Horizontal,float Vertical) {
    if(Traversal!=ETraversalState::Clinging||GripCooldown>0||bJournal)return false;
    const FVector2D Input=FVector2D(Horizontal,Vertical).GetSafeNormal();if(Input.IsNearlyZero())return false;
    int32 Candidate=-1;float Score=0;
    for(int32 Index:{CurrentGrip-1,CurrentGrip+1}) {
        if(Index<0||Index>=ExpeditionTower::Steps)continue;
        const FVector Delta=ExpeditionTower::Grip(Index)-ExpeditionTower::Grip(CurrentGrip);
        float Alignment=FVector2D::DotProduct(Input,FVector2D(Delta.Y,Delta.Z).GetSafeNormal());
        if(Delta.Size()>260||Alignment<.4f||Alignment<=Score)continue;Score=Alignment;Candidate=Index;
    }
    if(Candidate<0)return false;
    FCollisionQueryParams Query;Query.AddIgnoredActor(this);FHitResult Hit;
    if(GetWorld()->SweepSingleByChannel(Hit,GetActorLocation(),ExpeditionTower::HangPosition(Candidate),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(38,94),Query))return false;
    bEnteringFromRoof=false;TargetGrip=Candidate;ReachStart=GetActorLocation();ReachTime=0;Traversal=ETraversalState::Reaching;return true;
}
void AExplorerCharacter::UpdateClimbPose() {
    const bool Show=Traversal==ETraversalState::Clinging||Traversal==ETraversalState::Reaching||Traversal==ETraversalState::Hanging;
    GetMesh()->SetVisibility(!Show);ClimbPose->SetVisibility(Show);if(!Show)return;
    ClimbPose->CopyPoseFromSkeletalComponent(GetMesh());
    const FVector Side=FVector::CrossProduct(FVector::UpVector,-WallNormal),Body=GetActorLocation();
    auto Solve=[&](FName Upper,FName Lower,FName End,FVector Target,FVector Pole) {
        FTransform Root=ClimbPose->GetBoneTransformByName(Upper,EBoneSpaces::WorldSpace);
        FVector A=Root.GetLocation(),B=ClimbPose->GetBoneLocationByName(Lower,EBoneSpaces::WorldSpace),C=ClimbPose->GetBoneLocationByName(End,EBoneSpaces::WorldSpace);
        float L1=FVector::Dist(A,B),L2=FVector::Dist(B,C);if(L1<1||L2<1)return;
        FVector Direction=(Target-A).GetSafeNormal();float D=FMath::Clamp(FVector::Dist(A,Target),FMath::Abs(L1-L2)+1,L1+L2-1);
        Target=A+Direction*D;
        FVector Bend=(Pole-A)-Direction*FVector::DotProduct(Pole-A,Direction);Bend.Normalize();
        float Along=(L1*L1-L2*L2+D*D)/(2*D);FVector Joint=A+Direction*Along+Bend*FMath::Sqrt(FMath::Max(0.f,L1*L1-Along*Along));
        Root.SetRotation(FQuat::FindBetweenNormals((B-A).GetSafeNormal(),(Joint-A).GetSafeNormal())*Root.GetRotation());
        ClimbPose->SetBoneTransformByName(Upper,Root,EBoneSpaces::WorldSpace);
        FTransform Mid=ClimbPose->GetBoneTransformByName(Lower,EBoneSpaces::WorldSpace);
        FVector Tip=ClimbPose->GetBoneLocationByName(End,EBoneSpaces::WorldSpace);
        Mid.SetRotation(FQuat::FindBetweenNormals((Tip-Mid.GetLocation()).GetSafeNormal(),(Target-Mid.GetLocation()).GetSafeNormal())*Mid.GetRotation());
        ClimbPose->SetBoneTransformByName(Lower,Mid,EBoneSpaces::WorldSpace);
    };
    Solve(TEXT("upperarm_l"),TEXT("lowerarm_l"),TEXT("hand_l"),Ledge-Side*24+WallNormal*3,Body-Side*65+WallNormal*45+FVector(0,0,45));
    Solve(TEXT("upperarm_r"),TEXT("lowerarm_r"),TEXT("hand_r"),Ledge+Side*24+WallNormal*3,Body+Side*65+WallNormal*45+FVector(0,0,45));
    Solve(TEXT("thigh_l"),TEXT("calf_l"),TEXT("foot_l"),Body-WallNormal*65-Side*24-FVector(0,0,62),Body-WallNormal*90-Side*36-FVector(0,0,15));
    Solve(TEXT("thigh_r"),TEXT("calf_r"),TEXT("foot_r"),Body-WallNormal*65+Side*24-FVector(0,0,76),Body-WallNormal*90+Side*36-FVector(0,0,28));
    ClimbPose->RefreshBoneTransforms();
}
