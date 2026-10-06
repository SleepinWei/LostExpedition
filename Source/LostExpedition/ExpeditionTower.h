#pragma once
#include "CoreMinimal.h"

namespace ExpeditionTower {
inline const FVector Base(1600,900,2200);
inline const FVector WallNormal(-1,0,0);
constexpr int32 Steps=23;
constexpr float Height=2800.f;
inline FVector Grip(int32 Index){
    const float Side[Steps]={-260,-200,-280,-180,-80,-170,-90,20,130,230,260,140,40,-60,-150,-250,-200,-80,30,120,140,140,140};
    const float Up[Steps]={220,350,480,610,740,870,1000,1130,1130,1130,1260,1390,1520,1650,1780,1910,2040,2170,2300,2430,2560,2690,2800};
    Index=FMath::Clamp(Index,0,Steps-1);
    return Base+FVector(-545,Side[Index],Up[Index]);
}
inline FVector HangPosition(int32 Index){return Grip(Index)+WallNormal*48-FVector(0,0,95);}
inline float SummitZ(){return Base.Z+Height;}
inline FVector Start(){return Base+FVector(-780,-260,99);}
}
