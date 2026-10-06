#pragma once
#include "CoreMinimal.h"

namespace IslandTerrain {
inline float Smooth(float T){T=FMath::Clamp(T,0.f,1.f);return T*T*(3-2*T);}
inline float Radius(float X,float Y){return FMath::Sqrt(FMath::Square(X/10500.f)+FMath::Square(Y/8500.f));}
inline float Shore(float Angle){return 1+.055f*FMath::Sin(3*Angle+.4f)+.035f*FMath::Sin(5*Angle)+.018f*FMath::Sin(9*Angle);}
inline float TrailY(float X){float T=FMath::Clamp((X+6100)/7100.f,0.f,1.f);return -2600+1500*Smooth(T)+250*FMath::Sin(T*2*PI);}
inline float Height(float X,float Y){
    const float R=Radius(X,Y),Edge=Shore(FMath::Atan2(Y/8500.f,X/10500.f));
    float Low=FMath::Clamp((Edge-R)*1400.f,-1100.f,300.f);
    const float Q=FMath::Sqrt(FMath::Square((X-1400)/4300.f)+FMath::Square((Y-700)/3400.f));
    float Plateau=Smooth((1.13f-Q)/.37f);
    float H=Low+1900*Plateau;
    float Rough=FMath::Sin(X*.0013f)*FMath::Cos(Y*.0016f)*38+FMath::Sin(X*.0031f+Y*.0022f)*16;
    H+=Rough*Smooth((Edge-R)*8)*(1-Smooth((.7f-Q)/.2f));
    if(X>-6700&&X<1550){
        float T=Smooth((X+6100)/7100.f);
        float Blend=(1-Smooth((FMath::Abs(Y-TrailY(X))-360)/440))*Smooth((X+6700)/600)*Smooth((1550-X)/550);
        H=FMath::Lerp(H,300+1900*T,Blend);
    }
    return H;
}
inline FVector Normal(float X,float Y){return FVector(Height(X-30,Y)-Height(X+30,Y),Height(X,Y-30)-Height(X,Y+30),60).GetSafeNormal();}
}
