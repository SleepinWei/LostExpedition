#include "ExplorerPoseComponent.h"
#include "ExplorerCharacter.h"

UExplorerPoseComponent::UExplorerPoseComponent() {
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.TickGroup=TG_PostPhysics;
}
void UExplorerPoseComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction) {
    Super::TickComponent(DeltaTime,TickType,TickFunction);
    if(auto* Character=Cast<AExplorerCharacter>(GetOwner()))Character->UpdateClimbPose();
}
