#include "ExplorerMotionMatching.h"

#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AnimGraphNode_MotionMatching.h"
#include "AnimGraphNode_PoseSearchHistoryCollector.h"
#include "AnimGraphNode_SequenceEvaluator.h"
#include "AnimGraphNode_BlendListByBool.h"
#include "AnimGraphNode_Root.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "PoseSearch/AnimNode_MotionMatching.h"
#include "PoseSearch/AnimNode_PoseSearchHistoryCollector.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearch/PoseSearchFeatureChannel_Trajectory.h"
#include "PoseSearch/PoseSearchFeatureChannel_Pose.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace ExpeditionMMSetup {
const FString Folder=TEXT("/Game/Animation/MotionMatching/");
bool Save(UObject* Asset) {
    UPackage* Package=Asset->GetOutermost();Package->MarkPackageDirty();
    FString Filename=FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension());
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename),true);
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
    return UPackage::SavePackage(Package,Asset,*Filename,Args);
}
template<class T>T* Asset(const FString& Path,bool& New) {
    if(T* Existing=LoadObject<T>(nullptr,*Path)){New=false;return Existing;}
    New=true;auto* Result=NewObject<T>(CreatePackage(*Path),*FPackageName::GetShortName(Path),RF_Public|RF_Standalone);
    FAssetRegistryModule::AssetCreated(Result);return Result;
}
template<class T>T* Node(UEdGraph* Graph,int32 X,int32 Y) {
    T* Result=NewObject<T>(Graph);Graph->AddNode(Result,false,false);Result->CreateNewGuid();Result->PostPlacedNewNode();Result->NodePosX=X;Result->NodePosY=Y;Result->AllocateDefaultPins();return Result;
}
template<class T>T& Setting(UScriptStruct* Struct,void* Data,const TCHAR* Name) {
    return *FindFProperty<FProperty>(Struct,Name)->ContainerPtrToValuePtr<T>(Data);
}
void Show(UAnimGraphNode_Base* Node,const TCHAR* Name) {
    for(auto& Optional:Node->ShowPinForProperties)if(Optional.PropertyName==Name)Optional.bShowPin=true;
    Node->ReconstructNode();
}
bool Connect(UEdGraph* Graph,UEdGraphNode* From,const TCHAR* Output,UEdGraphNode* To,const TCHAR* Input) {
    auto* A=From->FindPin(Output,EGPD_Output);auto* B=To->FindPin(Input,EGPD_Input);
    if(!A||!B){UE_LOG(LogTemp,Error,TEXT("MM_SETUP missing pin %s.%s -> %s.%s"),*From->GetClass()->GetName(),Output,*To->GetClass()->GetName(),Input);return false;}
    return Graph->GetSchema()->TryCreateConnection(A,B);
}
bool Bind(UEdGraph* Graph,const TCHAR* Property,UEdGraphNode* To,const TCHAR* Input,int32 Y) {
    auto* Get=NewObject<UK2Node_VariableGet>(Graph);Graph->AddNode(Get,false,false);Get->CreateNewGuid();Get->VariableReference.SetSelfMember(Property);Get->NodePosX=-1000;Get->NodePosY=Y;Get->AllocateDefaultPins();
    return Connect(Graph,Get,Property,To,Input);
}
}
#endif

