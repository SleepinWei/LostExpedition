#pragma once
#include "CoreMinimal.h"

// One timeline drives the sampled performance, collision path and contact curves.
// Contacts release before flight and engage separately on arrival.
namespace ExplorerClimbMotion {
inline float Ease(float A,float B,float T) {
    T=FMath::Clamp((T-A)/(B-A),0.f,1.f);return T*T*T*(T*(T*6-15)+10);
}
inline float ReleaseEnd(bool Lead){return Lead?.30f:.34f;}
inline float CatchBegin(bool Lead){return Lead?.62f:.66f;}
inline float CatchEnd(bool Lead){return Lead?.94f:1.f;}
inline float Depart(float T,bool Lead){return 1-Ease(0.f,ReleaseEnd(Lead),T);}
inline float Arrive(float T,bool Lead){return Ease(CatchBegin(Lead),CatchEnd(Lead),T);}
// Cubic rotation blending spreads wrist rotation across the same contact window,
// lowering peak angular speed while retaining zero velocity at either endpoint.
inline float WristContact(float T,bool Lead) {
    return 1-FMath::SmoothStep(0.f,ReleaseEnd(Lead),T)+FMath::SmoothStep(CatchBegin(Lead),CatchEnd(Lead),T);
}
inline float FootDepart(float T){return 1-Ease(.04f,.38f,T);}
inline float FootArrive(float T,int32 Side){return Ease(Side==0?.62f:.64f,Side==0?.98f:1.f,T);}
inline FVector Travel(const FVector& Start,const FVector& End,const FVector& Normal,float T) {
    const float U=FMath::Clamp((T-.20f)/.80f,0.f,1.f);
    const float S=U*U*(3-2*U);
    const float Arc=16*U*U*(1-U)*(1-U);
    return FMath::Lerp(Start,End,S)+(FVector::UpVector*32+Normal*16)*Arc;
}
}
