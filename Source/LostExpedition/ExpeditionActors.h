#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ExpeditionActors.generated.h"
class AExplorerCharacter;

UENUM(BlueprintType)
enum class ELootKind : uint8 { Ammo, Medkit, Grenade, Key, Relic, Checkpoint, Gate, Exit };

UCLASS()
class LOSTEXPEDITION_API AExpeditionLoot : public AActor {
    GENERATED_BODY()
public:
    AExpeditionLoot();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DT) override;
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Loot") void RefreshVisuals();
    void Interact(AExplorerCharacter* Player);
    FString Prompt() const;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loot") ELootKind Kind=ELootKind::Ammo;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loot") FName ItemId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loot") FString DisplayName;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
    bool bUsed=false;
    FVector Home;
};

UCLASS()
class LOSTEXPEDITION_API AExpeditionGuard : public AActor {
    GENERATED_BODY()
public:
    AExpeditionGuard();
    virtual void BeginPlay() override;
    virtual void Tick(float DT) override;
    virtual float TakeDamage(float Damage, const FDamageEvent& Event, AController* Instigator, AActor* Causer) override;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> Body;
    UPROPERTY(EditAnywhere, Category="Guard") FVector PatrolOffset=FVector(0,500,0);
    UPROPERTY(EditAnywhere, Category="Guard") float Health=90;
    UPROPERTY(EditAnywhere, Category="Guard") bool bTrainingTarget=false;
    FVector Home;
    float Cooldown=2;
    bool bDead=false;
};

UCLASS()
class LOSTEXPEDITION_API AExpeditionGrenade : public AActor {
    GENERATED_BODY()
public:
    AExpeditionGrenade();
    virtual void BeginPlay() override;
    void Explode();
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UProjectileMovementComponent> Movement;
    FTimerHandle Fuse;
};