FString UExpeditionMotionMatchingLibrary::BuildMotionMatchingContent() {
#if WITH_EDITOR
    using namespace ExpeditionMMSetup;
    auto* Skeleton=LoadObject<USkeleton>(nullptr,TEXT("/Game/Characters/Mannequins/Meshes/SK_Mannequin"));
    if(!Skeleton)return TEXT("ERROR: restore the official mannequin template assets first");
    bool IsNew=false;auto* Schema=Asset<UPoseSearchSchema>(Folder+TEXT("PSS_Explorer"),IsNew);
    if(IsNew)Schema->AddSkeleton(Skeleton);
    auto* ChannelsProperty=FindFProperty<FArrayProperty>(Schema->GetClass(),TEXT("Channels"));
    FScriptArrayHelper(ChannelsProperty,ChannelsProperty->ContainerPtrToValuePtr<void>(Schema)).EmptyValues();
    Schema->SampleRate=30;
    auto* Trajectory=NewObject<UPoseSearchFeatureChannel_Trajectory>(Schema);Trajectory->Weight=12.f;Trajectory->Samples.Reset();
    for(float Time:{-.15f,.1f,.25f,.5f}) {
        FPoseSearchTrajectorySample Sample;Sample.Offset=Time;Sample.Flags=int32(EPoseSearchTrajectoryFlags::VelocityXY)|int32(EPoseSearchTrajectoryFlags::FacingDirectionXY);Sample.Weight=Time<0?.5f:1.f;Trajectory->Samples.Add(Sample);
    }
    Schema->AddChannel(Trajectory);
    auto* Pose=NewObject<UPoseSearchFeatureChannel_Pose>(Schema);Pose->Weight=.5f;Pose->SampledBones.Reset();Pose->InputQueryPose=EInputQueryPose::UseCharacterPose;
    for(const TCHAR* Bone:{TEXT("pelvis"),TEXT("foot_l"),TEXT("foot_r")}) {
        FPoseSearchBone Sample;Sample.Reference.BoneName=Bone;Sample.Flags=int32(EPoseSearchBoneFlags::Position)|int32(EPoseSearchBoneFlags::Velocity);Sample.Weight=Sample.Reference.BoneName==TEXT("pelvis")?.5f:1.f;Pose->SampledBones.Add(Sample);
    }
    Schema->AddChannel(Pose);Schema->PostEditChange();if(!Save(Schema))return TEXT("ERROR: saving schema");
    FString Report;UPoseSearchDatabase* FirstDatabase=nullptr;
    for(const TCHAR* Type:{TEXT("Unarmed"),TEXT("Pistol"),TEXT("Rifle")}) {
        FString Style(Type);auto* Database=Asset<UPoseSearchDatabase>(Folder+TEXT("PSD_")+Style,IsNew);Database->Schema=Schema;
        // These small databases fit an exact search. PCA with four default
        // dimensions can omit a valid backward pose after a weapon transition.
        Database->PoseSearchMode=EPoseSearchMode::BruteForce;Database->ContinuingPoseCostBias=-.003f;
        while(Database->GetNumAnimationAssets()>0)Database->RemoveAnimationAssetAt(Database->GetNumAnimationAssets()-1);
        TArray<FString> Paths;
        Paths.Add(Style==TEXT("Unarmed")?TEXT("Unarmed/MM_Idle"):Style+TEXT("/MF_")+Style+TEXT("_Idle_ADS"));
        for(const TCHAR* Gait:{TEXT("Walk"),TEXT("Jog")})for(const TCHAR* Direction:{TEXT("Fwd"),TEXT("Fwd_Right"),TEXT("Right"),TEXT("Bwd_Right"),TEXT("Bwd"),TEXT("Bwd_Left"),TEXT("Left"),TEXT("Fwd_Left")})
            Paths.Add(Style+TEXT("/")+Gait+TEXT("/MF_")+Style+TEXT("_")+Gait+TEXT("_")+Direction);
        for(const FString& Path:Paths) {
            auto* Source=LoadObject<UAnimSequence>(nullptr,*(TEXT("/Game/Characters/Mannequins/Anims/")+Path));if(!Source)return TEXT("ERROR: missing animation ")+Path;
            const FString CopyPath=Folder+TEXT("Sequences/")+Source->GetName();auto* Sequence=LoadObject<UAnimSequence>(nullptr,*CopyPath);
            if(!Sequence){Sequence=DuplicateObject<UAnimSequence>(Source,CreatePackage(*CopyPath),*Source->GetName());Sequence->SetFlags(RF_Public|RF_Standalone);FAssetRegistryModule::AssetCreated(Sequence);}
            // Search sees the original root trajectory; extraction keeps the rendered
            // skeleton in place while CharacterMovement controls capsule collision.
            Sequence->bLoop=true;Sequence->bEnableRootMotion=true;Sequence->bForceRootLock=true;
            if(!Save(Sequence))return TEXT("ERROR: saving generated sequence ")+CopyPath;
            FPoseSearchDatabaseAnimationAsset Entry;Entry.AnimAsset=Sequence;Database->AddAnimationAsset(Entry);
            UE_LOG(LogTemp,Display,TEXT("MM_SETUP clip=%s rootSpeed=%.1f loop=%d"),*Sequence->GetName(),Sequence->ExtractRootMotionFromRange(0,Sequence->GetPlayLength(),FAnimExtractContext()).GetTranslation().Size()/Sequence->GetPlayLength(),Sequence->bLoop);
        }
        Database->PostEditChange();
        using namespace UE::PoseSearch;
        auto Result=FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database,ERequestAsyncBuildFlag::NewRequest|ERequestAsyncBuildFlag::WaitForCompletion);
        if(Result!=EAsyncBuildIndexResult::Success||Database->GetSearchIndex().GetNumPoses()==0)return TEXT("ERROR: database indexing ")+Style;
        if(!Save(Database))return TEXT("ERROR: saving database ")+Style;
        Report+=FString::Printf(TEXT("%s: %d animations, %d indexed poses\n"),*Database->GetName(),Database->GetNumAnimationAssets(),Database->GetSearchIndex().GetNumPoses());
        if(!FirstDatabase)FirstDatabase=Database;
    }
    const FString BPPath=Folder+TEXT("ABP_ExplorerMotionMatching");auto* BP=LoadObject<UAnimBlueprint>(nullptr,*BPPath);
    if(!BP) {
        BP=Cast<UAnimBlueprint>(FKismetEditorUtilities::CreateBlueprint(UExplorerMotionMatching::StaticClass(),CreatePackage(*BPPath),TEXT("ABP_ExplorerMotionMatching"),BPTYPE_Normal,UAnimBlueprint::StaticClass(),UAnimBlueprintGeneratedClass::StaticClass(),TEXT("ExpeditionMotionMatchingSetup")));
        FAssetRegistryModule::AssetCreated(BP);
    }
    BP->TargetSkeleton=Skeleton;TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);UEdGraph* Graph=nullptr;
    for(auto* Candidate:Graphs)if(Candidate->GetFName()==TEXT("AnimGraph")){Graph=Candidate;break;}
    if(!Graph)return TEXT("ERROR: AnimGraph was not created");
    TArray<TObjectPtr<UEdGraphNode>> OldNodes=Graph->Nodes;for(auto Old:OldNodes)Graph->RemoveNode(Old);
    auto* MM=Node<UAnimGraphNode_MotionMatching>(Graph,-650,0);
    auto* MMData=FindFProperty<FStructProperty>(MM->GetClass(),TEXT("Node"))->ContainerPtrToValuePtr<FAnimNode_MotionMatching>(MM);
    Setting<TObjectPtr<const UPoseSearchDatabase>>(FAnimNode_MotionMatching::StaticStruct(),MMData,TEXT("Database"))=FirstDatabase;
    Setting<float>(FAnimNode_MotionMatching::StaticStruct(),MMData,TEXT("BlendTime"))=.18f;
    Setting<float>(FAnimNode_MotionMatching::StaticStruct(),MMData,TEXT("PoseReselectHistory"))=.15f;
    Setting<FFloatInterval>(FAnimNode_MotionMatching::StaticStruct(),MMData,TEXT("PoseJumpThresholdTime"))=FFloatInterval(.15f,.15f);
    Setting<FFloatInterval>(FAnimNode_MotionMatching::StaticStruct(),MMData,TEXT("PlayRate"))=FFloatInterval(.65f,1.8f);
    auto* Air=Node<UAnimGraphNode_SequenceEvaluator>(Graph,-650,400);Air->SetAnimationAsset(LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Fall_Loop")));
    Show(Air,TEXT("Sequence"));Show(Air,TEXT("ExplicitTime"));
    auto* Blend=Node<UAnimGraphNode_BlendListByBool>(Graph,-300,0);
    Setting<TArray<float>>(FAnimNode_BlendListByBool::StaticStruct(),&Blend->Node,TEXT("BlendTime"))={.14f,.14f};
    auto* History=Node<UAnimGraphNode_PoseSearchHistoryCollector>(Graph,0,0);
    auto* HistoryData=FindFProperty<FStructProperty>(History->GetClass(),TEXT("Node"))->ContainerPtrToValuePtr<FAnimNode_PoseSearchHistoryCollector>(History);
    HistoryData->PoseCount=12;HistoryData->SamplingInterval=1.f/30;HistoryData->CollectedBones.Reset();
    for(const TCHAR* Bone:{TEXT("pelvis"),TEXT("foot_l"),TEXT("foot_r")}){FBoneReference Ref;Ref.BoneName=Bone;HistoryData->CollectedBones.Add(Ref);}
    auto* Root=Node<UAnimGraphNode_Root>(Graph,350,0);
    bool Connected=Bind(Graph,TEXT("ActiveDatabase"),MM,TEXT("Database"),-200);
    Connected&=Bind(Graph,TEXT("AirSequence"),Air,TEXT("Sequence"),500);
    Connected&=Bind(Graph,TEXT("AirPoseTime"),Air,TEXT("ExplicitTime"),650);
    Connected&=Bind(Graph,TEXT("bUseAirPose"),Blend,TEXT("bActiveValue"),800);
    Connected&=Bind(Graph,TEXT("MotionTrajectory"),History,TEXT("TransformTrajectory"),950);
    Connected&=Connect(Graph,MM,TEXT("Pose"),Blend,TEXT("BlendPose_1"));
    Connected&=Connect(Graph,Air,TEXT("Pose"),Blend,TEXT("BlendPose_0"));
    Connected&=Connect(Graph,Blend,TEXT("Pose"),History,TEXT("Source"));
    Connected&=Connect(Graph,History,TEXT("Pose"),Root,TEXT("Result"));
    if(!Connected)return TEXT("ERROR: AnimGraph pin connections; see MM_SETUP log");
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Log;
    FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log);
    if(Log.NumErrors>0||BP->Status==BS_Error)return TEXT("ERROR: AnimBlueprint compilation; see compiler log");
    if(!Save(BP))return TEXT("ERROR: saving compiled AnimBlueprint");
    return Report+TEXT("ABP_ExplorerMotionMatching: compiled, trajectory + pose history + Motion Matching + air blend\nMM_SETUP_COMPLETE");
#else
    return TEXT("ERROR: content generation requires the Unreal Editor target");
#endif
}
