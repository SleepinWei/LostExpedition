#include "ExplorerCharacter.h"
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
    Boom=CreateDefaultSubobject<USpringArmComponent>(TEXT("ShoulderBoom"));Boom->SetupAttachment(RootComponent);
    Boom->TargetArmLength=360;Boom->SocketOffset=FVector(0,55,65);Boom->bUsePawnControlRotation=true;Boom->bEnableCameraLag=true;Boom->CameraLagSpeed=12;
    Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("AdventureCamera"));Camera->SetupAttachment(Boom);Camera->FieldOfView=80;
    WeaponMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Weapon"));WeaponMesh->SetupAttachment(GetMesh(),TEXT("HandGrip_R"));
    WeaponMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Weapons/Pistol/Meshes/SM_Pistol.SM_Pistol")));
    WeaponMesh->SetRelativeLocation(FVector(0,0,0));WeaponMesh->SetRelativeRotation(FRotator::ZeroRotator);WeaponMesh->SetRelativeScale3D(FVector(1));WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void AExplorerCharacter::BeginPlay() {
    Super::BeginPlay();
    if(!FParse::Param(FCommandLine::Get(),TEXT("AdventureSmokeTest"))) {
        if(auto* S=Cast<UExpeditionSave>(UGameplayStatics::LoadGameFromSlot(TEXT("LostExpedition_Checkpoint"),0))) {
            Checkpoint=S->Checkpoint;Collected=S->Collected;bHasKey=S->bHasKey;bGateOpen=S->bGateOpen;Relics=S->Relics;
            Medkits=S->Medkits;Grenades=S->Grenades;
            Magazine[0]=S->PistolMagazine;Magazine[1]=S->RifleMagazine;Reserve[0]=S->PistolReserve;Reserve[1]=S->RifleReserve;
            SetActorLocation(Checkpoint,false,nullptr,ETeleportType::TeleportPhysics);
        }
    }
    for(TActorIterator<AExpeditionLoot> It(GetWorld());It;++It) if(Collected.Contains(It->ItemId)||(It->Kind==ELootKind::Gate&&bGateOpen)) {It->bUsed=true;It->SetActorHiddenInGame(true);It->SetActorEnableCollision(false);}
    if(auto* PC=Cast<APlayerController>(GetController())){PC->SetControlRotation(FRotator(-8,0,0));PC->SetInputMode(FInputModeGameOnly());PC->bShowMouseCursor=false;}
    Notify(TEXT("CLIFF SANCTUARY  /  Follow the pale limestone ledges"));
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
    if(Controller&&Traversal==ETraversalState::Walking&&!bJournal&&!bCompleted) AddMovementInput(FRotator(0,Controller->GetControlRotation().Yaw,0).Vector(),V);
}
void AExplorerCharacter::Right(float V) {
    RightInput=V;
    if(Controller&&Traversal==ETraversalState::Walking&&!bJournal&&!bCompleted) AddMovementInput(FRotationMatrix(FRotator(0,Controller->GetControlRotation().Yaw,0)).GetUnitAxis(EAxis::Y),V);
}
void AExplorerCharacter::Yaw(float V){if(!bJournal)AddControllerYawInput(V*.8f);}
void AExplorerCharacter::Pitch(float V){if(!bJournal)AddControllerPitchInput(V*.65f);}
void AExplorerCharacter::SprintStart(){bSprint=true;}void AExplorerCharacter::SprintStop(){bSprint=false;}
void AExplorerCharacter::AimStart(){bAim=true;}void AExplorerCharacter::AimStop(){bAim=false;}
void AExplorerCharacter::Journal(){bJournal=!bJournal;if(bJournal){StopFire();GetCharacterMovement()->StopMovementImmediately();}}
void AExplorerCharacter::Notify(const FString& T){Notice=T;NoticeRemaining=4;}
void AExplorerCharacter::Tick(float DT) {
    Super::Tick(DT);ShotCooldown=FMath::Max(0.f,ShotCooldown-DT);NoticeRemaining-=DT;DamageFlash=FMath::Max(0.f,DamageFlash-DT);Invulnerability-=DT;LedgeCooldown-=DT;
    if(GetActorLocation().Z<-1000){Respawn();return;}
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
    if(Traversal==ETraversalState::Mantling) {
        MantleTime+=DT;float T=FMath::Clamp(MantleTime/.65f,0.f,1.f);
        FVector Above=MantleStart;Above.Z=MantleEnd.Z+15;
        FVector P=T<.55f?FMath::Lerp(MantleStart,Above,FMath::SmoothStep(0.f,.55f,T)):FMath::Lerp(Above,MantleEnd,FMath::SmoothStep(.55f,1.f,T));
        FHitResult Hit;SetActorLocation(P,true,&Hit);
        if(Hit.bBlockingHit){Drop();Notify(TEXT("Climb blocked"));}
        else if(T>=1){Traversal=ETraversalState::Walking;GetCharacterMovement()->SetMovementMode(MOVE_Walking);LedgeCooldown=.4;}
    } else if(Traversal==ETraversalState::Hanging) {
        if(FMath::Abs(RightInput)>.1f&&!bJournal) {
            FVector Tangent=FVector::CrossProduct(FVector::UpVector,-WallNormal);
            FVector E,N,At=GetActorLocation()+Tangent*RightInput*DT*145;
            if(TryLedge(At,-WallNormal,E,N)&&FMath::Abs(E.Z-Ledge.Z)<45&&FVector::DotProduct(N,WallNormal)>.8f){Ledge=E;WallNormal=N;SetActorLocation(Ledge+WallNormal*48-FVector(0,0,100),true);}
        }
    } else if(GetCharacterMovement()->IsFalling()&&GetVelocity().Z<100&&LedgeCooldown<=0&&!bJournal) BeginHang();
    RefreshLoot();
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
    if(Traversal!=ETraversalState::Hanging)return false;
    MantleEnd=Ledge-WallNormal*85+FVector(0,0,99);
    FCollisionQueryParams Params;Params.AddIgnoredActor(this);
    if(GetWorld()->OverlapBlockingTestByChannel(MantleEnd,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(38,94),Params)){Notify(TEXT("No room above this ledge"));return false;}
    MantleStart=GetActorLocation();MantleTime=0;Traversal=ETraversalState::Mantling;return true;
}
void AExplorerCharacter::JumpOrClimb() {
    if(bJournal||bCompleted)return;
    if(Traversal==ETraversalState::Hanging){BeginMantle();return;}
    if(Traversal==ETraversalState::Mantling)return;
    if(BeginHang()){BeginMantle();return;}Jump();
}
void AExplorerCharacter::Drop() {
    if(Traversal==ETraversalState::Walking)return;
    Traversal=ETraversalState::Walking;GetCharacterMovement()->SetMovementMode(MOVE_Falling);LedgeCooldown=.7;
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
        case ELootKind::Gate:if(!bHasKey){Notify(TEXT("Find the sanctuary key in the courtyard"));return false;}bGateOpen=true;break;
        case ELootKind::Exit:if(!bGateOpen||Relics<3){Notify(TEXT("Collect all 3 relics and open the sanctuary gate"));return false;}bCompleted=true;StopFire();Notify(TEXT("EXPEDITION COMPLETE"));return true;
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
    if(!UGameplayStatics::SaveGameToSlot(S,TEXT("LostExpedition_Checkpoint"),0))Notify(TEXT("Checkpoint could not be saved"));
}
void AExplorerCharacter::Respawn() {
    Drop();GetCharacterMovement()->StopMovementImmediately();SetActorLocation(Checkpoint,false,nullptr,ETeleportType::TeleportPhysics);
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);Health=100;Invulnerability=3;bReloading=false;bFiring=false;
    if(auto* PC=Cast<APlayerController>(GetController()))PC->SetControlRotation(FRotator(-8,0,0));Notify(TEXT("Returned to checkpoint"));
}
void AExplorerCharacter::FreshStart() {
    UGameplayStatics::DeleteGameInSlot(TEXT("LostExpedition_Checkpoint"),0);
    UGameplayStatics::OpenLevel(this,FName(TEXT("CliffSanctuary")));
}
