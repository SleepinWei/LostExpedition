#pragma once
#include "CoreMinimal.h"

// A persistent pre-contact pose. State changes update the goal, never the stored
// pose/velocity. Contact IK runs afterwards and is deliberately not fed back.
struct FExplorerPoseSpring {
    TArray<FTransform> Pose;
    TArray<FVector> Linear,Angular;
    void Reset(){Pose.Reset();Linear.Reset();Angular.Reset();}
    void Seed(const TArray<FTransform>& In){Pose=In;Linear.Init(FVector::ZeroVector,In.Num());Angular=Linear;}
    static FVector RotationVector(FQuat Q){Q.Normalize();if(Q.W<0)Q=Q*-1;FVector Axis;float Angle;Q.ToAxisAndAngle(Axis,Angle);return Axis*Angle;}
    static FQuat Rotation(const FVector& V){const float A=V.Size();return A<SMALL_NUMBER?FQuat::Identity:FQuat(V/A,A);}
    static FVector Step(const FVector& Error,FVector& Velocity,float DT,float Frequency) {
        const FVector J=Velocity+Error*Frequency;
        const float Decay=FMath::Exp(-Frequency*DT);
        Velocity=(Velocity-J*(Frequency*DT))*Decay;
        return (Error+J*DT)*Decay;
    }
    void Evaluate(TArray<FTransform>& Goal,float DT,float Frequency=24) {
        if(Pose.Num()!=Goal.Num()){Seed(Goal);return;}
        DT=FMath::Clamp(DT,0.f,.1f);
        for(int32 I=0;I<Goal.Num();I++) {
            FTransform Result=Goal[I];
            Result.AddToTranslation(Step(Pose[I].GetTranslation()-Goal[I].GetTranslation(),Linear[I],DT,Frequency));
            const FVector Error=RotationVector(Pose[I].GetRotation()*Goal[I].GetRotation().Inverse());
            Result.SetRotation(Rotation(Step(Error,Angular[I],DT,Frequency))*Goal[I].GetRotation());
            Result.NormalizeRotation();Pose[I]=Result;Goal[I]=Result;
        }
    }
};
