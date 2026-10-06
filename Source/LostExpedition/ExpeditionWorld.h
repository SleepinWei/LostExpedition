#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ExpeditionWorld.generated.h"

UCLASS()
class LOSTEXPEDITION_API AExpeditionWorld : public AActor {
    GENERATED_BODY()
public:
    AExpeditionWorld();
    virtual void OnConstruction(const FTransform& Transform) override;
    UFUNCTION(CallInEditor, BlueprintCallable, Category="Expedition") void RebuildScene();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Expedition") int32 Seed=42;
    UFUNCTION(BlueprintCallable, Category="Island") float GroundHeight(float X,float Y) const;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UProceduralMeshComponent> Terrain;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UProceduralMeshComponent> Surf;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Pieces;
private:
    UStaticMeshComponent* Shape(const FString& Name, const FString& Mesh, FVector Pos, FVector Size, FLinearColor Color, FRotator Rot=FRotator::ZeroRotator, bool Collision=true, bool Climb=false);
    UStaticMeshComponent* Asset(const FString& Name, const TCHAR* Path, FVector Pos, float Height, FRotator Rot);
    void Palm(FVector P, float Scale, FRandomStream& R);
    void Arch(FVector P, float Yaw);
};
