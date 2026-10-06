#include "ExpeditionWorld.h"
#include "ExpeditionTower.h"
#include "IslandTerrain.h"
#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

namespace {
const FLinearColor Stone(.59,.57,.45), Rock(.20,.22,.18), Wood(.36,.20,.10), Chalk(.93,.76,.40);
const TCHAR* CliffMesh=TEXT("/Game/Coastal/Meshes/SM_coastal_cliff_02.SM_coastal_cliff_02");
const TCHAR* RockMesh=TEXT("/Game/Coastal/Meshes/SM_rock_07.SM_rock_07");
const TCHAR* FernMesh=TEXT("/Game/Coastal/Meshes/SM_fern_02.SM_fern_02");
}
AExpeditionWorld::AExpeditionWorld() {
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    Terrain=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("IslandTerrain"));Terrain->SetupAttachment(RootComponent);
    Terrain->bUseComplexAsSimpleCollision=true;Terrain->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);
    Surf=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ShoreWash"));Surf->SetupAttachment(RootComponent);Surf->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
float AExpeditionWorld::GroundHeight(float X,float Y) const{return IslandTerrain::Height(X,Y);}
void AExpeditionWorld::OnConstruction(const FTransform& T) { Super::OnConstruction(T); RebuildScene(); }
void AExpeditionWorld::BeginPlay() {
    Super::BeginPlay();
    // Existing generated maps keep their serialized components. Add the contact
    // surface tag without rebuilding the island or replacing the user's edits.
    for(auto Part:Pieces)if(Part&&Part->GetName().StartsWith(TEXT("TowerWestWall")))Part->ComponentTags.AddUnique(TEXT("Climbable"));
}
UStaticMeshComponent* AExpeditionWorld::Shape(const FString& Name,const FString& Mesh,FVector P,FVector S,FLinearColor C,FRotator R,bool Collision,bool Climb) {
    auto* Part=NewObject<UStaticMeshComponent>(this,*FString::Printf(TEXT("%s_%d"),*Name,Pieces.Num()));
    Part->CreationMethod=EComponentCreationMethod::UserConstructionScript; Part->SetupAttachment(RootComponent);
    Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*(TEXT("/Engine/BasicShapes/")+Mesh+TEXT(".")+Mesh)));
    Part->SetRelativeLocation(P); Part->SetRelativeRotation(R); Part->SetRelativeScale3D(S/100.f);
    if(Mesh==TEXT("Cube") && (Name.Contains(TEXT("Arch"))||Name.Contains(TEXT("Wall"))||Name.Contains(TEXT("Tower"))||Name.Contains(TEXT("Facade"))||Name.Contains(TEXT("Masonry"))||Name.Contains(TEXT("Coping"))||Name.Contains(TEXT("Pediment"))||Name.Contains(TEXT("Buttress")))) {
        if(auto* Bevel=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Coastal/Meshes/SM_WeatheredBlock.SM_WeatheredBlock"))) {
            Part->SetStaticMesh(Bevel);Part->SetRelativeScale3D(S/(Bevel->GetBounds().BoxExtent*2));
        }
    }
    Part->SetCollisionEnabled(Collision?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
    Part->SetCollisionResponseToAllChannels(ECR_Block); Part->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);
    if(Climb) Part->ComponentTags.Add(TEXT("Climbable"));
    FString Mat=TEXT("M_ExpeditionStone");
    if(C.B>.45f && C.B>C.R) Mat=TEXT("M_CoastalWater");
    else if(C.G>C.R*1.2f) Mat=TEXT("M_CoastalGround");
    else if(C.R>.8f) Mat=TEXT("M_CoastalTrim");
    else if(C.R>C.G*1.5f) Mat=TEXT("M_ExpeditionWood");
    else if(C.R<.25f) Mat=TEXT("M_ExpeditionRock");
    if(Name.Contains(TEXT("Paving")))Mat=TEXT("M_CoastalPaving");
    if(Name.Contains(TEXT("Plaster")))Mat=TEXT("M_CoastalPlaster");
    Part->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,*(TEXT("/Game/Materials/")+Mat+TEXT("V2"))));
    Part->RegisterComponent(); Pieces.Add(Part); return Part;
}
UStaticMeshComponent* AExpeditionWorld::Asset(const FString& Name,const TCHAR* Path,FVector P,float Height,FRotator Rot) {
    UStaticMesh* Mesh=LoadObject<UStaticMesh>(nullptr,Path); if(!Mesh)return nullptr;
    auto* Part=NewObject<UStaticMeshComponent>(this,*FString::Printf(TEXT("%s_%d"),*Name,Pieces.Num()));
    Part->CreationMethod=EComponentCreationMethod::UserConstructionScript;Part->SetupAttachment(RootComponent);
    Part->SetStaticMesh(Mesh);Part->SetRelativeRotation(Rot);
    const auto Bounds=Mesh->GetBounds();const float Scale=Height/FMath::Max(1.f,Bounds.BoxExtent.Z*2);
    // Every asset is positioned by its bottom centre, independent of its imported pivot.
    const FVector Bottom=Bounds.Origin-FVector(0,0,Bounds.BoxExtent.Z);
    Part->SetRelativeLocation(P-Rot.RotateVector(Bottom*Scale));Part->SetRelativeScale3D(FVector(Scale));
    Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->RegisterComponent();Pieces.Add(Part);return Part;
}
void AExpeditionWorld::Palm(FVector P,float Scale,FRandomStream& R) {
    Asset(TEXT("CC0_IslandTree"),TEXT("/Game/Nature/SM_TownTree.SM_TownTree"),P,1250*Scale,FRotator(0,R.FRandRange(0,360),0));
}
void AExpeditionWorld::Arch(FVector P,float Yaw) {
    const FRotator Rotation(0,Yaw,0);
    auto Place=[&](FString N,FVector Q,FVector S,FRotator Local=FRotator::ZeroRotator){return Shape(N,TEXT("Cube"),P+Rotation.RotateVector(Q),S,Chalk,FRotator(Local.Pitch,Yaw,Local.Roll));};
    // Actual open arch: human-scale spring line and a thicker keystone.
    for(int Side:{-1,1}) {
        Place(TEXT("ArchPierFoot"),FVector(0,Side*325,40),FVector(225,180,80));
        for(int I=0;I<7;I++)Place(TEXT("ArchPierStone"),FVector(0,Side*325,80+I*76+36),FVector(190,146,72));
        Place(TEXT("ArchImpost"),FVector(0,Side*325,615),FVector(220,175,45));
    }
    for(int I=0;I<15;I++) {
        float A=(I+.5f)*PI/15.f;
        Place(TEXT("ArchRadialStone"),FVector(0,325*FMath::Cos(A),640+325*FMath::Sin(A)),FVector(I==7?235:195,69,150),FRotator(0,0,FMath::RadiansToDegrees(A)-90));
    }
}
void AExpeditionWorld::RebuildScene() {
    for(auto P:Pieces)if(IsValid(P))P->DestroyComponent();Pieces.Empty();
    FRandomStream R(Seed);
    TArray<FVector> V,N;TArray<int32> Tri;TArray<FVector2D> UV;TArray<FLinearColor> Colors;TArray<FProcMeshTangent> Tangents;
    constexpr int32 NX=141,NY=121;constexpr float Cell=200;
    for(int32 J=0;J<NY;J++)for(int32 I=0;I<NX;I++) {
        const float X=(I-(NX-1)*.5f)*Cell,Y=(J-(NY-1)*.5f)*Cell;
        const FVector Normal=IslandTerrain::Normal(X,Y);const float H=GroundHeight(X,Y);
        V.Add(FVector(X,Y,H));N.Add(Normal);UV.Add(FVector2D(X/1000,Y/1000));
        float Sand=IslandTerrain::Smooth((IslandTerrain::Radius(X,Y)-.68f)/.19f);
        float Cliff=IslandTerrain::Smooth((.92f-Normal.Z)/.3f);
        Colors.Add(FLinearColor(Sand,Cliff,0,1));
    }
    for(int32 J=0;J<NY-1;J++)for(int32 I=0;I<NX-1;I++){int32 A=J*NX+I;Tri.Append({A,A+NX,A+1,A+1,A+NX,A+NX+1});}
    Terrain->ClearAllMeshSections();Terrain->CreateMeshSection_LinearColor(0,V,Tri,N,UV,Colors,Tangents,true);
    Terrain->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_IslandTerrain")));
    Terrain->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Terrain->SetCollisionResponseToAllChannels(ECR_Block);Terrain->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);
    V.Empty();N.Empty();Tri.Empty();UV.Empty();Colors.Empty();
    for(int32 I=0;I<=240;I++) {
        float A=I*2*PI/240,Edge=IslandTerrain::Shore(A);
        for(int32 Band=0;Band<2;Band++) {
            float Q=Edge+.002f+Band*.012f;
            V.Add(FVector(FMath::Cos(A)*10500*Q,FMath::Sin(A)*8500*Q,6));N.Add(FVector::UpVector);UV.Add(FVector2D(I*.7f,Band));Colors.Add(FLinearColor::White);
        }
        if(I<240){int32 K=I*2;Tri.Append({K,K+2,K+1,K+1,K+2,K+3});}
    }
    Surf->ClearAllMeshSections();Surf->CreateMeshSection_LinearColor(0,V,Tri,N,UV,Colors,Tangents,false);Surf->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_IslandFoam")));
    auto* Sea=Shape(TEXT("OpenSea"),TEXT("Plane"),FVector(0,0,-6),FVector(4000000,4000000,100),FLinearColor(.03,.12,.55),FRotator::ZeroRotator,false);
    Sea->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_IslandWater")));
    auto Block=[&](FString Name,FVector P,FVector S,bool Collision=true){return Shape(Name,TEXT("Cube"),P,S,Stone,FRotator::ZeroRotator,Collision);};
    auto Trim=[&](FString Name,FVector P,FVector S,bool Collision=true){return Shape(Name,TEXT("Cube"),P,S,Chalk,FRotator::ZeroRotator,Collision);};
    auto Beam=[&](FString Name,FVector A,FVector B,float Width){const FVector D=B-A;return Shape(Name,TEXT("Cylinder"),(A+B)*.5,FVector(Width,Width,D.Size()),Wood,FRotationMatrix::MakeFromZ(D).Rotator(),false);};
    auto Scan=[&](FString Name,FVector P,FVector Size,FRotator Rot){auto* C=Asset(Name,RockMesh,FVector::ZeroVector,100,Rot);if(!C)return;auto B=C->GetStaticMesh()->GetBounds();FVector S=Size/(B.BoxExtent*2);C->SetRelativeScale3D(S);C->SetRelativeLocation(P-Rot.RotateVector(B.Origin*S));};
    // Geological clusters follow the actual slope, with the beach and trail kept open.
    for(int I=0;I<75;I++) {
        float A=R.FRandRange(0,2*PI),Q=R.FRandRange(.81,1.09);float X=1400+FMath::Cos(A)*4300*Q,Y=700+FMath::Sin(A)*3400*Q;
        if(X<1300&&FMath::Abs(Y-IslandTerrain::TrailY(X))<950)continue;
        float H=R.FRandRange(450,1000);Scan(TEXT("HighlandRock"),FVector(X,Y,GroundHeight(X,Y)-H*.18f),FVector(H*1.5,H,H),FRotator(0,R.FRandRange(0,360),0));
    }
    for(int I=0;I<45;I++) {
        float A=R.FRandRange(0,2*PI),Q=IslandTerrain::Shore(A)*R.FRandRange(.92,1.06);
        float X=FMath::Cos(A)*10500*Q,Y=FMath::Sin(A)*8500*Q;if(X<-3800&&Y<-3000)continue;
        float H=R.FRandRange(100,360);Scan(TEXT("TidalRock"),FVector(X,Y,GroundHeight(X,Y)+H*.2),FVector(H*1.7,H*1.3,H),FRotator(0,R.FRandRange(0,360),0));
    }
    // The climbable ruin is the high point. It has real openings, a missing roof and a broken crown.
    const FVector T=ExpeditionTower::Base;const float Top=ExpeditionTower::SummitZ();
    Block(TEXT("TowerFoundation"),T-FVector(0,0,45),FVector(1180,1130,90));
    Block(TEXT("TowerWestWall"),T+FVector(-440,0,1400),FVector(120,1000,2800))->ComponentTags.Add(TEXT("Climbable"));
    for(int L=0;L<3;L++) {
        const float B=T.Z+L*900;
        for(int SX:{-1,1})for(int SY:{-1,1}) {
            Block(TEXT("TowerCorner"),FVector(T.X+SX*415,T.Y+SY*420,B+440),FVector(170,160,880));
            for(int K=0;K<6;K++)Trim(TEXT("TowerQuoin"),FVector(T.X+SX*420,T.Y+SY*425,B+65+K*148),FVector(180,172,120),false);
        }
        for(int Side:{-1,1}) {
            for(int J:{-1,1})Block(TEXT("TowerWindowPier"),FVector(T.X+J*300,T.Y+Side*440,B+450),FVector(220,120,900));
            Block(TEXT("TowerWindowSill"),FVector(T.X,T.Y+Side*440,B+185),FVector(400,120,370));
            Block(TEXT("TowerWindowLintel"),FVector(T.X,T.Y+Side*440,B+805),FVector(400,120,190));
            for(int K=0;K<11;K++) {
                float A=(K+.5f)*PI/11;
                Shape(TEXT("TowerWindowArch"),TEXT("Cube"),FVector(T.X+200*FMath::Cos(A),T.Y+Side*440,B+510+200*FMath::Sin(A)),FVector(62,145,92),Chalk,FRotator(90-FMath::RadiansToDegrees(A),0,0),false);
            }
        }
        // Part of the inland wall has collapsed, revealing the dark interior.
        Block(TEXT("TowerEastRemnant"),FVector(T.X+440,T.Y+280,B+440),FVector(120,410,880));
        if(L==0)Block(TEXT("TowerEastRubbleWall"),FVector(T.X+440,T.Y-280,B+200),FVector(120,350,400));
        Block(TEXT("TowerInteriorFloor"),FVector(T.X,T.Y,B-20),FVector(880,880,40));
        // Courses stay flush on the climbing face so the capsule cannot snag.
        for(int Side:{-1,1})Trim(TEXT("TowerCornice"),FVector(T.X,T.Y+Side*490,B+875),FVector(1040,55,50));
    }
    Block(TEXT("TowerRoofDeck"),FVector(T.X,T.Y,Top-35),FVector(1000,1000,70));
    for(int I=0;I<9;I++) {
        float H=R.FRandRange(90,390);
        Block(TEXT("TowerBrokenCrown"),FVector(T.X-450+I*110,T.Y+460,Top+H*.5),FVector(108,100,H));
        if(I>4)Block(TEXT("TowerBrokenCrown"),FVector(T.X+460,T.Y-450+I*110,Top+H*.3),FVector(100,108,H*.6));
    }
    for(int Side:{-1,1})Block(TEXT("TowerSummitParapet"),FVector(T.X+Side*460,T.Y-280,Top+65),FVector(80,400,130));
    Beam(TEXT("BrokenTowerRoof"),FVector(T.X-420,T.Y+330,Top+190),FVector(T.X+220,T.Y+240,Top+280),35);
    Beam(TEXT("BrokenTowerRoof"),FVector(T.X+390,T.Y+340,Top+230),FVector(T.X+320,T.Y-80,Top+310),27);
    // Discrete protruding stone handles, not walkable exterior stairs.
    for(int I=0;I<ExpeditionTower::Steps;I++) {
        FVector P=ExpeditionTower::Grip(I);const float Width=I==7||I==8?105:78;
        auto* Grip=Trim(FString::Printf(TEXT("WallGrip_%02d"),I),P+FVector(23,0,-10),FVector(48,Width,20),false);
        Grip->ComponentTags.Add(TEXT("WallGrip"));
        Shape(TEXT("GripMortarCrack"),TEXT("Cube"),P+FVector(39,0,-30),FVector(3,Width+24,8),Rock,FRotator::ZeroRotator,false);
        if(I%3==0)Trim(TEXT("ClimbFootNub"),P+FVector(30,30,-90),FVector(27,38,15),false);
    }
    for(int I=0;I<60;I++) {
        float A=R.FRandRange(0,2*PI),Radius=R.FRandRange(670,1400),X=T.X+FMath::Cos(A)*Radius,Y=T.Y+FMath::Sin(A)*Radius;
        if(X<T.X&&FMath::Abs(Y-(T.Y-260))<200)continue;
        Shape(TEXT("FallenTowerMasonry"),TEXT("Cube"),FVector(X,Y,GroundHeight(X,Y)+R.FRandRange(15,40)),FVector(R.FRandRange(35,110),R.FRandRange(40,120),R.FRandRange(30,90)),Stone,FRotator(R.FRandRange(-20,20),R.FRandRange(0,360),R.FRandRange(-15,15)),false);
    }
    // One instanced component per species keeps the tropical understory inexpensive to render.
    auto Instances=[&](const FString& Name,const TCHAR* Path){
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,Path);if(!Mesh)return (UHierarchicalInstancedStaticMeshComponent*)nullptr;
        auto* C=NewObject<UHierarchicalInstancedStaticMeshComponent>(this,*Name);C->CreationMethod=EComponentCreationMethod::UserConstructionScript;C->SetupAttachment(RootComponent);C->SetStaticMesh(Mesh);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCullDistances(22000,36000);C->RegisterComponent();Pieces.Add(C);return C;
    };
    auto* Palms=Instances(TEXT("IslandPalms"),TEXT("/Game/Island/SM_CoconutPalm"));
    auto* Trees=Instances(TEXT("JungleCanopy"),TEXT("/Game/Nature/SM_TownTree"));
    if(Trees){Trees->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_IslandTrunk")));Trees->SetMaterial(1,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_IslandCanopy")));Trees->SetMaterial(2,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Materials/M_IslandBranches")));}
    auto* Ferns=Instances(TEXT("JungleFerns"),FernMesh);
    auto* Grass=Instances(TEXT("JungleGrass"),TEXT("/Game/Nature/SM_TownGrass"));
    auto Plant=[&](UHierarchicalInstancedStaticMeshComponent* C,FVector P,float Height,float Yaw){if(!C)return;auto B=C->GetStaticMesh()->GetBounds();float S=Height/FMath::Max(B.BoxExtent.Z*2,1.f);FRotator Rot(0,Yaw,0);P-=Rot.RotateVector((B.Origin-FVector(0,0,B.BoxExtent.Z))*S);C->AddInstance(FTransform(Rot,P,FVector(S)));};
    for(int I=0;I<3000;I++) {
        float X=R.FRandRange(-9300,9300),Y=R.FRandRange(-7800,7800),Q=IslandTerrain::Radius(X,Y),H=GroundHeight(X,Y);
        if(Q>.91||H<170||IslandTerrain::Normal(X,Y).Z<.9)continue;
        if(FVector2D(X-T.X,Y-T.Y).Size()<1200)continue;
        if(X>-7000&&X<1550&&FMath::Abs(Y-IslandTerrain::TrailY(X))<500)continue;
        if(X<-5300&&Y<-3300&&FMath::Abs(Y+.7f*X+9540)<500)continue;
        FVector P(X,Y,H);float A=R.FRandRange(0,360);
        if(I%15==0)Plant(Palms,P,R.FRandRange(1000,1700),A);
        else if(I%12==0&&Q<.78)Plant(Trees,P,R.FRandRange(800,1550),A);
        else if(I%3==0)Plant(Ferns,P,R.FRandRange(75,155),A);
        else Plant(Grass,P,R.FRandRange(45,105),A);
    }
    // Beach palms frame the landing without filling the open sand with grass.
    for(auto P:{FVector(-7350,-3650,0),FVector(-5500,-5300,0),FVector(-4300,-5800,0),FVector(-8000,-2600,0),FVector(4900,-5200,0),FVector(6100,-4300,0)}){P.Z=GroundHeight(P.X,P.Y);Plant(Palms,P,R.FRandRange(1350,1750),R.FRandRange(0,360));}
    for(int I=0;I<38;I++) {
        float Y=T.Y+R.FRandRange(-390,390),Z=T.Z+R.FRandRange(200,2700);
        if(FMath::Abs(Y-ExpeditionTower::Grip(FMath::Clamp(int((Z-T.Z)/130),0,21)).Y)<80)continue;
        Plant(Ferns,FVector(T.X-485,Y,Z),R.FRandRange(45,85),R.FRandRange(0,360));
    }
    // Weathered supplies on the beach and a few route stones at the trail entrance.
    for(auto P:{FVector(-6910,-4380,0),FVector(-6800,-4300,0),FVector(2280,1650,0)}){P.Z=GroundHeight(P.X,P.Y);Asset(TEXT("IslandSupplyCrate"),TEXT("/Game/Coastal/Meshes/SM_wooden_crate_02"),P,85,FRotator(0,R.FRandRange(-20,20),0));}
    for(int I=0;I<14;I++){float X=-6100+I*490,Y=IslandTerrain::TrailY(X);for(int Side:{-1,1})Scan(TEXT("TrailStone"),FVector(X,Y+Side*380,GroundHeight(X,Y+Side*380)+20),FVector(95,75,70),FRotator(0,R.FRandRange(0,360),0));}
}
