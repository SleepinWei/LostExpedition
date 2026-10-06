#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/SaveGame.h"
#include "ExplorerCharacter.generated.h"
class AExpeditionLoot;

UCLASS()
class LOSTEXPEDITION_API UExpeditionSave : public USaveGame {
    GENERATED_BODY()
public:
    UPROPERTY() FVector Checkpoint=FVector(-7200,-4500,300);
    UPROPERTY() TArray<FName> Collected;
    UPROPERTY() bool bHasKey=false;
    UPROPERTY() bool bGateOpen=false;
    UPROPERTY() int32 Relics=0;
    UPROPERTY() int32 Medkits=2;
    UPROPERTY() int32 Grenades=3;
    UPROPERTY() int32 PistolMagazine=12;
    UPROPERTY() int32 RifleMagazine=30;
    UPROPERTY() int32 PistolReserve=60;
    UPROPERTY() int32 RifleReserve=120;
};

UENUM(BlueprintType)
enum class ETraversalState : uint8 { Walking, Hanging, Mantling, Clinging, Reaching };

UCLASS()
class LOSTEXPEDITION_API AExplorerCharacter : public ACharacter {
    GENERATED_BODY()
public:
    AExplorerCharacter();
    virtual void BeginPlay() override;
    virtual void Tick(float DT) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    virtual float TakeDamage(float Damage, const FDamageEvent& Event, AController* Instigator, AActor* Causer) override;
    virtual void Landed(const FHitResult& Hit) override;
    void Forward(float Value); void Right(float Value); void Yaw(float Value); void Pitch(float Value);
    void JumpOrClimb(); void Drop(); void Interact(); void Heal(); void ThrowGrenade(); void Reload();
    void StartFire(); void StopFire(); void FireShot(); void AimStart(); void AimStop();
    void SprintStart(); void SprintStop(); void Pistol(); void Rifle(); void Journal(); void FreshStart();
    bool TryLedge(const FVector& At, const FVector& Direction, FVector& Edge, FVector& Normal) const;
    bool BeginWallGrip(); bool MoveWallGrip(float Horizontal,float Vertical); void UpdateClimbPose();
    static constexpr float GripTransferDuration=.76f, RoofEntryDuration=1.10f, MantleDuration=1.10f;
    int32 CurrentGrip=-1, TargetGrip=-1;
    float ReachTime=0, GripCooldown=0;
    bool bEnteringFromRoof=false;
    FVector ReachStart;
    bool bLeadRight=true;
    float GrabTime=1, AnimationClock=0, ArmAnimationAlpha=0, FireAnimationTime=-1;
    FVector GrabHands[2],GrabFeet[2],AnimatedHands[2],AnimatedFeet[2];
    UPROPERTY() TArray<TObjectPtr<class UAnimSequence>> WeaponIdleAnimations;
    UPROPERTY() TArray<TObjectPtr<class UAnimSequence>> WeaponFireAnimations;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UPoseableMeshComponent> ClimbPose;
    bool BeginHang(); bool BeginMantle(); void Respawn(); void SaveCheckpoint();
    bool ReceiveLoot(AExpeditionLoot* Loot);
    void Notify(const FString& Text);
    UPROPERTY(VisibleAnywhere) TObjectPtr<class USpringArmComponent> Boom;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UCameraComponent> Camera;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> WeaponMesh;
    UPROPERTY(BlueprintReadOnly) ETraversalState Traversal=ETraversalState::Walking;
    UPROPERTY(BlueprintReadOnly) float Health=100;
    UPROPERTY(BlueprintReadOnly) int32 Medkits=2;
    UPROPERTY(BlueprintReadOnly) int32 Grenades=3;
    UPROPERTY(BlueprintReadOnly) int32 Relics=0;
    UPROPERTY(BlueprintReadOnly) bool bHasKey=false;
    UPROPERTY(BlueprintReadOnly) bool bGateOpen=false;
    UPROPERTY(BlueprintReadOnly) bool bCompleted=false;
    bool bAim=false, bSprint=false, bFiring=false, bReloading=false, bJournal=false;
    int32 Weapon=0;
    int32 Magazine[2]={12,30};
    int32 Reserve[2]={60,120};
    const int32 Capacity[2]={12,30};
    float ShotCooldown=0, ReloadRemaining=0, DamageFlash=0, NoticeRemaining=0;
    float Invulnerability=0, LedgeCooldown=0;
    FString Notice;
    FVector Checkpoint=FVector(-7200,-4500,300);
    TArray<FName> Collected;
    TWeakObjectPtr<AExpeditionLoot> Nearby;
    FVector Ledge, WallNormal, MantleStart, MantleEnd;
    float MantleTime=0;
    void RunSmokeTest();
private:
    float ForwardInput=0, RightInput=0;
    void Equip(int32 Index);
    void RefreshLoot();
    void BeginGrabAnimation();
};
