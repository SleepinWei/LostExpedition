#include "ExpeditionGameMode.h"
#include "ExplorerCharacter.h"
#include "ExpeditionActors.h"
#include "ExpeditionTower.h"
#include "IslandTerrain.h"
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
    if(FParse::Param(FCommandLine::Get(),TEXT("AnimationVisualReview"))||FParse::Param(FCommandLine::Get(),TEXT("MotionMatchingVisualReview"))) {
        GetWorldTimerManager().SetTimer(AnimationReviewTimer,this,&AExpeditionGameMode::StartAnimationReview,3.f,false);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("WatchtowerStart"))) {
        FTimerHandle Start;
        GetWorldTimerManager().SetTimer(Start,FTimerDelegate::CreateWeakLambda(this,[this](){
            if(TActorIterator<AExplorerCharacter> It(GetWorld());It) {
                It->SetActorLocation(ExpeditionTower::Start());It->SetActorRotation(FRotator::ZeroRotator);
                if(auto* PC=Cast<APlayerController>(It->GetController()))PC->SetControlRotation(FRotator(16,0,0));
                It->Notify(TEXT("RUINED TOWER / E grabs / W S climb / A D traverse / SPACE tops out"));
            }
        }),.5f,false);
    }
    const bool TowerReview=FParse::Param(FCommandLine::Get(),TEXT("WatchtowerVisualReview"));
    if(TowerReview||FParse::Param(FCommandLine::Get(),TEXT("AdventureVisualReview"))) {
        FTimerHandle Review;
        GetWorldTimerManager().SetTimer(Review,FTimerDelegate::CreateWeakLambda(this,[this,TowerReview](){
            for(TActorIterator<AExpeditionGuard> It(GetWorld());It;++It)It->bTrainingTarget=true;
            if(TActorIterator<AExplorerCharacter> It(GetWorld());It) {
                It->SetActorLocation(FVector(-7200,-4500,IslandTerrain::Height(-7200,-4500)+105));It->SetActorRotation(FRotator::ZeroRotator);
                if(auto* PC=Cast<APlayerController>(It->GetController()))PC->SetControlRotation(FRotator(8,28,0));
                if(TowerReview) {
                    It->SetActorLocation(ExpeditionTower::HangPosition(4)+ExpeditionTower::WallNormal*40);It->SetActorRotation(FRotator::ZeroRotator);
                    It->LedgeCooldown=0;It->BeginHang();
                    if(auto* PC=Cast<APlayerController>(It->GetController()))PC->SetControlRotation(FRotator(12,30,0));
                }
            }
            FTimerHandle Capture,Close;
            GetWorldTimerManager().SetTimer(Capture,FTimerDelegate::CreateWeakLambda(this,[TowerReview](){
                FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/(TowerReview?TEXT("Docs/tower-gameplay.png"):TEXT("Docs/gameplay-preview.png"))),true,false);
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
    Text(TEXT("01  /  PALM ISLAND"),45*S,69*S,Dim,.95);
    FString Objective;
    if(!P->bHasKey)Objective=P->GetActorLocation().X<1600?TEXT("Follow the forest trail to the highland"):TEXT("Explore the island / find the tower key");
    else if(!P->bGateOpen)Objective=TEXT("Use the key at the tower doorway");
    else Objective=TEXT("Collect 3 relics / return to the beach");
    const FVector Position=P->GetActorLocation();
    if(FVector::Dist2D(Position,ExpeditionTower::Base)<1200) {
        const float Height=FMath::Clamp((Position.Z-96-ExpeditionTower::Base.Z)/100.f,0.f,28.f);
        Objective=Height>27.5f?TEXT("RUINED TOWER SUMMIT / save at the beacon"):FString::Printf(TEXT("TOWER WALL  %.0f / 28 m  /  follow stone grips"),Height);
    }
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
        Text(P->Traversal==ETraversalState::Clinging||P->Traversal==ETraversalState::Reaching?TEXT("W S climb / A D traverse / SPACE top out / CTRL drop"):P->Traversal==ETraversalState::Hanging?TEXT("A / D shimmy / SPACE climb up / CTRL drop"):TEXT("CLIMBING"),W*.5f-210*S,H*.68f+14*S,Gold,1.05);
    } else if(P->Nearby.IsValid()&&!P->bJournal) {
        FString Prompt=TEXT("[E]  ")+P->Nearby->Prompt();float TW,TH;GetTextSize(Prompt,TW,TH,GEngine->GetSmallFont(),1.2*S);
        DrawRect(Ink,W/2-TW/2-18,H*.68f,TW+36,40*S);Text(Prompt,W/2-TW/2,H*.68f+12*S,Gold,1.2);
    }
    if(P->Traversal==ETraversalState::Walking&&FMath::Abs(Position.Z-(ExpeditionTower::SummitZ()+99))<30&&FMath::Abs(Position.Y-ExpeditionTower::Grip(ExpeditionTower::Steps-1).Y)<110&&Position.X<ExpeditionTower::Base.X-290) {
        DrawRect(Ink,W*.5f-245*S,H*.68f,490*S,44*S);Text(TEXT("Face the sea / [E] lower onto the wall handholds"),W*.5f-225*S,H*.68f+14*S,Gold,1.05);
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
        Text(TEXT("PALM ISLAND  /  an island beyond the charts"),X,Y+55*S,Dim,1.1);
        TArray<FString> Lines=P->bCompleted?TArray<FString>{TEXT("You recovered all three relics and escaped the sanctuary."),TEXT("The lost expedition's trail lives on."),TEXT("F5  /  Start a fresh expedition")}:TArray<FString>{TEXT("01  Leave the beach. Follow the trail through the palms."),TEXT("02  Climb the forest trail to the central highland."),TEXT("03  Explore the jungle and recover the tower key."),TEXT("04  E grabs stone handles. W/S climb; A/D traverse."),TEXT("05  SPACE climbs onto the roof. Recover the final relic."),TEXT("06  Return to the beach beacon to finish the expedition."),TEXT("RMB aim / LMB fire / 1-2 weapons / R reload"),TEXT("Q medkit / G grenade / CTRL drop / F5 fresh start"),TEXT("TAB  /  Return to the expedition")};
        for(int I=0;I<Lines.Num();I++)Text(Lines[I],X,Y+(100+I*35)*S,White,1.15);
    }
}
