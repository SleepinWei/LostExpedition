#pragma once
#include "CoreMinimal.h"
#include "Components/PoseableMeshComponent.h"
#include "ExplorerPoseComponent.generated.h"

// Evaluate the presentation pose after the locomotion mesh has updated its bones.
UCLASS()
class LOSTEXPEDITION_API UExplorerPoseComponent : public UPoseableMeshComponent {
    GENERATED_BODY()
public:
    UExplorerPoseComponent();
    virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction) override;
};
