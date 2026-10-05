#pragma once
#include "CoreMinimal.h"

// Centimetres. Each terrace is wide enough to turn before the next grab.
namespace ExpeditionTower {
inline const FVector Base(7330,-3000,620);
constexpr int32 Steps=21;
constexpr float Rise=200.f;
inline FVector Direction(int32 Step) {
    const FVector Directions[]={FVector(0,-1,0),FVector(1,0,0),FVector(0,1,0),FVector(-1,0,0)};
    return Directions[((Step-1)/4)%4];
}
inline FVector Terrace(int32 Step) {
    FVector P=Base+FVector(-800,800,0);
    for(int32 I=1;I<=Step;I++)P+=Direction(I)*400+FVector(0,0,Rise);
    return P;
}
inline float SummitZ(){return Base.Z+Steps*Rise;}
}
