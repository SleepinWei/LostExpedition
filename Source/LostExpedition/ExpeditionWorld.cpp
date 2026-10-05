#include "ExpeditionWorld.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

namespace {
const FLinearColor Stone(.59,.57,.45), Rock(.20,.22,.18), Wood(.36,.20,.10), Chalk(.93,.76,.40);
const TCHAR* CliffMesh=TEXT("/Game/Coastal/Meshes/SM_coastal_cliff_02.SM_coastal_cliff_02");
const TCHAR* RockMesh=TEXT("/Game/Coastal/Meshes/SM_rock_07.SM_rock_07");
const TCHAR* FernMesh=TEXT("/Game/Coastal/Meshes/SM_fern_02.SM_fern_02");
}
AExpeditionWorld::AExpeditionWorld() { RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")); }
void AExpeditionWorld::OnConstruction(const FTransform& T) { Super::OnConstruction(T); RebuildScene(); }
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
    auto Block=[&](FString N,FVector P,FVector S,FRotator Rot=FRotator::ZeroRotator,bool Collide=true){return Shape(N,TEXT("Cube"),P,S,Stone,Rot,Collide);};
    auto Trim=[&](FString N,FVector P,FVector S,FRotator Rot=FRotator::ZeroRotator){return Shape(N,TEXT("Cube"),P,S,Chalk,Rot);};
    auto Ground=[&](FVector P,FVector S){return Shape(TEXT("MossAndEarth"),TEXT("Cube"),P,S,FLinearColor(.13,.24,.09));};
    auto Beam=[&](FString N,FVector A,FVector B,float Width){FVector D=B-A;return Shape(N,TEXT("Cylinder"),(A+B)*.5,FVector(Width,Width,D.Size()),Wood,FRotationMatrix::MakeFromZ(D).Rotator(),false);};
    auto Scan=[&](FString N,const TCHAR* Path,FVector Center,FVector Dimensions,FRotator Rot){
        auto* C=Asset(N,Path,FVector::ZeroVector,100,Rot);if(!C)return C;
        const auto B=C->GetStaticMesh()->GetBounds(); const FVector Scale=Dimensions/(B.BoxExtent*2);
        C->SetRelativeScale3D(Scale);C->SetRelativeLocation(Center-Rot.RotateVector(B.Origin*Scale));
        if(N.Contains(TEXT("Core")))C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);return C;
    };
    // The collision route remains continuous. Thin surface geometry and scanned cliff faces
    // replace the old visible rectangular pedestal walls.
    auto Route=[&](FString N,FVector P,FVector S,bool Climb=false){auto* C=Shape(N,TEXT("Cube"),P,S,Rock,FRotator::ZeroRotator,true,Climb);return C;};
    Shape(TEXT("OpenSea"),TEXT("Plane"),FVector(15000,0,-2600),FVector(4000000,4000000,100),FLinearColor(.03,.12,.55),FRotator::ZeroRotator,false);
    Route(TEXT("Landing"),FVector(-1550,0,-200),FVector(1900,1900,400));
    Route(TEXT("FirstClimb"),FVector(-350,0,0),FVector(700,1200,380),true);
    Route(TEXT("SecondClimb"),FVector(370,0,90),FVector(760,1100,620),true);
    Route(TEXT("ThirdClimb"),FVector(1130,0,150),FVector(760,1100,940),true);
    for(auto P:{FVector(-705,0,180),FVector(-15,0,390),FVector(745,0,610)}) {
        // Broken limestone seams are the diegetic traversal cue.
        for(int J=-3;J<=3;J++)Shape(TEXT("WeatheredClimbSeam"),TEXT("Cube"),P+FVector(0,J*145,0),FVector(20,138,R.FRandRange(14,22)),Chalk,FRotator(0,0,R.FRandRange(-2,2)),false);
    }
    Route(TEXT("OutlookSupport"),FVector(2720,0,560),FVector(2420,1900,120));
    Route(TEXT("CourtyardSupport"),FVector(6310,0,560),FVector(2450,2450,120));
    Route(TEXT("SanctuarySupport"),FVector(8900,0,770),FVector(1900,2100,120));
    Ground(FVector(-1550,0,-8),FVector(1895,1895,15));
    Ground(FVector(2720,0,610),FVector(2410,1890,18));
    Ground(FVector(6310,0,610),FVector(2440,2440,18));
    Block(TEXT("SanctuaryPaving"),FVector(8900,0,821),FVector(1895,2095,16));
    for(int I=0;I<5;I++)Block(TEXT("SanctuaryPavingStairs"),FVector(7550+I*95,0,620+(I+1)*42-21),FVector(110,700,42));
    // A path of uneven surviving flags, with earth and vegetation showing between stones.
    for(int Zone=0;Zone<2;Zone++)for(int X=0;X<14;X++)for(int Y=-2;Y<=2;Y++) {
        if(R.FRand()<.13f && Y!=0)continue;
        float PX=(Zone?5150:1600)+X*170, PY=Y*170+((X%2)?32:0);
        Block(TEXT("BrokenPaving"),FVector(PX,PY,615+R.FRandRange(0,3)),FVector(R.FRandRange(147,163),R.FRandRange(148,164),12),FRotator(0,R.FRandRange(-2,2),0));
    }
    Scan(TEXT("OutlookCore"),TEXT("/Game/Coastal/Meshes/SM_CliffCore.SM_CliffCore"),FVector(2700,0,-940),FVector(2800,2550,3100),FRotator::ZeroRotator);
    Scan(TEXT("FortCore"),TEXT("/Game/Coastal/Meshes/SM_CliffCore.SM_CliffCore"),FVector(6310,0,-940),FVector(2750,3100,3100),FRotator::ZeroRotator);
    Scan(TEXT("SanctuaryCore"),TEXT("/Game/Coastal/Meshes/SM_CliffCore.SM_CliffCore"),FVector(8880,0,-830),FVector(1900,2750,3300),FRotator::ZeroRotator);
    // Scanned geology: broad, overlapping cliff sheets instead of isolated egg-shaped rocks.
    Scan(TEXT("WestCliff"),CliffMesh,FVector(-1300,100,-1460),FVector(3100,2900,2800),FRotator(0,180,0));
    Scan(TEXT("ClimbingCliff"),CliffMesh,FVector(450,200,-1400),FVector(2300,3000,2900),FRotator(0,180,0));
    Scan(TEXT("OutlookCliff"),CliffMesh,FVector(2550,250,-1570),FVector(3900,3400,4200),FRotator(0,186,0));
    Scan(TEXT("FortCliff"),CliffMesh,FVector(6980,480,-1570),FVector(5100,3800,4220),FRotator(0,172,0));
    Scan(TEXT("ChapelCliff"),CliffMesh,FVector(9270,450,-1410),FVector(3300,3600,4390),FRotator(0,192,0));
    // Secondary rocks disguise seams, anchor the buildings and vary the shoreline.
    for(int I=0;I<28;I++) {
        float X=R.FRandRange(-2400,10100);if(X>3850&&X<5150)continue;
        float Y=(I%2?1:-1)*R.FRandRange(1450,1900);float H=R.FRandRange(450,1100);
        float Top=X<0?0:X<1500?350:X<7800?650:850;
        Scan(TEXT("ScannedButtressRock"),RockMesh,FVector(X,Y,Top-H*.6),FVector(H*1.1,H*.9,H),FRotator(0,R.FRandRange(0,360),0));
    }
    // The upper, inland slope is deliberately taller; the ocean side stays open.
    for(int I=0;I<10;I++) {
        float X=-1500+I*1380;
        Scan(TEXT("InlandRockRidge"),RockMesh,FVector(X,3700+R.FRandRange(0,650),300),FVector(2400,2200,R.FRandRange(1800,2700)),FRotator(0,R.FRandRange(-20,20),0));
    }
    // A narrow, sagging timber crossing through an actual break in the cliff.
    for(int I=0;I<23;I++) {
        float X=3940+I*52, Dip=FMath::Sin(I*PI/22)*50;
        Shape(TEXT("BridgePlank"),TEXT("Cube"),FVector(X,R.FRandRange(-6,6),610-Dip),FVector(48,R.FRandRange(478,505),28),Wood,FRotator(0,R.FRandRange(-1.5,1.5),0));
        if(I%5==0||I==22)for(int S:{-1,1})Beam(TEXT("BridgePost"),FVector(X,S*260,590-Dip),FVector(X,S*260,790-Dip),18);
        if(I<22)for(int S:{-1,1}) {
            float NextDip=FMath::Sin((I+1)*PI/22)*50;
            Beam(TEXT("BridgeRope"),FVector(X,S*260,770-Dip),FVector(X+52,S*260,770-NextDip),5);
            Beam(TEXT("BridgeLowRope"),FVector(X,S*260,650-Dip),FVector(X+52,S*260,650-NextDip),4);
        }
    }
    for(float X:{3860.f,5150.f})for(int S:{-1,1}) {
        Block(TEXT("BridgeAnchor"),FVector(X,S*365,720),FVector(165,180,220));
        Trim(TEXT("BridgeAnchorCap"),FVector(X,S*365,840),FVector(185,200,24));
    }
    // Surviving sea wall and broken battlements. Deliberately unequal heights and gaps.
    for(int Side:{-1,1})for(int I=0;I<12;I++) {
        if((I==2||I==7)&&Side==-1)continue;
        float X=5200+I*190, H=R.FRandRange(95,230);
        Block(TEXT("FortParapet"),FVector(X,Side*1190,620+H*.5),FVector(187,110,H));
        if(I%3==0)Trim(TEXT("ParapetCoping"),FVector(X,Side*1190,620+H+12),FVector(199,135,24));
    }
    for(int I=0;I<6;I++) {
        float X=5470+(I%3)*670,Y=(I/3==0?-1:1)*520;
        Shape(TEXT("CourtyardCover"),TEXT("Cube"),FVector(X,Y,685),FVector(350,150,130),Stone,FRotator(0,R.FRandRange(-4,4),0),true,true);
        Trim(TEXT("CoverCoping"),FVector(X,Y,752),FVector(360,161,12));
    }
    // Hero chapel facade: open arch, side windows, broken pediment and asymmetric tower.
    Arch(FVector(8110,0,830),0);
    for(int S:{-1,1}) {
        Block(TEXT("FacadeWall"),FVector(8165,S*690,1370),FVector(170,550,1080));
        Block(TEXT("FacadePlaster"),FVector(8068,S*730,1360),FVector(16,360,785));
        Trim(TEXT("FacadeFooting"),FVector(8080,S*730,890),FVector(230,650,120));
        Trim(TEXT("FacadeCornice"),FVector(8080,S*650,1905),FVector(225,705,65));
        Block(TEXT("FacadeShoulder"),FVector(8170,S*620,2150),FVector(190,560,460));
        for(int J=0;J<3;J++)Trim(TEXT("FacadeButtress"),FVector(8010-J*35,S*1000,1070+J*265),FVector(260-J*40,165,480-J*95));
    }
    Block(TEXT("ArchSpandrel"),FVector(8165,0,2020),FVector(170,820,230));
    for(int I=-6;I<=6;I++) {
        float Y=I*62.f, Bottom=1470+FMath::Sqrt(FMath::Max(0.f,400.f*400.f-Y*Y));
        Block(TEXT("ArchSpandrelInfill"),FVector(8165,Y,(Bottom+1940)*.5),FVector(170,65,1940-Bottom));
    }
    // Jagged upper edge suggests the missing nave roof.
    for(int I=-5;I<=5;I++) {
        float H=280-FMath::Abs(I)*37+R.FRandRange(-60,65);
        Block(TEXT("BrokenPediment"),FVector(8165,I*170,2115+H*.5),FVector(180,168,H));
    }
    // Long side walls with rhythm from open archways, not a row of detached columns.
    for(int S:{-1,1}) {
        for(int I=0;I<6;I++) {
            float X=8250+I*300, H=(S==1?1050:520)+R.FRandRange(-180,180);
            Block(TEXT("NaveWall"),FVector(X,S*1000,830+H*.5),FVector(297,150,H));
            if(I%2==0)Trim(TEXT("NaveButtress"),FVector(X,S*1110,1150),FVector(165,220,640));
        }
        for(int I=0;I<4;I++) {
            float X=8320+I*470;
            Shape(TEXT("BrokenRoofBeam"),TEXT("Cube"),FVector(X,S*610,1920),FVector(30,790,34),Wood,FRotator(0,0,S*24),false);
        }
    }
    Arch(FVector(9730,0,830),0);
    Shape(TEXT("RelicAltar"),TEXT("Cylinder"),FVector(8750,-500,875),FVector(270,270,90),Stone);
    // Bell tower stands inland of the gate. Two levels of real window openings.
    const FVector Tower(8490,1490,830);
    for(int L=0;L<2;L++) {
        float B=Tower.Z+L*1050;
        for(int S:{-1,1}) {
            Block(TEXT("TowerCorner"),FVector(Tower.X-345,Tower.Y+S*295,B+505),FVector(220,210,1010));
            Block(TEXT("TowerCorner"),FVector(Tower.X+345,Tower.Y+S*295,B+505),FVector(220,210,1010));
            Block(TEXT("TowerSide"),FVector(Tower.X,Tower.Y+S*345,B+310),FVector(690,150,620));
            Block(TEXT("TowerWindowLintel"),FVector(Tower.X,Tower.Y+S*345,B+980),FVector(720,150,130));
            Block(TEXT("TowerFrontBase"),FVector(Tower.X+S*345,Tower.Y,B+310),FVector(150,610,620));
            Block(TEXT("TowerFrontLintel"),FVector(Tower.X+S*345,Tower.Y,B+980),FVector(150,630,130));
            Trim(TEXT("TowerStringCourse"),FVector(Tower.X,Tower.Y+S*350,B+1050),FVector(930,180,60));
            Trim(TEXT("TowerStringCourse"),FVector(Tower.X+S*400,Tower.Y,B+1050),FVector(170,810,60));
        }
    }
    for(int I=0;I<9;I++) {
        float H=R.FRandRange(110,320);
        Block(TEXT("TowerBrokenCrown"),Tower+FVector(-350+I*88,350,2110+H*.5),FVector(85,160,H));
    }
    // Abandoned storage bay on the approach, partly collapsed into the sea.
    for(int I=0;I<6;I++) {
        float H=I<3?R.FRandRange(330,520):R.FRandRange(100,230);
        Block(TEXT("OutlookRuinedWall"),FVector(1740+I*265,850,620+H*.5),FVector(261,145,H));
    }
    for(int I=0;I<4;I++)Block(TEXT("OutlookBrokenWall"),FVector(1870+I*230,-855,690+I%2*30),FVector(220,120,140+I%2*60));
    // Physical detail: weathered supply crates and masonry fallen from the adjacent wall.
    for(auto P:{FVector(2220,700,620),FVector(2380,730,620),FVector(5800,-1000,620),FVector(6150,-980,620),FVector(9160,730,830)})
        Asset(TEXT("AbandonedCrate"),TEXT("/Game/Coastal/Meshes/SM_wooden_crate_02.SM_wooden_crate_02"),P,R.FRandRange(80,115),FRotator(0,R.FRandRange(-25,25),0));
    for(int I=0;I<85;I++) {
        float X=R.FRandRange(5400,9700),Y=(I%2?1:-1)*R.FRandRange(830,1100);if(X>7480&&X<7950)continue;
        float Z=X>7950?830:620;
        Trim(TEXT("FallenMasonry"),FVector(X,Y,Z+R.FRandRange(10,26)),FVector(R.FRandRange(35,100),R.FRandRange(30,75),R.FRandRange(25,55)),FRotator(R.FRandRange(-18,18),R.FRandRange(0,180),R.FRandRange(-20,20)));
    }
    // Clustered foliage sits on known ground elevations, clear of gameplay sight lines.
    for(auto P:{FVector(-2090,640,0),FVector(1750,730,620),FVector(3220,-900,620),FVector(5430,1050,620),FVector(7010,1030,620),FVector(9650,900,830)})Palm(P,R.FRandRange(1,1.35),R);
    Asset(TEXT("UE_ArchVisTree"),TEXT("/Game/ArchVis/SampleScene/Tree/HillTree_02.HillTree_02"),FVector(2680,1850,200),2000,FRotator(0,75,0));
    for(int I=0;I<210;I++) {
        float X=R.FRandRange(1600,9760);if(X>3810&&X<5220||X>7470&&X<8000)continue;
        float Y=(I%2?1:-1)*R.FRandRange(570,X<3900?910:1010),Z=X>7950?832:622;
        Asset(TEXT("CC0_PathGrass"),TEXT("/Game/Nature/SM_TownGrass.SM_TownGrass"),FVector(X,Y,Z),R.FRandRange(35,76),FRotator(0,R.FRandRange(0,360),0));
        if(I%3==0)Asset(TEXT("CoastalFern"),FernMesh,FVector(X+30,Y-20,Z),R.FRandRange(45,75),FRotator(0,R.FRandRange(0,360),0));
    }
    // Ferns on the facade read as plants reclaiming mortar and broken ledges.
    for(int I=0;I<36;I++) {
        float Y=(I%2?1:-1)*R.FRandRange(500,990),Z=R.FRandRange(920,2250);
        Asset(TEXT("WallVegetation"),FernMesh,FVector(8010,Y,Z),R.FRandRange(40,80),FRotator(0,R.FRandRange(0,360),R.FRandRange(-12,12)));
    }
    Scan(TEXT("OceanHeadland"),RockMesh,FVector(23500,-12000,-1700),FVector(15000,9500,7800),FRotator(0,35,0));
    Scan(TEXT("OceanFarIsland"),RockMesh,FVector(39000,-24000,-2500),FVector(20000,13000,9500),FRotator(0,-35,0));
    // Background islands are closed rock scans, with haze between them.
    for(int I=0;I<7;I++) {
        float X=19000+I*5500,Y=9000+I*4000;
        Scan(TEXT("DistantHeadland"),RockMesh,FVector(X,Y,-1800),FVector(17000,12000,7200+I*850),FRotator(0,25+I*35,0));
    }
}
