#include "ExpeditionGameMode.h"
#include "ExplorerCharacter.h"
#include "ExpeditionActors.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "GameFramework/PlayerController.h"

AExpeditionGameMode::AExpeditionGameMode() {
    DefaultPawnClass=AExplorerCharacter::StaticClass();HUDClass=AExpeditionHUD::StaticClass();
}
void AExpeditionGameMode::BeginPlay() {
    Super::BeginPlay();
    if(FParse::Param(FCommandLine::Get(),TEXT("AdventureVisualReview"))) {
        FTimerHandle Review;
        GetWorldTimerManager().SetTimer(Review,FTimerDelegate::CreateWeakLambda(this,[this](){
            for(TActorIterator<AExpeditionGuard> It(GetWorld());It;++It)It->bTrainingTarget=true;
            if(TActorIterator<AExplorerCharacter> It(GetWorld());It) {
                It->SetActorLocation(FVector(3520,0,740));It->SetActorRotation(FRotator::ZeroRotator);
                if(auto* PC=Cast<APlayerController>(It->GetController()))PC->SetControlRotation(FRotator(5,0,0));
            }
            FTimerHandle Capture,Close;
            GetWorldTimerManager().SetTimer(Capture,FTimerDelegate::CreateWeakLambda(this,[](){
                FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("Docs/gameplay-preview.png")),true,false);
            }),6.f,false);
            GetWorldTimerManager().SetTimer(Close,FTimerDelegate::CreateWeakLambda(this,[](){FPlatformMisc::RequestExit(false);}),10.f,false);
        }),3.f,false);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("AdventureSmokeTest"))) {
        GetWorldTimerManager().SetTimer(TestTimer,this,&AExpeditionGameMode::RunTestWhenReady,1,false);
    }
}
void AExpeditionGameMode::RunTestWhenReady() {if(TActorIterator<AExplorerCharacter> It(GetWorld());It)It->RunSmokeTest();}
void AExpeditionHUD::DrawHUD() {
    Super::DrawHUD();if(!Canvas)return;
    auto* P=Cast<AExplorerCharacter>(GetOwningPawn());if(!P)return;
    float W=Canvas->SizeX,H=Canvas->SizeY,S=FMath::Clamp(W/1440.f,.7f,1.4f);
    const FLinearColor Gold(.95,.78,.43), White(.91,.94,.90), Dim(.55,.66,.63), Ink(.025,.06,.055,.84);
    auto Text=[&](const FString& T,float X,float Y,FLinearColor C,float Scale=1.f){DrawText(T,C,X,Y,GEngine->GetSmallFont(),Scale*S);};
    DrawRect(Ink,25*S,25*S,420*S,132*S);DrawRect(Gold,25*S,25*S,4*S,132*S);
    Text(TEXT("LOST EXPEDITION"),45*S,40*S,Gold,1.65);
    Text(TEXT("01  /  CLIFF SANCTUARY"),45*S,69*S,Dim,.95);
    FString Objective;
    if(!P->bHasKey)Objective=P->GetActorLocation().X<1600?TEXT("Climb the pale limestone ledges"):TEXT("Cross the bridge / find the courtyard key");
    else if(!P->bGateOpen)Objective=TEXT("Use the key at the sanctuary gate");
    else Objective=TEXT("Collect 3 relics / reach the exit beacon");
    Text(Objective,45*S,98*S,White,1.05);
    Text(FString::Printf(TEXT("RELICS  %d / 3     KEY  %s"),P->Relics,P->bHasKey?TEXT("FOUND"):TEXT("--")),45*S,125*S,Gold,.95);
    DrawRect(Ink,25*S,H-119*S,300*S,91*S);
    Text(TEXT("HEALTH"),42*S,H-107*S,Dim,.85);DrawRect(FLinearColor(.12,.17,.15),42*S,H-83*S,264*S,8*S);
    DrawRect(P->Health<30?FLinearColor(.85,.24,.14):Gold,42*S,H-83*S,264*S*P->Health/100,8*S);
    Text(FString::Printf(TEXT("[Q] MEDKIT %d     [G] GRENADE %d"),P->Medkits,P->Grenades),42*S,H-61*S,White,.95);
    DrawRect(Ink,W-310*S,H-119*S,285*S,91*S);
    Text(P->Weapon==0?TEXT("[1]  9MM PISTOL"):TEXT("[2]  ASSAULT RIFLE"),W-290*S,H-104*S,Gold,1.2);
    Text(P->bReloading?TEXT("RELOADING..."):FString::Printf(TEXT("%02d   /   %03d    [R] RELOAD"),P->Magazine[P->Weapon],P->Reserve[P->Weapon]),W-290*S,H-72*S,White,1.25);
    Text(TEXT("WASD move  /  SHIFT sprint  /  SPACE jump & climb  /  E interact  /  TAB journal"),W*.5f-345*S,H-28*S,Dim,.85);
    if(P->Traversal!=ETraversalState::Walking) {
        DrawRect(Ink,W*.5f-230*S,H*.68f,460*S,44*S);
        Text(P->Traversal==ETraversalState::Hanging?TEXT("A / D  shimmy    SPACE climb up    CTRL drop"):TEXT("CLIMBING"),W*.5f-210*S,H*.68f+14*S,Gold,1.05);
    } else if(P->Nearby.IsValid()&&!P->bJournal) {
        FString Prompt=TEXT("[E]  ")+P->Nearby->Prompt();float TW,TH;GetTextSize(Prompt,TW,TH,GEngine->GetSmallFont(),1.2*S);
        DrawRect(Ink,W/2-TW/2-18,H*.68f,TW+36,40*S);Text(Prompt,W/2-TW/2,H*.68f+12*S,Gold,1.2);
    }
    if(P->NoticeRemaining>0){float TW,TH;GetTextSize(P->Notice,TW,TH,GEngine->GetSmallFont(),1.1*S);DrawRect(Ink,W/2-TW/2-16,180*S,TW+32,36*S);Text(P->Notice,W/2-TW/2,190*S,White,1.1);}
    if(P->bAim||P->bFiring){float X=W/2,Y=H/2;DrawLine(X-12,Y,X-4,Y,Gold,1.6);DrawLine(X+4,Y,X+12,Y,Gold,1.6);DrawLine(X,Y-12,X,Y-4,Gold,1.6);DrawLine(X,Y+4,X,Y+12,Gold,1.6);}
    if(P->DamageFlash>0){DrawRect(FLinearColor(.7,.04,.01,P->DamageFlash*.7),0,0,W,10);DrawRect(FLinearColor(.7,.04,.01,P->DamageFlash*.7),0,H-10,W,10);}
    // Objective beacons are screen-space and remain legible above the ruins.
    for(TActorIterator<AExpeditionLoot> It(GetWorld());It;++It) {
        if(It->bUsed||It->Kind==ELootKind::Ammo||It->Kind==ELootKind::Medkit||It->Kind==ELootKind::Grenade)continue;
        FVector2D Screen;float D=FVector::Dist(P->GetActorLocation(),It->GetActorLocation());
        if(D>2800||!PlayerOwner->ProjectWorldLocationToScreen(It->GetActorLocation()+FVector(0,0,105),Screen)||Screen.X<0||Screen.X>W||Screen.Y<0||Screen.Y>H)continue;
        Text(It->Kind==ELootKind::Relic?TEXT("* RELIC"):It->Kind==ELootKind::Key?TEXT("* KEY"):It->Kind==ELootKind::Checkpoint?TEXT("+ CHECKPOINT"):It->Kind==ELootKind::Gate?TEXT("GATE"):TEXT("EXIT"),Screen.X,Screen.Y,Gold,.85);
    }
    if(P->bJournal||P->bCompleted) {
        DrawRect(FLinearColor(.01,.025,.022,.92),0,0,W,H);
        float X=W/2-340*S,Y=H/2-220*S;
        Text(P->bCompleted?TEXT("EXPEDITION COMPLETE"):TEXT("EXPLORER'S JOURNAL"),X,Y,Gold,2.4);
        Text(TEXT("CLIFF SANCTUARY  /  an island beyond the charts"),X,Y+55*S,Dim,1.1);
        TArray<FString> Lines=P->bCompleted?TArray<FString>{TEXT("You recovered all three relics and escaped the sanctuary."),TEXT("The lost expedition's trail lives on."),TEXT("F5  /  Start a fresh expedition")}:TArray<FString>{TEXT("01  Follow the pale ledges. E grabs; SPACE climbs up."),TEXT("02  Cross the suspension bridge. Save at the blue beacon."),TEXT("03  Clear the courtyard. Collect the key and two relics."),TEXT("04  Unlock the sanctuary. Recover the final relic."),TEXT("05  Find the exit beacon to finish the expedition."),TEXT("RMB aim / LMB fire / 1-2 weapons / R reload"),TEXT("Q medkit / G grenade / CTRL drop / F5 fresh start"),TEXT("TAB  /  Return to the expedition")};
        for(int I=0;I<Lines.Num();I++)Text(Lines[I],X,Y+(100+I*35)*S,White,1.15);
    }
}
