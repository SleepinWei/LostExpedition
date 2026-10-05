#include "ExpeditionActors.h"
#include "ExplorerCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "EngineUtils.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"

AExpeditionLoot::AExpeditionLoot() {
    PrimaryActorTick.bCanEverTick=true;
    Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LootMesh")); RootComponent=Mesh;
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void AExpeditionLoot::OnConstruction(const FTransform& T) {
    Super::OnConstruction(T);
    FString Mat=TEXT("Ledge"); FVector Scale(.5,.5,.5);
    if(Kind==ELootKind::Gate) {Scale=FVector(.4,4.5,5.4); Mat=TEXT("Wood");Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Mesh->SetCollisionResponseToAllChannels(ECR_Block);}
    else {Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);}
    if(Kind==ELootKind::Relic) {Scale=FVector(.4,.4,.75);Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cone.Cone")));}
    if(Kind==ELootKind::Key) Scale=FVector(.5,.15,.15);
    if(Kind==ELootKind::Ammo) {Scale=FVector(.65,.4,.35);Mat=TEXT("Wood");}
    if(Kind==ELootKind::Medkit) {Scale=FVector(.5,.4,.18);Mat=TEXT("Supply");}
    if(Kind==ELootKind::Checkpoint||Kind==ELootKind::Exit) {Scale=FVector(.8,.8,.18);Mat=TEXT("Water");}
    Mesh->SetRelativeScale3D(Scale);
    Mesh->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,Kind==ELootKind::Gate?TEXT("/Game/Materials/M_ExpeditionWoodV2"):*(TEXT("/Game/Materials/MI_")+Mat)));
}
void AExpeditionLoot::RefreshVisuals() {OnConstruction(GetActorTransform());}
void AExpeditionLoot::BeginPlay() {
    Super::BeginPlay();Home=GetActorLocation();
    if(auto* P=Cast<AExplorerCharacter>(UGameplayStatics::GetPlayerCharacter(this,0))) {
        if(P->Collected.Contains(ItemId)||(Kind==ELootKind::Gate&&P->bGateOpen)){bUsed=true;SetActorHiddenInGame(true);SetActorEnableCollision(false);}
    }
}
void AExpeditionLoot::Tick(float DT) {
    Super::Tick(DT);if(bUsed)return;
    if(Kind!=ELootKind::Gate&&Kind!=ELootKind::Checkpoint&&Kind!=ELootKind::Exit) {
        SetActorLocation(Home+FVector(0,0,FMath::Sin(GetWorld()->TimeSeconds*2)*9));AddActorLocalRotation(FRotator(0,DT*35,0));
    }
}
FString AExpeditionLoot::Prompt() const {
    if(!DisplayName.IsEmpty())return DisplayName;
    switch(Kind) {
        case ELootKind::Ammo:return TEXT("Take ammunition");case ELootKind::Medkit:return TEXT("Take medkit");
        case ELootKind::Grenade:return TEXT("Take grenade");case ELootKind::Key:return TEXT("Take sanctuary key");
        case ELootKind::Relic:return TEXT("Collect ancient relic");case ELootKind::Checkpoint:return TEXT("Save checkpoint");
        case ELootKind::Gate:return TEXT("Unlock sanctuary gate");case ELootKind::Exit:return TEXT("Finish expedition");
    }return TEXT("Interact");
}
void AExpeditionLoot::Interact(AExplorerCharacter* P) {
    if(!P||bUsed)return;
    if(P->ReceiveLoot(this)) {
        if(Kind!=ELootKind::Checkpoint&&Kind!=ELootKind::Exit){bUsed=true;SetActorHiddenInGame(true);SetActorEnableCollision(false);}
    }
}
AExpeditionGuard::AExpeditionGuard() {
    PrimaryActorTick.bCanEverTick=true;
    Body=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("GuardBody"));RootComponent=Body;
    Body->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")));
    Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Body->SetCollisionObjectType(ECC_WorldDynamic);Body->SetCollisionResponseToAllChannels(ECR_Ignore);Body->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    Body->SetAnimation(LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle")));
}
void AExpeditionGuard::BeginPlay() {Super::BeginPlay();Home=GetActorLocation();Body->Play(true);}
void AExpeditionGuard::Tick(float DT) {
    Super::Tick(DT);if(bDead||bTrainingTarget)return;
    auto* P=Cast<AExplorerCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));if(!P)return;
    Cooldown-=DT;
    const FVector Eye=GetActorLocation()+FVector(0,0,145),Target=P->GetActorLocation()+FVector(0,0,35);
    FHitResult Hit;FCollisionQueryParams Params;Params.AddIgnoredActor(this);
    bool Visible=FVector::Dist(Eye,Target)<2100&&(!GetWorld()->LineTraceSingleByChannel(Hit,Eye,Target,ECC_Visibility,Params)||Hit.GetActor()==P);
    if(Visible&&!P->bCompleted) {
        FVector D=Target-Eye;D.Z=0;SetActorRotation(FRotator(0,D.Rotation().Yaw-90,0));
        if(Cooldown<=0) {
            Cooldown=1.15f;DrawDebugLine(GetWorld(),Eye,Target,FColor(255,110,60),false,.15,0,2);
            UGameplayStatics::ApplyDamage(P,8,nullptr,this,UDamageType::StaticClass());
        }
    } else {
        FVector Dest=Home+PatrolOffset*(.5f+.5f*FMath::Sin(GetWorld()->TimeSeconds*.35f));
        // Patrol remains on its authored terrace; no navmesh or runtime dependency is needed.
        SetActorLocation(FMath::VInterpConstantTo(GetActorLocation(),Dest,DT,90));
        FVector D=Dest-GetActorLocation();if(D.SizeSquared()>10)SetActorRotation(FRotator(0,D.Rotation().Yaw-90,0));
    }
}
float AExpeditionGuard::TakeDamage(float D,const FDamageEvent& E,AController* I,AActor* C) {
    if(bDead)return 0;const float Applied=FMath::Max(0.f,D);Health-=Applied;
    if(Health<=0){bDead=true;Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);SetActorRotation(GetActorRotation()+FRotator(75,0,0));SetLifeSpan(12);}
    return Applied;
}
AExpeditionGrenade::AExpeditionGrenade() {
    Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Grenade"));RootComponent=Mesh;
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));Mesh->SetRelativeScale3D(FVector(.16));
    Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Movement=CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Ballistics"));Movement->UpdatedComponent=Mesh;Movement->InitialSpeed=1300;Movement->MaxSpeed=1600;Movement->bShouldBounce=true;Movement->Bounciness=.35;Movement->ProjectileGravityScale=1;
}
void AExpeditionGrenade::BeginPlay() {
    Super::BeginPlay();if(GetOwner())Mesh->IgnoreActorWhenMoving(GetOwner(),true);
    Mesh->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/MI_Foliage")));
    GetWorldTimerManager().SetTimer(Fuse,this,&AExpeditionGrenade::Explode,2.2,false);
}
void AExpeditionGrenade::Explode() {
    const FVector Center=GetActorLocation();DrawDebugSphere(GetWorld(),Center,420,24,FColor(255,140,25),false,.8,0,5);
    for(TActorIterator<AExpeditionGuard> It(GetWorld());It;++It) {
        float Dist=FVector::Dist(Center,It->GetActorLocation()+FVector(0,0,80));if(Dist>420)continue;
        FHitResult Hit;FCollisionQueryParams Params;Params.AddIgnoredActor(this);
        if(!GetWorld()->LineTraceSingleByChannel(Hit,Center,It->GetActorLocation()+FVector(0,0,80),ECC_Visibility,Params)||Hit.GetActor()==*It)
            UGameplayStatics::ApplyDamage(*It,FMath::Lerp(160.f,55.f,Dist/420),GetInstigatorController(),this,UDamageType::StaticClass());
    }
    Destroy();
}
