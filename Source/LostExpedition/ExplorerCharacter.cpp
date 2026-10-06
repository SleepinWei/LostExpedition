#include "ExplorerCharacter.h"
#include "ExpeditionTower.h"
#include "Components/PoseableMeshComponent.h"
#include "ExplorerPoseComponent.h"
#include "Animation/AnimSequence.h"
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
    GetCharacterMovement()->MaxAcceleration=2400;GetCharacterMovement()->GroundFriction=6;GetCharacterMovement()->BrakingFrictionFactor=1;
    GetMesh()->SetRelativeLocation(FVector(0,0,-96));GetMesh()->SetRelativeRotation(FRotator(0,-90,0));
    GetMesh()->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")));
    GetMesh()->SetAnimInstanceClass(LoadClass<UAnimInstance>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C")));
    ClimbPose=CreateDefaultSubobject<UExplorerPoseComponent>(TEXT("CharacterActionPose"));ClimbPose->SetupAttachment(RootComponent);
    ClimbPose->SetSkinnedAssetAndUpdate(GetMesh()->GetSkinnedAsset());ClimbPose->SetRelativeTransform(GetMesh()->GetRelativeTransform());
    ClimbPose->SetCollisionEnabled(ECollisionEnabled::NoCollision);ClimbPose->SetVisibility(true);GetMesh()->SetVisibility(false);
    ClimbPose->AddTickPrerequisiteComponent(GetMesh());
    WeaponIdleAnimations={LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Pistol/MF_Pistol_Idle_ADS")),LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS"))};
    WeaponFireAnimations={LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Fire")),LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Fire"))};
    WeaponReloadAnimations={LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Reload")),LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Reload"))};
    WeaponEquipAnimations={LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Equip")),LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Equip"))};
    const TCHAR* Directions[]={TEXT("Fwd"),TEXT("Fwd_Right"),TEXT("Right"),TEXT("Bwd_Right"),TEXT("Bwd"),TEXT("Bwd_Left"),TEXT("Left"),TEXT("Fwd_Left")};
    for(const TCHAR* Type:{TEXT("Pistol"),TEXT("Rifle")})for(const TCHAR* Gait:{TEXT("Walk"),TEXT("Jog")})for(const TCHAR* Direction:Directions)
        ArmedLocomotionAnimations.Add(LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Characters/Mannequins/Anims/%s/%s/MF_%s_%s_%s"),Type,Gait,Type,Gait,Direction)));
    GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Boom=CreateDefaultSubobject<USpringArmComponent>(TEXT("ShoulderBoom"));Boom->SetupAttachment(RootComponent);
    Boom->TargetArmLength=360;Boom->SocketOffset=FVector(0,55,65);Boom->bUsePawnControlRotation=true;Boom->bEnableCameraLag=true;Boom->CameraLagSpeed=12;
    Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("AdventureCamera"));Camera->SetupAttachment(Boom);Camera->FieldOfView=80;
    WeaponMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Weapon"));WeaponMesh->SetupAttachment(ClimbPose,TEXT("HandGrip_R"));
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
void AExplorerCharacter::Journal(){bJournal=!bJournal;if(bJournal){StopFire();ClearTraversalInput();GetCharacterMovement()->StopMovementImmediately();}}
void AExplorerCharacter::Notify(const FString& T){Notice=T;NoticeRemaining=4;}
void AExplorerCharacter::Tick(float DT) {
    Super::Tick(DT);ShotCooldown=FMath::Max(0.f,ShotCooldown-DT);NoticeRemaining-=DT;DamageFlash=FMath::Max(0.f,DamageFlash-DT);Invulnerability-=DT;LedgeCooldown-=DT;GripCooldown-=DT;
    if(GetActorLocation().Z<-180){Respawn();return;}
    Camera->FieldOfView=FMath::FInterpTo(Camera->FieldOfView,bAim?58:80,DT,9);
    Boom->TargetArmLength=FMath::FInterpTo(Boom->TargetArmLength,bAim?200:360,DT,9);
    AnimationClock+=DT;GrabTime+=DT;
    const float Speed=GetVelocity().Size2D();
    LocomotionPhase=FMath::Fmod(LocomotionPhase+DT*Speed/FMath::Lerp(300.f,600.f,FMath::SmoothStep(240.f,430.f,Speed)),1.f);
    if(FireAnimationTime>=0){FireAnimationTime+=DT;if(!WeaponFireAnimations[Weapon]||FireAnimationTime>WeaponFireAnimations[Weapon]->GetPlayLength()/(Weapon==0?1.5f:2.f))FireAnimationTime=-1;}
    FireBlendTime+=DT;if(PreviousFireAnimationTime>=0){PreviousFireAnimationTime+=DT;if(FireBlendTime>=.055f)PreviousFireAnimationTime=-1;}
    if(EquipAnimationTime>=0){EquipAnimationTime+=DT;if(!WeaponEquipAnimations[Weapon]||EquipAnimationTime>WeaponEquipAnimations[Weapon]->GetPlayLength())EquipAnimationTime=-1;}
    const bool Armed=Traversal==ETraversalState::Walking&&(bAim||FireAnimationTime>=0||bReloading||EquipAnimationTime>=0)&&!bCompleted;
    ArmAnimationAlpha=FMath::Lerp(ArmAnimationAlpha,Armed?1.f:0.f,1-FMath::Exp(-18*DT));
    bUseControllerRotationYaw=false;
    GetCharacterMovement()->bUseControllerDesiredRotation=Armed;
    GetCharacterMovement()->bOrientRotationToMovement=!Armed;
    GetCharacterMovement()->RotationRate=FRotator(0,Armed?720:540,0);
    GetCharacterMovement()->MaxWalkSpeed=FMath::Lerp(GetCharacterMovement()->MaxWalkSpeed,bAim?240.f:(bSprint?650.f:430.f),1-FMath::Exp(-14*DT));
    WeaponMesh->SetVisibility(Traversal==ETraversalState::Walking&&!bCompleted);
    if(bReloading) {
        ReloadRemaining-=DT;
        if(ReloadRemaining<=0){int32 N=FMath::Min(Capacity[Weapon]-Magazine[Weapon],Reserve[Weapon]);Magazine[Weapon]+=N;Reserve[Weapon]-=N;bReloading=false;}
    }
    if(bFiring&&Weapon==1)FireShot();
    UpdateTraversal(DT);
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
    if(BeginWallGrip())return true;
    FVector Direction=GetActorForwardVector();Direction.Z=0;
    if(!TryLedge(GetActorLocation(),Direction,Ledge,WallNormal))return false;
    FVector Target=Ledge+WallNormal*48-FVector(0,0,100);
    if(GetCharacterMovement()->IsMovingOnGround())Target.Z=FMath::Max(Target.Z,GetActorLocation().Z);
    FCollisionQueryParams Params;Params.AddIgnoredActor(this);
    if(GetWorld()->OverlapBlockingTestByChannel(Target,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(38,94),Params))return false;
    BeginGrabAnimation();Traversal=ETraversalState::Hanging;GetCharacterMovement()->StopMovementImmediately();GetCharacterMovement()->SetMovementMode(MOVE_Flying);
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
    bBufferedMantle=false;MantleStart=GetActorLocation();MantleTime=0;Traversal=ETraversalState::Mantling;return true;
}
void AExplorerCharacter::JumpOrClimb() {
    if(bJournal||bCompleted)return;
    if(Traversal==ETraversalState::Clinging){if(CurrentGrip==ExpeditionTower::Steps-1)BeginMantle();else MoveWallGrip(0,1);return;}
    if(Traversal==ETraversalState::Reaching){if(TargetGrip==ExpeditionTower::Steps-1&&!bEnteringFromRoof)bBufferedMantle=true;else {BufferedTraversalInput=FVector2D(0,1);TraversalBufferRemaining=FMath::Max(.18f,ReachDuration-ReachTime+.18f);}return;}
    if(Traversal==ETraversalState::Hanging){BeginMantle();return;}
    if(Traversal==ETraversalState::Mantling)return;
    if(BeginHang()){BeginMantle();return;}Jump();
}
void AExplorerCharacter::Drop() {
    if(Traversal==ETraversalState::Walking)return;
    ClearTraversalInput();TraversalVelocity=FVector::ZeroVector;CurrentGrip=-1;bEnteringFromRoof=false;ArmAnimationAlpha=0;FireAnimationTime=-1;Traversal=ETraversalState::Walking;GetCharacterMovement()->SetMovementMode(MOVE_Falling);LedgeCooldown=.7;UpdateClimbPose();
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
    if(Traversal==ETraversalState::Reaching){JumpOrClimb();return;}
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
    if(Traversal!=ETraversalState::Walking||bJournal||bCompleted)return;Weapon=I;bReloading=false;bFiring=false;FireAnimationTime=-1;EquipAnimationTime=0;
    WeaponMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,I==0?TEXT("/Game/Weapons/Pistol/Meshes/SM_Pistol.SM_Pistol"):TEXT("/Game/Weapons/Rifle/Meshes/SM_Rifle.SM_Rifle")));
    WeaponMesh->SetRelativeScale3D(FVector(1));
}
void AExplorerCharacter::Pistol(){Equip(0);}void AExplorerCharacter::Rifle(){Equip(1);}
void AExplorerCharacter::Reload() {
    if(bReloading||Magazine[Weapon]>=Capacity[Weapon]||Reserve[Weapon]<=0||Traversal!=ETraversalState::Walking||bJournal||bCompleted)return;
    bReloading=true;StopFire();EquipAnimationTime=-1;FireAnimationTime=-1;ReloadDuration=Weapon==0?1.2f:1.8f;ReloadRemaining=ReloadDuration;
}
void AExplorerCharacter::StartFire(){bFiring=true;FireShot();}void AExplorerCharacter::StopFire(){bFiring=false;}
void AExplorerCharacter::FireShot() {
    if(ShotCooldown>0||bReloading||Traversal!=ETraversalState::Walking||bJournal||bCompleted)return;
    if(Magazine[Weapon]<=0){Reload();return;}
    Magazine[Weapon]--;ShotCooldown=Weapon==0?.24f:.105f;PreviousFireAnimationTime=FireAnimationTime;FireBlendTime=0;FireAnimationTime=0;EquipAnimationTime=-1;
    UpdateClimbPose();
    FVector Start=Camera->GetComponentLocation(),D=Camera->GetForwardVector();
    float Spread=bAim?.0025f:.016f;D=FMath::VRandCone(D,Spread);
    FHitResult AimHit,Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(Weapon),true,this);
    FVector End=Start+D*12000;
    if(GetWorld()->LineTraceSingleByChannel(AimHit,Start,End,ECC_Visibility,Params))End=AimHit.ImpactPoint;
    FVector Muzzle=(ClimbPose->IsVisible()?ClimbPose->GetSocketLocation(TEXT("hand_r")):GetMesh()->GetSocketLocation(TEXT("hand_r")))+D*38;
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
    Drop();ClearTraversalInput();EquipAnimationTime=-1;ArmAnimationAlpha=0;FireAnimationTime=-1;CastChecked<UExplorerPoseComponent>(ClimbPose)->ResetTransition();UpdateClimbPose();GetCharacterMovement()->StopMovementImmediately();SetActorLocation(Checkpoint,false,nullptr,ETeleportType::TeleportPhysics);
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
        BeginGrabAnimation();CurrentGrip=TargetGrip=ExpeditionTower::Steps-1;Ledge=RoofGrip;WallNormal=ExpeditionTower::WallNormal;
        ClearTraversalInput();ReachDuration=RoofEntryDuration;TraversalVelocity=FVector::ZeroVector;ReachStart=At;ReachTime=0;bEnteringFromRoof=true;Traversal=ETraversalState::Reaching;
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
    ClearTraversalInput();TraversalVelocity=FVector::ZeroVector;BeginGrabAnimation();CurrentGrip=TargetGrip=Best;WallNormal=ExpeditionTower::WallNormal;Ledge=ExpeditionTower::Grip(Best);
    GetCharacterMovement()->StopMovementImmediately();GetCharacterMovement()->SetMovementMode(MOVE_Flying);
    SetActorLocation(Target);SetActorRotation((-WallNormal).Rotation());Traversal=ETraversalState::Clinging;
    StopFire();bReloading=false;GripCooldown=.08f;UpdateClimbPose();return true;
}
void AExplorerCharacter::ClearTraversalInput() {
    BufferedTraversalInput=PreviousTraversalInput=FVector2D::ZeroVector;TraversalBufferRemaining=0;QueuedGrip=-1;bBufferedMantle=false;
    ForwardInput=RightInput=0;
}
int32 AExplorerCharacter::FindGrip(int32 From,const FVector2D& Value) const {
    const FVector2D Input=Value.GetSafeNormal();if(Input.IsNearlyZero()||From<0)return -1;
    int32 Candidate=-1;float Score=.4f;
    for(int32 Index:{From-1,From+1}) {
        if(Index<0||Index>=ExpeditionTower::Steps)continue;
        const FVector Delta=ExpeditionTower::Grip(Index)-ExpeditionTower::Grip(From);
        const float Alignment=FVector2D::DotProduct(Input,FVector2D(Delta.Y,Delta.Z).GetSafeNormal());
        if(Delta.Size()>260||Alignment<=Score)continue;
        FCollisionQueryParams Query;Query.AddIgnoredActor(this);FHitResult Hit;
        if(GetWorld()->SweepSingleByChannel(Hit,ExpeditionTower::HangPosition(From),ExpeditionTower::HangPosition(Index),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(38,94),Query))continue;
        Score=Alignment;Candidate=Index;
    }
    return Candidate;
}
bool AExplorerCharacter::StartGripTransfer(int32 Candidate) {
    if(Candidate<0)return false;
    // Recheck the actual body path; queued targets can become obstructed.
    FCollisionQueryParams Query;Query.AddIgnoredActor(this);FHitResult Hit;
    if(GetWorld()->SweepSingleByChannel(Hit,GetActorLocation(),ExpeditionTower::HangPosition(Candidate),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(38,94),Query))return false;
    const FVector Delta=ExpeditionTower::Grip(Candidate)-ExpeditionTower::Grip(CurrentGrip);
    bLeadRight=FMath::Abs(Delta.Y)>100?Delta.Y>0:!bLeadRight;
    ReachDuration=FMath::Clamp(Delta.Size()/240.f,.48f,GripTransferDuration);
    ReachStartVelocity=TraversalVelocity.GetClampedToMaxSize(170);
    ReachCurveStartTime=0;ReachCurveNextGrip=-2;ReachEndVelocity=FVector::ZeroVector;
    bEnteringFromRoof=false;TargetGrip=Candidate;ReachStart=GetActorLocation();ReachTime=0;Traversal=ETraversalState::Reaching;
    BufferedTraversalInput=FVector2D::ZeroVector;TraversalBufferRemaining=0;QueuedGrip=-1;return true;
}
bool AExplorerCharacter::MoveWallGrip(float Horizontal,float Vertical) {
    if(bJournal)return false;
    if(Traversal==ETraversalState::Reaching) {
        BufferedTraversalInput=FVector2D(Horizontal,Vertical);TraversalBufferRemaining=FMath::Max(.18f,ReachDuration-ReachTime+.18f);return false;
    }
    if(Traversal!=ETraversalState::Clinging||GripCooldown>0)return false;
    return StartGripTransfer(FindGrip(CurrentGrip,FVector2D(Horizontal,Vertical)));
}
void AExplorerCharacter::UpdateTraversal(float DT) {
    const bool WasMantling=Traversal==ETraversalState::Mantling;
    const FVector2D Input=bJournal?FVector2D::ZeroVector:FVector2D(RightInput,ForwardInput);
    if(Traversal==ETraversalState::Reaching&&!Input.IsNearlyZero(.15f)&&!Input.Equals(PreviousTraversalInput,.1f)) {
        BufferedTraversalInput=Input;TraversalBufferRemaining=FMath::Max(.18f,ReachDuration-ReachTime+.18f);
        if(Input.Y<-.15f)bBufferedMantle=false;
    }
    PreviousTraversalInput=Input;
    if(bJournal){BufferedTraversalInput=FVector2D::ZeroVector;TraversalBufferRemaining=0;}
    FVector2D Command=!Input.IsNearlyZero(.15f)?Input:(TraversalBufferRemaining>0?BufferedTraversalInput:FVector2D::ZeroVector);
    if(Traversal==ETraversalState::Clinging&&GripCooldown<=0&&!bJournal)StartGripTransfer(FindGrip(CurrentGrip,Command));
    // Consume the remaining timestep at contact boundaries. Chaining has no cooldown
    // or extra frame of idle, including at 30 Hz and when reversing direction.
    float Remaining=DT;
    for(int32 Boundary=0;Boundary<4&&Remaining>SMALL_NUMBER&&Traversal==ETraversalState::Reaching;Boundary++) {
        QueuedGrip=bEnteringFromRoof?-1:FindGrip(TargetGrip,Command);
        const FVector End=ExpeditionTower::HangPosition(TargetGrip);
        FVector EndVelocity=FVector::ZeroVector;
        if(QueuedGrip>=0) {
            const FVector Next=ExpeditionTower::HangPosition(QueuedGrip)-End;
            const FVector Direction=ExpeditionTower::HangPosition(TargetGrip)-ExpeditionTower::HangPosition(CurrentGrip);
            if(FVector::DotProduct(Next.GetSafeNormal(),Direction.GetSafeNormal())>.3f)EndVelocity=(Next.GetSafeNormal()+Direction.GetSafeNormal()).GetSafeNormal()*150;
        }
        if(!bEnteringFromRoof&&QueuedGrip!=ReachCurveNextGrip) {
            // Replan the remaining curve from the current position and analytic
            // velocity when the player releases or changes direction. Changing
            // only the final tangent would otherwise move the body instantly.
            ReachCurveNextGrip=QueuedGrip;ReachCurveStartTime=ReachTime;ReachStart=GetActorLocation();ReachStartVelocity=TraversalVelocity;ReachEndVelocity=EndVelocity;
        }
        const float Step=FMath::Min(Remaining,FMath::Max(0.f,ReachDuration-ReachTime));Remaining-=Step;ReachTime+=Step;
        const float T=FMath::Clamp(ReachTime/ReachDuration,0.f,1.f);
        const float Span=FMath::Max(SMALL_NUMBER,ReachDuration-ReachCurveStartTime),CurveTime=FMath::Clamp((ReachTime-ReachCurveStartTime)/Span,0.f,1.f);
        FVector Target=FMath::CubicInterp(ReachStart,ReachStartVelocity*Span,End,ReachEndVelocity*Span,CurveTime);
        if(bEnteringFromRoof){FVector Above=End;Above.Z=ExpeditionTower::SummitZ()+110;Target=T<.45f?FMath::Lerp(ReachStart,Above,FMath::SmoothStep(0.f,.45f,T)):FMath::Lerp(Above,End,FMath::SmoothStep(.45f,1.f,T));}
        const FVector Old=GetActorLocation();FHitResult Hit;SetActorLocation(Target,true,&Hit);
        TraversalVelocity=bEnteringFromRoof?(Step>SMALL_NUMBER?(GetActorLocation()-Old)/Step:FVector::ZeroVector):FMath::CubicInterpDerivative(ReachStart,ReachStartVelocity*Span,End,ReachEndVelocity*Span,CurveTime)/Span;
        Ledge=FMath::Lerp(ExpeditionTower::Grip(CurrentGrip),ExpeditionTower::Grip(TargetGrip),FMath::SmoothStep(0.f,1.f,T));
        if(Hit.bBlockingHit){Drop();Notify(TEXT("No clear reach to that handhold"));break;}
        if(T>=1) {
            CurrentGrip=TargetGrip;bEnteringFromRoof=false;Traversal=ETraversalState::Clinging;GripCooldown=0;TraversalVelocity=EndVelocity;
            if(bBufferedMantle){bBufferedMantle=false;if(BeginMantle())break;}
            if(!StartGripTransfer(QueuedGrip)){TraversalVelocity=FVector::ZeroVector;break;}
            Command=!Input.IsNearlyZero(.15f)?Input:FVector2D::ZeroVector;
        }
    }
    TraversalBufferRemaining=FMath::Max(0.f,TraversalBufferRemaining-DT);
    if(Traversal==ETraversalState::Mantling) {
        MantleTime+=WasMantling?DT:Remaining;const float T=FMath::Clamp(MantleTime/MantleDuration,0.f,1.f);
        FVector Above=MantleStart;Above.Z=MantleEnd.Z+15;
        const FVector P=T<.55f?FMath::Lerp(MantleStart,Above,FMath::SmoothStep(0.f,.55f,T)):FMath::Lerp(Above,MantleEnd,FMath::SmoothStep(.55f,1.f,T));
        FHitResult Hit;SetActorLocation(P,true,&Hit);
        if(Hit.bBlockingHit){Drop();Notify(TEXT("Climb blocked"));}
        else if(T>=1){ClearTraversalInput();CurrentGrip=-1;Traversal=ETraversalState::Walking;GetCharacterMovement()->SetMovementMode(MOVE_Walking);LedgeCooldown=.4f;}
    } else if(Traversal==ETraversalState::Hanging) {
        if(FMath::Abs(RightInput)>.1f&&!bJournal) {
            const FVector Tangent=FVector::CrossProduct(FVector::UpVector,-WallNormal);
            FVector E,N,At=GetActorLocation()+Tangent*RightInput*DT*145;
            if(TryLedge(At,-WallNormal,E,N)&&FMath::Abs(E.Z-Ledge.Z)<45&&FVector::DotProduct(N,WallNormal)>.8f){Ledge=E;WallNormal=N;SetActorLocation(Ledge+WallNormal*48-FVector(0,0,100),true);}
        }
    } else if(Traversal==ETraversalState::Walking&&GetCharacterMovement()->IsFalling()&&GetVelocity().Z<100&&LedgeCooldown<=0&&!bJournal)BeginHang();
}
