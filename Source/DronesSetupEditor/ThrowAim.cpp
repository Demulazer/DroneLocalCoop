// Editor-only graph authoring. All aiming, prediction and rendering execute in Blueprint.
#include "Engine/Blueprint.h"
#include "InputAction.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/StaticMeshActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_Self.h"
#include "K2Node_DynamicCast.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Components/SkeletalMeshComponent.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_GetArrayItem.h"
#include "K2Node_CallArrayFunction.h"
#include "K2Node_MakeArray.h"
#include "K2Node_EnhancedInputAction.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetArrayLibrary.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "UObject/SavePackage.h"
#include "AssetCompilingManager.h"

namespace ThrowAim
{
const auto* S=GetDefault<UEdGraphSchema_K2>();
void Save(UObject* O) { auto* P=O->GetOutermost(); P->MarkPackageDirty(); FSavePackageArgs A; A.TopLevelFlags=RF_Public|RF_Standalone; check(UPackage::SavePackage(P,O,*FPackageName::LongPackageNameToFilename(P->GetName(),FPackageName::GetAssetPackageExtension()),A)); }
void Compile(UBlueprint* BP) { FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP); FCompilerResultsLog Log; FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log); checkf(!Log.NumErrors,TEXT("Aim Blueprint errors: %d"),Log.NumErrors); Save(BP); }
FEdGraphPinType Type(FName Category, UObject* Object=nullptr) { FEdGraphPinType T; T.PinCategory=Category; T.PinSubCategoryObject=Object; return T; }
struct Graph
{
    UBlueprint* BP; UEdGraph* G;
    template<class T> T* Node() { auto* N=NewObject<T>(G); G->AddNode(N,false,false); N->CreateNewGuid(); return N; }
    UEdGraphPin* Pin(UEdGraphNode* N,const TCHAR* Name) { auto* P=N->FindPin(Name); checkf(P,TEXT("Missing aim pin %s on %s"),Name,*N->GetNodeTitle(ENodeTitleType::FullTitle).ToString()); return P; }
    void Link(UEdGraphNode* A,const TCHAR* AP,UEdGraphNode* B,const TCHAR* BPn) { checkf(S->TryCreateConnection(Pin(A,AP),Pin(B,BPn)),TEXT("Aim connection %s -> %s failed"),AP,BPn); }
    void Exec(UEdGraphNode* A,UEdGraphNode* B) { Link(A,TEXT("then"),B,TEXT("execute")); }
    void Val(UEdGraphNode* N,const TCHAR* Name,const TCHAR* V) { S->TrySetDefaultValue(*Pin(N,Name),V); }
    UK2Node_CallFunction* Call(UClass* C,const TCHAR* Name) { auto* N=Node<UK2Node_CallFunction>(); auto* F=C->FindFunctionByName(Name); checkf(F,TEXT("Missing aim function %s"),Name); N->SetFromFunction(F); N->AllocateDefaultPins(); return N; }
    UK2Node_CallFunction* Own(const TCHAR* Name) { return Call(BP->SkeletonGeneratedClass,Name); }
    UK2Node_CallFunction* Math(const TCHAR* Name) { return Call(UKismetMathLibrary::StaticClass(),Name); }
    UK2Node_CustomEvent* Event(const TCHAR* Name) { auto* N=Node<UK2Node_CustomEvent>(); N->CustomFunctionName=Name; N->AllocateDefaultPins(); return N; }
    UK2Node_VariableGet* Get(const TCHAR* Name) { auto* N=Node<UK2Node_VariableGet>(); N->VariableReference.SetSelfMember(Name); N->AllocateDefaultPins(); return N; }
    UK2Node_VariableGet* External(const TCHAR* Name,UClass* Class,UEdGraphNode* Target,const TCHAR* TargetPin) { auto* N=Node<UK2Node_VariableGet>(); N->VariableReference.SetExternalMember(Name,Class); N->AllocateDefaultPins(); Link(Target,TargetPin,N,TEXT("self")); return N; }
    UK2Node_VariableSet* Set(const TCHAR* Name) { auto* N=Node<UK2Node_VariableSet>(); N->VariableReference.SetSelfMember(Name); N->AllocateDefaultPins(); return N; }
    UK2Node_IfThenElse* Branch() { auto* N=Node<UK2Node_IfThenElse>(); N->AllocateDefaultPins(); return N; }
    UK2Node_CallFunction* Scale(UEdGraphNode* V,const TCHAR* VP,UEdGraphNode* F,const TCHAR* FP) { auto* N=Math(TEXT("Multiply_VectorFloat")); Link(V,VP,N,TEXT("A")); Link(F,FP,N,TEXT("B")); return N; }
    UK2Node_CallFunction* VectorOp(const TCHAR* Name,UEdGraphNode* A,const TCHAR* AP,UEdGraphNode* B,const TCHAR* BPn) { auto* N=Math(Name); Link(A,AP,N,TEXT("A")); Link(B,BPn,N,TEXT("B")); return N; }
    void Layout() { int I=0; for(UEdGraphNode* N:G->Nodes) { N->NodePosX=(I%8)*650; N->NodePosY=(I/8)*650; ++I; } }
};
UMaterial* Material()
{
    const TCHAR* Path=TEXT("/Game/VehicleTemplate/Materials/M_ThrowAimRed");
    if(auto* Existing=LoadObject<UMaterial>(nullptr,Path)) return Existing;
    auto* M=NewObject<UMaterial>(CreatePackage(Path),TEXT("M_ThrowAimRed"),RF_Public|RF_Standalone);
    M->SetShadingModel(MSM_Unlit); M->bUsedWithInstancedStaticMeshes=true;
    auto* Color=NewObject<UMaterialExpressionConstant3Vector>(M); Color->Constant=FLinearColor(3,0,0);
    M->GetExpressionCollection().AddExpression(Color); M->GetEditorOnlyData()->EmissiveColor.Expression=Color;
    M->PostEditChange(); Save(M); return M;
}
void AddVisual(UBlueprint* BP,const TCHAR* Name,const TCHAR* MeshPath,UMaterial* M)
{
    if(BP->SimpleConstructionScript->FindSCSNode(Name)) return;
    auto* N=BP->SimpleConstructionScript->CreateNode(UInstancedStaticMeshComponent::StaticClass(),Name);
    auto* C=CastChecked<UInstancedStaticMeshComponent>(N->ComponentTemplate);
    C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,MeshPath)); C->SetMaterial(0,M);
    C->SetMobility(EComponentMobility::Movable); C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    C->SetOnlyOwnerSee(true); C->SetCastShadow(false); C->SetCanEverAffectNavigation(false);
    BP->SimpleConstructionScript->AddNode(N);
}
void Author()
{
    auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/VehicleTemplate/Blueprints/VehicleAdvPawn")); check(BP);
    if(BP->GeneratedClass->FindFunctionByName(TEXT("BeginThrowAim"))) return;
    auto* Cube=LoadObject<UBlueprint>(nullptr,TEXT("/Game/VehicleTemplate/Blueprints/BP_ThrowableCube")); check(Cube);
    auto Vec=Type(UEdGraphSchema_K2::PC_Struct,TBaseStructure<FVector>::Get());
    FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("IsThrowAiming"),Type(UEdGraphSchema_K2::PC_Boolean),TEXT("false"));
    FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("ThrowAimDirection"),Vec,TEXT("(X=1,Y=0,Z=0)"));
    AddVisual(BP,TEXT("ThrowAimBeam"),TEXT("/Engine/BasicShapes/Cube.Cube"),Material());
    AddVisual(BP,TEXT("ThrowAimDot"),TEXT("/Engine/BasicShapes/Sphere.Sphere"),Material());
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    auto* G=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("Throw Aiming"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(BP,G); Graph P{BP,G};
    auto* Begin=P.Event(TEXT("BeginThrowAim")); auto* Update=P.Event(TEXT("UpdateThrowAim"));
    auto* Release=P.Event(TEXT("ReleaseThrowAim")); auto* Cancel=P.Event(TEXT("CancelThrowAim"));
    auto* Calc=P.Event(TEXT("CalculateThrowAim")); auto* Draw=P.Event(TEXT("DrawThrowAimPath"));
    auto PointsType=Vec; PointsType.ContainerType=EPinContainerType::Array;
    Draw->CreateUserDefinedPin(TEXT("Points"),PointsType,EGPD_Output);
    auto* ShowHit=P.Event(TEXT("ShowThrowAimHit")); auto HitType=Type(UEdGraphSchema_K2::PC_Struct,TBaseStructure<FHitResult>::Get()); ShowHit->CreateUserDefinedPin(TEXT("Hit"),HitType,EGPD_Output);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    auto* Held=P.Get(TEXT("HeldThrowable")); auto* Valid=P.Call(UKismetSystemLibrary::StaticClass(),TEXT("IsValid")); P.Link(Held,TEXT("HeldThrowable"),Valid,TEXT("Object"));
    auto* Beam=P.Get(TEXT("ThrowAimBeam")); auto* Dot=P.Get(TEXT("ThrowAimDot")); auto* Aiming=P.Get(TEXT("IsThrowAiming"));
    auto Clear=[&](UEdGraphNode* Prev) { auto* A=P.Call(UInstancedStaticMeshComponent::StaticClass(),TEXT("ClearInstances")); P.Link(Beam,TEXT("ThrowAimBeam"),A,TEXT("self")); P.Exec(Prev,A); auto* B=P.Call(UInstancedStaticMeshComponent::StaticClass(),TEXT("ClearInstances")); P.Link(Dot,TEXT("ThrowAimDot"),B,TEXT("self")); P.Exec(A,B); return B; };
    auto* BeginBranch=P.Branch(); P.Exec(Begin,BeginBranch); P.Link(Valid,TEXT("ReturnValue"),BeginBranch,TEXT("Condition"));
    auto* SetAim=P.Set(TEXT("IsThrowAiming")); P.Val(SetAim,TEXT("IsThrowAiming"),TEXT("true")); P.Link(BeginBranch,TEXT("then"),SetAim,TEXT("execute"));
    auto* InitialUpdate=P.Own(TEXT("UpdateThrowAim")); P.Exec(SetAim,InitialUpdate);
    // Cancel never throws. Release requires a genuine earlier press with a held item.
    auto* ReleaseBranch=P.Branch(); P.Exec(Release,ReleaseBranch); P.Link(Aiming,TEXT("IsThrowAiming"),ReleaseBranch,TEXT("Condition"));
    auto* ReleaseSet=P.Set(TEXT("IsThrowAiming")); P.Val(ReleaseSet,TEXT("IsThrowAiming"),TEXT("false")); P.Link(ReleaseBranch,TEXT("then"),ReleaseSet,TEXT("execute"));
    auto* ReleasedClear=Clear(ReleaseSet); auto* Throw=P.Own(TEXT("TryThrow")); P.Exec(ReleasedClear,Throw);
    auto* CancelSet=P.Set(TEXT("IsThrowAiming")); P.Val(CancelSet,TEXT("IsThrowAiming"),TEXT("false")); P.Exec(Cancel,CancelSet); Clear(CancelSet);
    // The same car-based direction is evaluated for preview and again on release.
    auto* Start=P.Call(AActor::StaticClass(),TEXT("K2_GetActorLocation")); P.Link(Held,TEXT("HeldThrowable"),Start,TEXT("self"));
    auto* Origin=P.Call(AActor::StaticClass(),TEXT("K2_GetActorLocation")); auto* Forward=P.Call(AActor::StaticClass(),TEXT("GetActorForwardVector")); auto* Up=P.Call(AActor::StaticClass(),TEXT("GetActorUpVector"));
    auto* Speed=P.External(TEXT("ThrowSpeed"),Cube->GeneratedClass,Held,TEXT("HeldThrowable")); auto* Duration=P.External(TEXT("FlightDuration"),Cube->GeneratedClass,Held,TEXT("HeldThrowable"));
    auto* Range=P.Math(TEXT("Multiply_DoubleDouble")); P.Link(Speed,TEXT("ThrowSpeed"),Range,TEXT("A")); P.Link(Duration,TEXT("FlightDuration"),Range,TEXT("B"));
    auto* ForwardOffset=P.Scale(Forward,TEXT("ReturnValue"),Range,TEXT("ReturnValue"));
    auto* Offset=P.VectorOp(TEXT("Subtract_VectorVector"),Start,TEXT("ReturnValue"),Origin,TEXT("ReturnValue"));
    auto* Height=P.VectorOp(TEXT("Dot_VectorVector"),Offset,TEXT("ReturnValue"),Up,TEXT("ReturnValue"));
    auto* UpOffset=P.Scale(Up,TEXT("ReturnValue"),Height,TEXT("ReturnValue"));
    auto* Ahead=P.VectorOp(TEXT("Add_VectorVector"),Origin,TEXT("ReturnValue"),ForwardOffset,TEXT("ReturnValue"));
    auto* Target=P.VectorOp(TEXT("Add_VectorVector"),Ahead,TEXT("ReturnValue"),UpOffset,TEXT("ReturnValue"));
    auto* Unit=P.Math(TEXT("GetDirectionUnitVector")); P.Link(Start,TEXT("ReturnValue"),Unit,TEXT("From")); P.Link(Target,TEXT("ReturnValue"),Unit,TEXT("To"));
    auto* Direction=P.Set(TEXT("ThrowAimDirection")); P.Exec(Calc,Direction); P.Link(Unit,TEXT("ReturnValue"),Direction,TEXT("ThrowAimDirection"));
    auto* AimDir=P.Get(TEXT("ThrowAimDirection")); auto* Velocity=P.Scale(AimDir,TEXT("ThrowAimDirection"),Speed,TEXT("ThrowSpeed"));
    // Build prediction as two pieces: a weightless straight flight, then gravity.
    auto* UB=P.Branch(); P.Exec(Update,UB); P.Link(Aiming,TEXT("IsThrowAiming"),UB,TEXT("Condition"));
    auto* UV=P.Branch(); P.Link(UB,TEXT("then"),UV,TEXT("execute")); P.Link(Valid,TEXT("ReturnValue"),UV,TEXT("Condition"));
    auto* InvalidCancel=P.Own(TEXT("CancelThrowAim")); P.Link(UV,TEXT("else"),InvalidCancel,TEXT("execute"));
    auto* Recalc=P.Own(TEXT("CalculateThrowAim")); P.Link(UV,TEXT("then"),Recalc,TEXT("execute")); auto* Clean=Clear(Recalc);
    auto* Self=P.Node<UK2Node_Self>(); Self->AllocateDefaultPins();
    auto* Ignore=P.Node<UK2Node_MakeArray>(); Ignore->AllocateDefaultPins(); Ignore->AddInputPin();

    auto Predict=[&]() { auto* N=P.Call(UGameplayStatics::StaticClass(),TEXT("Blueprint_PredictProjectilePath_ByTraceChannel")); P.Link(Velocity,TEXT("ReturnValue"),N,TEXT("LaunchVelocity")); P.Link(Ignore,TEXT("Array"),N,TEXT("ActorsToIgnore")); if(P.Pin(Ignore,TEXT("[0]"))->LinkedTo.Num()==0) { P.Link(Self,TEXT("self"),Ignore,TEXT("[0]")); P.Link(Held,TEXT("HeldThrowable"),Ignore,TEXT("[1]")); } P.Val(N,TEXT("bTracePath"),TEXT("true")); P.Val(N,TEXT("ProjectileRadius"),TEXT("20")); P.Val(N,TEXT("TraceChannel"),TEXT("ECC_Visibility")); P.Val(N,TEXT("SimFrequency"),TEXT("60")); P.Val(N,TEXT("DrawDebugType"),TEXT("None")); return N; };
    auto* First=Predict(); P.Exec(Clean,First); P.Link(Start,TEXT("ReturnValue"),First,TEXT("StartPos")); P.Link(Duration,TEXT("FlightDuration"),First,TEXT("MaxSimTime")); P.Val(First,TEXT("OverrideGravityZ"),TEXT("-0.00001"));
    auto* DrawFirst=P.Own(TEXT("DrawThrowAimPath")); P.Exec(First,DrawFirst); P.Link(First,TEXT("OutPathPositions"),DrawFirst,TEXT("Points"));
    auto* HitFirst=P.Branch(); P.Exec(DrawFirst,HitFirst); P.Link(First,TEXT("ReturnValue"),HitFirst,TEXT("Condition"));
    auto* ShowFirst=P.Own(TEXT("ShowThrowAimHit")); P.Link(HitFirst,TEXT("then"),ShowFirst,TEXT("execute")); P.Link(First,TEXT("OutHit"),ShowFirst,TEXT("Hit"));
    auto* Second=Predict(); P.Link(HitFirst,TEXT("else"),Second,TEXT("execute")); P.Link(First,TEXT("OutLastTraceDestination"),Second,TEXT("StartPos")); P.Val(Second,TEXT("MaxSimTime"),TEXT("3"));
    auto* DrawSecond=P.Own(TEXT("DrawThrowAimPath")); P.Exec(Second,DrawSecond); P.Link(Second,TEXT("OutPathPositions"),DrawSecond,TEXT("Points"));
    auto* HitSecond=P.Branch(); P.Exec(DrawSecond,HitSecond); P.Link(Second,TEXT("ReturnValue"),HitSecond,TEXT("Condition"));
    auto* ShowSecond=P.Own(TEXT("ShowThrowAimHit")); P.Link(HitSecond,TEXT("then"),ShowSecond,TEXT("execute")); P.Link(Second,TEXT("OutHit"),ShowSecond,TEXT("Hit"));
    // Render real emissive meshes, visible only in the owning player's view.
    auto* Length=P.Node<UK2Node_CallArrayFunction>(); Length->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(TEXT("Array_Length"))); Length->AllocateDefaultPins(); P.Link(Draw,TEXT("Points"),Length,TEXT("TargetArray"));
    auto* Last=P.Math(TEXT("Subtract_IntInt")); P.Link(Length,TEXT("ReturnValue"),Last,TEXT("A")); P.Val(Last,TEXT("B"),TEXT("2"));
    auto* MacroBP=LoadObject<UBlueprint>(nullptr,TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros")); check(MacroBP);
    UEdGraph* ForGraph=nullptr; for(UEdGraph* MG:MacroBP->MacroGraphs) if(MG->GetName()==TEXT("ForLoop")) ForGraph=MG; check(ForGraph);
    auto* Loop=P.Node<UK2Node_MacroInstance>(); Loop->SetMacroGraph(ForGraph); Loop->AllocateDefaultPins(); P.Exec(Draw,Loop); P.Link(Last,TEXT("ReturnValue"),Loop,TEXT("LastIndex"));
    auto* Next=P.Math(TEXT("Add_IntInt")); P.Link(Loop,TEXT("Index"),Next,TEXT("A")); P.Val(Next,TEXT("B"),TEXT("1"));
    auto Item=[&](UEdGraphNode* Index,const TCHAR* IP) { auto* N=P.Node<UK2Node_GetArrayItem>(); N->AllocateDefaultPins(); P.Link(Draw,TEXT("Points"),N,TEXT("Array")); P.Link(Index,IP,N,TEXT("Dimension 1")); return N; };
    auto* A=Item(Loop,TEXT("Index")); auto* B=Item(Next,TEXT("ReturnValue"));
    auto* Delta=P.VectorOp(TEXT("Subtract_VectorVector"),B,TEXT("Output"),A,TEXT("Output"));
    auto* Rot=P.Math(TEXT("MakeRotFromX")); P.Link(Delta,TEXT("ReturnValue"),Rot,TEXT("X"));
    auto* Mid=P.Math(TEXT("VLerp")); P.Link(A,TEXT("Output"),Mid,TEXT("A")); P.Link(B,TEXT("Output"),Mid,TEXT("B")); P.Val(Mid,TEXT("Alpha"),TEXT("0.5"));
    auto* Size=P.Math(TEXT("VSize")); P.Link(Delta,TEXT("ReturnValue"),Size,TEXT("A"));
    auto* Scaled=P.Math(TEXT("Multiply_DoubleDouble")); P.Link(Size,TEXT("ReturnValue"),Scaled,TEXT("A")); P.Val(Scaled,TEXT("B"),TEXT("0.01"));
    auto* Scale=P.Math(TEXT("MakeVector")); P.Link(Scaled,TEXT("ReturnValue"),Scale,TEXT("X")); P.Val(Scale,TEXT("Y"),TEXT("0.015")); P.Val(Scale,TEXT("Z"),TEXT("0.015"));
    auto* Transform=P.Math(TEXT("MakeTransform")); P.Link(Mid,TEXT("ReturnValue"),Transform,TEXT("Location")); P.Link(Rot,TEXT("ReturnValue"),Transform,TEXT("Rotation")); P.Link(Scale,TEXT("ReturnValue"),Transform,TEXT("Scale"));
    auto* Add=P.Call(UInstancedStaticMeshComponent::StaticClass(),TEXT("AddInstance")); P.Link(Loop,TEXT("LoopBody"),Add,TEXT("execute")); P.Link(Beam,TEXT("ThrowAimBeam"),Add,TEXT("self")); P.Link(Transform,TEXT("ReturnValue"),Add,TEXT("InstanceTransform")); P.Val(Add,TEXT("bWorldSpace"),TEXT("true"));
    auto* BreakHit=P.Call(UGameplayStatics::StaticClass(),TEXT("BreakHitResult")); P.Link(ShowHit,TEXT("Hit"),BreakHit,TEXT("Hit"));
    auto* DotTransform=P.Math(TEXT("MakeTransform")); P.Link(BreakHit,TEXT("ImpactPoint"),DotTransform,TEXT("Location")); P.Val(DotTransform,TEXT("Scale"),TEXT("(X=0.08,Y=0.08,Z=0.08)"));
    auto* AddDot=P.Call(UInstancedStaticMeshComponent::StaticClass(),TEXT("AddInstance")); P.Exec(ShowHit,AddDot); P.Link(Dot,TEXT("ThrowAimDot"),AddDot,TEXT("self")); P.Link(DotTransform,TEXT("ReturnValue"),AddDot,TEXT("InstanceTransform")); P.Val(AddDot,TEXT("bWorldSpace"),TEXT("true"));
    // Wire Enhanced Input's Started/Triggered/Completed/Canceled separately.
    for(UEdGraph* Page:BP->UbergraphPages)
    {
        TArray<UEdGraphNode*> Nodes; for(UEdGraphNode* N:Page->Nodes) Nodes.Add(N); Graph Old{BP,Page};
        for(auto* N:Nodes)
        {
            if(auto* Input=Cast<UK2Node_EnhancedInputAction>(N); Input && Input->InputAction && Input->InputAction->GetName()==TEXT("IA_Throw"))
            {
                for(auto Pair : {TPair<const TCHAR*,const TCHAR*>(TEXT("Started"),TEXT("BeginThrowAim")),{TEXT("Triggered"),TEXT("UpdateThrowAim")},{TEXT("Completed"),TEXT("ReleaseThrowAim")},{TEXT("Canceled"),TEXT("CancelThrowAim")}})
                { Old.Pin(Input,Pair.Key)->BreakAllPinLinks(); auto* F=Old.Own(Pair.Value); Old.Link(Input,Pair.Key,F,TEXT("execute")); }
            }
            if(auto* Event=Cast<UK2Node_CustomEvent>(N); Event && Event->CustomFunctionName==TEXT("TryThrow"))
            {
                auto* Then=Old.Pin(Event,TEXT("then")); auto Links=Then->LinkedTo; Then->BreakAllPinLinks();
                // Calculate only after the existing held-item validity branch succeeds.
                check(Links.Num()==1); auto* Branch=CastChecked<UK2Node_IfThenElse>(Links[0]->GetOwningNode());
                check(S->TryCreateConnection(Then,Links[0]));
                auto* Success=Old.Pin(Branch,TEXT("then")); auto Dest=Success->LinkedTo; Success->BreakAllPinLinks();
                auto* F=Old.Own(TEXT("CalculateThrowAim")); Old.Link(Branch,TEXT("then"),F,TEXT("execute")); for(auto* L:Dest) check(S->TryCreateConnection(Old.Pin(F,TEXT("then")),L));
            }
            if(auto* F=Cast<UK2Node_CallFunction>(N); F && F->FunctionReference.GetMemberName()==TEXT("Launch"))
            { Old.Pin(F,TEXT("Direction"))->BreakAllPinLinks(); auto* Dir=Old.Get(TEXT("ThrowAimDirection")); Old.Link(Dir,TEXT("ThrowAimDirection"),F,TEXT("Direction")); }
        }
        if(Page==G || Page->GetName()==TEXT("Pickup and Throw")) Old.Layout();
    }
    Compile(BP);
    for(const TCHAR* Path:{TEXT("/Game/VehicleTemplate/Blueprints/SportsCar/SportsCar_Pawn"),TEXT("/Game/VehicleTemplate/Blueprints/OffroadCar/OffroadCar_Pawn")}) Compile(LoadObject<UBlueprint>(nullptr,Path));
}
}
namespace ThrowAim
{
void GuardThrowerCollision()
{
    // Six free axes preserve physical flight; only the thrower/cube body pair is excluded.
    auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/VehicleTemplate/Blueprints/BP_ThrowableCube"));
    if(BP->SimpleConstructionScript->FindSCSNode(TEXT("ThrowOwnerCollisionGuard")))
    {
        bool Changed=false;
        for(UEdGraph* G:BP->UbergraphPages)
        {
            Graph P{BP,G}; TArray<UEdGraphNode*> Nodes; for(UEdGraphNode* N:G->Nodes) Nodes.Add(N);
            for(auto* N:Nodes) if(auto* F=Cast<UK2Node_CallFunction>(N); F && F->FunctionReference.GetMemberName()==TEXT("SetConstrainedComponents") && P.Pin(F,TEXT("BoneName2"))->LinkedTo.Num()==0)
            {
                auto* Body=P.Get(TEXT("ThrowerBody")); auto* Cast=P.Node<UK2Node_DynamicCast>(); Cast->TargetType=USkeletalMeshComponent::StaticClass(); Cast->SetPurity(true); Cast->AllocateDefaultPins(); P.Link(Body,TEXT("ThrowerBody"),Cast,TEXT("Object"));
                auto* Bone=P.Call(USkeletalMeshComponent::StaticClass(),TEXT("GetBoneName")); check(S->TryCreateConnection(Cast->GetCastResultPin(),P.Pin(Bone,TEXT("self")))); P.Val(Bone,TEXT("BoneIndex"),TEXT("0")); P.Link(Bone,TEXT("ReturnValue"),F,TEXT("BoneName2")); Changed=true;
            }
            P.Layout();
        }
        if(Changed) Compile(BP); return;
    }
    FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("ThrowerBody"),Type(UEdGraphSchema_K2::PC_Object,UPrimitiveComponent::StaticClass()));
    auto* Node=BP->SimpleConstructionScript->CreateNode(UPhysicsConstraintComponent::StaticClass(),TEXT("ThrowOwnerCollisionGuard"));
    auto* Component=CastChecked<UPhysicsConstraintComponent>(Node->ComponentTemplate);
    Component->SetLinearXLimit(LCM_Free,0); Component->SetLinearYLimit(LCM_Free,0); Component->SetLinearZLimit(LCM_Free,0);
    Component->SetAngularSwing1Limit(ACM_Free,0); Component->SetAngularSwing2Limit(ACM_Free,0); Component->SetAngularTwistLimit(ACM_Free,0); Component->SetDisableCollision(true);
    BP->SimpleConstructionScript->AddNode(Node); FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    for(UEdGraph* G:BP->UbergraphPages)
    {
        Graph P{BP,G}; TArray<UEdGraphNode*> Nodes; for(UEdGraphNode* N:G->Nodes) Nodes.Add(N);
        auto* Guard=P.Get(TEXT("ThrowOwnerCollisionGuard")); auto* Mesh=P.Get(TEXT("ThrowableMesh")); auto* Body=P.Get(TEXT("ThrowerBody"));
        for(auto* N:Nodes)
        {
            if(auto* Event=Cast<UK2Node_CustomEvent>(N); Event && Event->CustomFunctionName==TEXT("Pickup"))
            {
                auto* Then=P.Pin(Event,TEXT("then")); auto Links=Then->LinkedTo; Then->BreakAllPinLinks();
                auto* Break=P.Call(UPhysicsConstraintComponent::StaticClass(),TEXT("BreakConstraint")); P.Link(Guard,TEXT("ThrowOwnerCollisionGuard"),Break,TEXT("self")); P.Exec(Event,Break);
                auto* Root=P.Call(AActor::StaticClass(),TEXT("K2_GetRootComponent")); P.Link(Event,TEXT("Holder"),Root,TEXT("self"));
                auto* Cast=P.Node<UK2Node_DynamicCast>(); Cast->TargetType=UPrimitiveComponent::StaticClass(); Cast->SetPurity(true); Cast->AllocateDefaultPins(); P.Link(Root,TEXT("ReturnValue"),Cast,TEXT("Object"));
                auto* Set=P.Set(TEXT("ThrowerBody")); P.Exec(Break,Set); check(S->TryCreateConnection(Cast->GetCastResultPin(),P.Pin(Set,TEXT("ThrowerBody"))));
                for(auto* L:Links) check(S->TryCreateConnection(P.Pin(Set,TEXT("then")),L));
            }
            if(auto* Sim=Cast<UK2Node_CallFunction>(N); Sim && Sim->FunctionReference.GetMemberName()==TEXT("SetSimulatePhysics") && P.Pin(Sim,TEXT("bSimulate"))->DefaultValue==TEXT("true"))
            {
                auto* Then=P.Pin(Sim,TEXT("then")); auto Links=Then->LinkedTo; Then->BreakAllPinLinks();
                auto* Bind=P.Call(UPhysicsConstraintComponent::StaticClass(),TEXT("SetConstrainedComponents")); P.Exec(Sim,Bind); P.Link(Guard,TEXT("ThrowOwnerCollisionGuard"),Bind,TEXT("self")); P.Link(Mesh,TEXT("ThrowableMesh"),Bind,TEXT("Component1")); P.Link(Body,TEXT("ThrowerBody"),Bind,TEXT("Component2"));
                for(auto* L:Links) check(S->TryCreateConnection(P.Pin(Bind,TEXT("then")),L));
            }
        }
        P.Layout();
    }
    Compile(BP); GuardThrowerCollision();
}
void GuardThrowerWheels()
{
    auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/VehicleTemplate/Blueprints/BP_ThrowableCube"));
    if(BP->SimpleConstructionScript->FindSCSNode(TEXT("ThrowWheelGuardFR"))) return;
    for(const TCHAR* Suffix:{TEXT("FR"),TEXT("FL"),TEXT("BR"),TEXT("BL")})
    {
        const FString Name=FString(TEXT("ThrowWheelGuard"))+Suffix;
        auto* N=BP->SimpleConstructionScript->CreateNode(UPhysicsConstraintComponent::StaticClass(),*Name); auto* C=CastChecked<UPhysicsConstraintComponent>(N->ComponentTemplate);
        C->SetLinearXLimit(LCM_Free,0); C->SetLinearYLimit(LCM_Free,0); C->SetLinearZLimit(LCM_Free,0); C->SetAngularSwing1Limit(ACM_Free,0); C->SetAngularSwing2Limit(ACM_Free,0); C->SetAngularTwistLimit(ACM_Free,0); C->SetDisableCollision(true);
        BP->SimpleConstructionScript->AddNode(N);
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    for(UEdGraph* G:BP->UbergraphPages)
    {
        Graph P{BP,G}; UK2Node_CallFunction* MainBind=nullptr; UK2Node_CallFunction* MainBreak=nullptr;
        for(UEdGraphNode* N:G->Nodes) if(auto* F=Cast<UK2Node_CallFunction>(N))
        {
            if(F->FunctionReference.GetMemberName()==TEXT("SetConstrainedComponents")) MainBind=F;
            if(F->FunctionReference.GetMemberName()==TEXT("BreakConstraint")) MainBreak=F;
        }
        if(!MainBind || !MainBreak) continue;
        auto* Body=P.Get(TEXT("ThrowerBody")); auto* Mesh=P.Get(TEXT("ThrowableMesh"));
        auto* Cast=P.Node<UK2Node_DynamicCast>(); Cast->TargetType=USkeletalMeshComponent::StaticClass(); Cast->SetPurity(true); Cast->AllocateDefaultPins(); P.Link(Body,TEXT("ThrowerBody"),Cast,TEXT("Object"));
        for(const TCHAR* Suffix:{TEXT("FR"),TEXT("FL"),TEXT("BR"),TEXT("BL")})
        {
            const FString Name=FString(TEXT("ThrowWheelGuard"))+Suffix; auto* Guard=P.Get(*Name);
            auto* Index=P.Call(USkeletalMeshComponent::StaticClass(),TEXT("GetBoneIndex")); check(S->TryCreateConnection(Cast->GetCastResultPin(),P.Pin(Index,TEXT("self")))); P.Val(Index,TEXT("BoneName"),*(FString(TEXT("Phys_Wheel_"))+Suffix));
            auto* Exists=P.Math(TEXT("NotEqual_IntInt")); P.Link(Index,TEXT("ReturnValue"),Exists,TEXT("A")); P.Val(Exists,TEXT("B"),TEXT("-1"));
            auto* Bone=P.Math(TEXT("SelectName")); P.Val(Bone,TEXT("A"),*(FString(TEXT("Phys_Wheel_"))+Suffix)); P.Val(Bone,TEXT("B"),*(FString(TEXT("PhysWheel_"))+Suffix)); P.Link(Exists,TEXT("ReturnValue"),Bone,TEXT("bPickA"));
            auto* Then=P.Pin(MainBind,TEXT("then")); auto BindLinks=Then->LinkedTo; Then->BreakAllPinLinks();
            auto* Bind=P.Call(UPhysicsConstraintComponent::StaticClass(),TEXT("SetConstrainedComponents")); P.Exec(MainBind,Bind); P.Link(Guard,*Name,Bind,TEXT("self")); P.Link(Mesh,TEXT("ThrowableMesh"),Bind,TEXT("Component1")); P.Link(Body,TEXT("ThrowerBody"),Bind,TEXT("Component2")); P.Link(Bone,TEXT("ReturnValue"),Bind,TEXT("BoneName2"));
            for(auto* L:BindLinks) check(S->TryCreateConnection(P.Pin(Bind,TEXT("then")),L)); MainBind=Bind;
            auto* BreakThen=P.Pin(MainBreak,TEXT("then")); auto BreakLinks=BreakThen->LinkedTo; BreakThen->BreakAllPinLinks();
            auto* Break=P.Call(UPhysicsConstraintComponent::StaticClass(),TEXT("BreakConstraint")); P.Exec(MainBreak,Break); P.Link(Guard,*Name,Break,TEXT("self")); for(auto* L:BreakLinks) check(S->TryCreateConnection(P.Pin(Break,TEXT("then")),L)); MainBreak=Break;
        }
        P.Layout();
    }
    Compile(BP);
}
void CompleteImpactLine()
{
    auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/VehicleTemplate/Blueprints/VehicleAdvPawn"));
    for(UEdGraph* G:BP->UbergraphPages) if(G->GetName()==TEXT("Throw Aiming"))
    {
        Graph P{BP,G}; UK2Node_CustomEvent* Event=nullptr; UK2Node_CallFunction* Break=nullptr;
        for(UEdGraphNode* N:G->Nodes)
        {
            if(N->NodeComment==TEXT("Aim contact connector")) return;
            if(auto* E=Cast<UK2Node_CustomEvent>(N); E && E->CustomFunctionName==TEXT("ShowThrowAimHit")) Event=E;
            if(auto* F=Cast<UK2Node_CallFunction>(N); F && F->FunctionReference.GetMemberName()==TEXT("BreakHitResult")) Break=F;
        }
        check(Event && Break);
        auto* Points=P.Node<UK2Node_MakeArray>(); Points->AllocateDefaultPins(); Points->AddInputPin();
        auto* Draw=P.Own(TEXT("DrawThrowAimPath")); Draw->NodeComment=TEXT("Aim contact connector");
        P.Link(Points,TEXT("Array"),Draw,TEXT("Points")); P.Link(Break,TEXT("Location"),Points,TEXT("[0]")); P.Link(Break,TEXT("ImpactPoint"),Points,TEXT("[1]"));
        auto* Then=P.Pin(Event,TEXT("then")); auto Links=Then->LinkedTo; Then->BreakAllPinLinks(); P.Exec(Event,Draw); for(auto* L:Links) check(S->TryCreateConnection(P.Pin(Draw,TEXT("then")),L));
        P.Layout(); Compile(BP);
        for(const TCHAR* Path:{TEXT("/Game/VehicleTemplate/Blueprints/SportsCar/SportsCar_Pawn"),TEXT("/Game/VehicleTemplate/Blueprints/OffroadCar/OffroadCar_Pawn")}) Compile(LoadObject<UBlueprint>(nullptr,Path));
    }
}
void Verify(const TCHAR* CarPath);
}
int32 ConfigureThrowAim()
{
    ThrowAim::GuardThrowerCollision(); ThrowAim::GuardThrowerWheels(); ThrowAim::Author(); ThrowAim::CompleteImpactLine(); ThrowAim::Verify(TEXT("/Game/VehicleTemplate/Blueprints/SportsCar/SportsCar_Pawn")); ThrowAim::Verify(TEXT("/Game/VehicleTemplate/Blueprints/OffroadCar/OffroadCar_Pawn"));
    UE_LOG(LogTemp,Display,TEXT("THROW AIM: authoring and compile PASS")); return 0;
}



namespace ThrowAim
{
void Verify(const TCHAR* CarPath)
{
    auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/VehicleTemplate/Blueprints/VehicleAdvPawn"));
    auto* Car=LoadObject<UBlueprint>(nullptr,CarPath);
    auto* Cube=LoadObject<UBlueprint>(nullptr,TEXT("/Game/VehicleTemplate/Blueprints/BP_ThrowableCube"));
    FAssetCompilingManager::Get().FinishAllCompilation();
    auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("ThrowAimVerification")); GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W); W->InitializeActorsForPlay(FURL());
    FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* P1=W->SpawnActor<APawn>(Car->GeneratedClass,FVector(0,0,500),FRotator::ZeroRotator,SP);
    auto* P2=W->SpawnActor<APawn>(Car->GeneratedClass,FVector(0,2000,500),FRotator(0,90,0),SP);
    auto* A=W->SpawnActor<AActor>(Cube->GeneratedClass,FVector(200,0,500),FRotator::ZeroRotator,SP);
    auto* B=W->SpawnActor<AActor>(Cube->GeneratedClass,FVector(0,2200,500),FRotator::ZeroRotator,SP);
    auto Invoke=[](UObject* O,const TCHAR* Name) { O->ProcessEvent(O->FindFunctionChecked(Name),nullptr); };
    auto* Held=FindFProperty<FObjectProperty>(BP->GeneratedClass,TEXT("HeldThrowable")); auto* Aiming=FindFProperty<FBoolProperty>(BP->GeneratedClass,TEXT("IsThrowAiming")); auto* Direction=FindFProperty<FStructProperty>(BP->GeneratedClass,TEXT("ThrowAimDirection"));
    auto GetDirection=[&](UObject* O) { return *Direction->ContainerPtrToValuePtr<FVector>(O); };
    auto GetISM=[&](UObject* O,const TCHAR* Name) { return CastChecked<UInstancedStaticMeshComponent>(FindFProperty<FObjectProperty>(BP->GeneratedClass,Name)->GetObjectPropertyValue_InContainer(O)); };
    auto* Beam=GetISM(P1,TEXT("ThrowAimBeam")); auto* Dot=GetISM(P1,TEXT("ThrowAimDot"));
    check(Beam->bOnlyOwnerSee && Dot->bOnlyOwnerSee && Beam->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
    Invoke(P1,TEXT("BeginThrowAim")); check(!Aiming->GetPropertyValue_InContainer(P1) && Beam->GetInstanceCount()==0);
    Invoke(P1,TEXT("ReleaseThrowAim"));
    Invoke(P1,TEXT("TryPickup")); Invoke(P2,TEXT("TryPickup")); check(Held->GetObjectPropertyValue_InContainer(P1)==A && Held->GetObjectPropertyValue_InContainer(P2)==B);
    Invoke(P1,TEXT("BeginThrowAim"));
    auto* M=CastChecked<UPrimitiveComponent>(A->GetRootComponent());
    check(Aiming->GetPropertyValue_InContainer(P1) && !M->IsSimulatingPhysics() && Held->GetObjectPropertyValue_InContainer(P1)==A);
    const FVector Expected=FVector(500,-230,0).GetSafeNormal();
    check(GetDirection(P1).Equals(Expected,.001)); check(Beam->GetInstanceCount()>25 && Dot->GetInstanceCount()==0);
    UE_LOG(LogTemp,Display,TEXT("AIM TEST diagonal direction=%s beam segments=%d"),*GetDirection(P1).ToString(),Beam->GetInstanceCount());
    A->SetActorRelativeRotation(FRotator(0,123,0)); Invoke(P1,TEXT("UpdateThrowAim")); check(GetDirection(P1).Equals(Expected,.001));
    // A solid target yields a dot on its surface and truncates the beam.
    auto* Obstacle=W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),A->GetActorLocation()+Expected*300,FRotator::ZeroRotator,SP);
    auto* OM=Obstacle->GetStaticMeshComponent(); OM->SetMobility(EComponentMobility::Movable); OM->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"))); OM->SetCollisionProfileName(TEXT("BlockAll"));
    Invoke(P1,TEXT("UpdateThrowAim")); checkf(Dot->GetInstanceCount()==1,TEXT("Expected red impact dot on obstacle"));
    FTransform HitDot; Dot->GetInstanceTransform(0,HitDot,true);
    check((HitDot.GetLocation()-Obstacle->GetActorLocation()).Size()<90);
    UE_LOG(LogTemp,Display,TEXT("AIM TEST impact dot=%s segments=%d"),*HitDot.GetLocation().ToString(),Beam->GetInstanceCount());
    Obstacle->SetActorLocation(FVector(10000,10000,10000)); Invoke(P1,TEXT("UpdateThrowAim")); check(Dot->GetInstanceCount()==0);
    // A second vehicle is a valid aim target, including its physics collision.
    auto* TargetMesh=CastChecked<UPrimitiveComponent>(P2->GetRootComponent());
    P2->SetActorRotation(FRotator::ZeroRotator,ETeleportType::TeleportPhysics); W->UpdateWorldComponents(false,false);
    P2->SetActorLocation(A->GetActorLocation()+Expected*400-(TargetMesh->Bounds.Origin-P2->GetActorLocation()),false,nullptr,ETeleportType::TeleportPhysics);
    Invoke(P1,TEXT("UpdateThrowAim")); checkf(Dot->GetInstanceCount()==1,TEXT("Vehicle is missing from aim collision query"));
    UE_LOG(LogTemp,Display,TEXT("AIM TEST second vehicle impact dot PASS"));
    P2->SetActorLocation(FVector(0,2000,500),false,nullptr,ETeleportType::TeleportPhysics); P2->SetActorRotation(FRotator(0,90,0),ETeleportType::TeleportPhysics);
    // Ground after the straight segment is part of the curved prediction.
    auto* Floor=W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),FVector(0,0,-10),FRotator::ZeroRotator,SP);
    auto* FM=Floor->GetStaticMeshComponent(); FM->SetMobility(EComponentMobility::Movable); FM->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"))); FM->SetWorldScale3D(FVector(100,100,.2)); FM->SetCollisionProfileName(TEXT("BlockAll"));
    Invoke(P1,TEXT("UpdateThrowAim")); check(Dot->GetInstanceCount()==1); Dot->GetInstanceTransform(0,HitDot,true); check(FMath::Abs(HitDot.GetLocation().Z)<2);
    UE_LOG(LogTemp,Display,TEXT("AIM TEST curved path ground dot=%s"),*HitDot.GetLocation().ToString());
    Floor->Destroy(); Invoke(P1,TEXT("UpdateThrowAim")); check(Dot->GetInstanceCount()==0);
    Invoke(P1,TEXT("CancelThrowAim")); check(!Aiming->GetPropertyValue_InContainer(P1) && Beam->GetInstanceCount()==0 && Held->GetObjectPropertyValue_InContainer(P1)==A);
    // Both players can aim independently. Rotating the car rotates the trajectory.
    Invoke(P1,TEXT("BeginThrowAim")); Invoke(P2,TEXT("BeginThrowAim"));
    check(GetDirection(P2).Equals(FRotator(0,90,0).RotateVector(Expected),.001));
    check(GetISM(P2,TEXT("ThrowAimBeam"))->GetInstanceCount()>0);
    A->SetActorRelativeRotation(FRotator::ZeroRotator);
    const FVector Start=A->GetActorLocation();
    W->SetBegunPlay(true); W->bShouldSimulatePhysics=true;
    Invoke(P1,TEXT("ReleaseThrowAim"));
    check(!Aiming->GetPropertyValue_InContainer(P1) && Beam->GetInstanceCount()==0 && Dot->GetInstanceCount()==0);
    check(!Held->GetObjectPropertyValue_InContainer(P1) && M->IsSimulatingPhysics());
    check(M->GetPhysicsLinearVelocity().Equals(Expected*1000,.1));
    auto* Guard=A->FindComponentByClass<UPhysicsConstraintComponent>();
    check(Guard && Guard->ConstraintInstance.IsValidConstraintInstance());
    check(Aiming->GetPropertyValue_InContainer(P2) && Held->GetObjectPropertyValue_InContainer(P2)==B);
    Invoke(P1,TEXT("ReleaseThrowAim")); // No double throw on duplicate release.
    for(int I=0;I<60;++I) { ++GFrameCounter; W->Tick(LEVELTICK_All,1.f/120.f); }
    const FVector Travel=A->GetActorLocation()-Start;
    UE_LOG(LogTemp,Display,TEXT("AIM TEST released travel=%s length=%.2f velocity=%s"),*Travel.ToString(),Travel.Size(),*M->GetPhysicsLinearVelocity().ToString());
    checkf(Travel.Equals(Expected*500,20),TEXT("Diagonal trajectory hit the thrower's car or diverged from preview"));
    Invoke(P2,TEXT("CancelThrowAim")); W->SetBegunPlay(false); W->DestroyWorld(false); GEngine->DestroyWorldContext(W);
    UE_LOG(LogTemp,Display,TEXT("THROW AIM VERIFY: car direction, cube rotation independence, hold, hit dot, cancel, tap/release, split-screen state, physics path PASS"));
}
}










namespace ThrowAim
{
void AuthorImpactPush()
{
    auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/VehicleTemplate/Blueprints/BP_ThrowableCube")); check(BP);
    if(FindFProperty<FDoubleProperty>(BP->GeneratedClass,TEXT("PushImpulse"))) return;
    auto Real=Type(UEdGraphSchema_K2::PC_Real); Real.PinSubCategory=UEdGraphSchema_K2::PC_Double;
    FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("PushImpulse"),Real,TEXT("3000000"));
    FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("CanApplyThrowPush"),Type(UEdGraphSchema_K2::PC_Boolean),TEXT("false"));
    FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("LastThrowDirection"),Type(UEdGraphSchema_K2::PC_Struct,TBaseStructure<FVector>::Get()),TEXT("(X=1,Y=0,Z=0)"));
    FBlueprintEditorUtils::SetBlueprintVariableCategory(BP,TEXT("PushImpulse"),nullptr,FText::FromString(TEXT("Throwing")));
    FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP,TEXT("PushImpulse"),nullptr,TEXT("ClampMin"),TEXT("0"));
    FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP,TEXT("PushImpulse"),nullptr,TEXT("ToolTip"),TEXT("Extra one-shot impulse along the throw direction (kg cm/s). 0 disables the extra push. Vehicle mass, brakes and ground friction still affect travel."));
    auto* Mesh=CastChecked<UPrimitiveComponent>(BP->SimpleConstructionScript->FindSCSNode(TEXT("ThrowableMesh"))->ComponentTemplate); Mesh->SetNotifyRigidBodyCollision(true);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    for(UEdGraph* Page:BP->UbergraphPages)
    {
        Graph P{BP,Page}; TArray<UEdGraphNode*> Nodes; for(UEdGraphNode* N:Page->Nodes) Nodes.Add(N);
        for(auto* N:Nodes) if(auto* E=Cast<UK2Node_CustomEvent>(N))
        {
            const bool Launch=E->CustomFunctionName==TEXT("Launch"); const bool Pickup=E->CustomFunctionName==TEXT("Pickup"); if(!Launch && !Pickup) continue;
            auto* Then=P.Pin(E,TEXT("then")); auto Links=Then->LinkedTo; Then->BreakAllPinLinks();
            auto* Arm=P.Set(TEXT("CanApplyThrowPush")); P.Val(Arm,TEXT("CanApplyThrowPush"),Launch?TEXT("true"):TEXT("false")); P.Exec(E,Arm);
            UEdGraphNode* Tail=Arm;
            if(Launch) { auto* Normal=P.Math(TEXT("Normal")); P.Link(E,TEXT("Direction"),Normal,TEXT("A")); auto* Dir=P.Set(TEXT("LastThrowDirection")); P.Link(Normal,TEXT("ReturnValue"),Dir,TEXT("LastThrowDirection")); P.Exec(Arm,Dir); Tail=Dir; }
            for(auto* L:Links) check(S->TryCreateConnection(P.Pin(Tail,TEXT("then")),L));
        }
        P.Layout();
    }
    auto* G=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("Throw Impact Push"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass()); FBlueprintEditorUtils::AddUbergraphPage(BP,G); Graph P{BP,G};
    auto* Hit=P.Node<UK2Node_Event>(); Hit->EventReference.SetExternalMember(TEXT("ReceiveHit"),AActor::StaticClass()); Hit->bOverrideFunction=true; Hit->AllocateDefaultPins();
    auto* Armed=P.Get(TEXT("CanApplyThrowPush")); auto* Check=P.Branch(); P.Exec(Hit,Check); P.Link(Armed,TEXT("CanApplyThrowPush"),Check,TEXT("Condition"));
    // Consume on the first blocking contact, including walls/ground, to prevent
    // repeated boosts from bouncing or driving into an old resting projectile.
    auto* Consume=P.Set(TEXT("CanApplyThrowPush")); P.Val(Consume,TEXT("CanApplyThrowPush"),TEXT("false")); P.Link(Check,TEXT("then"),Consume,TEXT("execute"));
    auto* Vehicle=LoadObject<UBlueprint>(nullptr,TEXT("/Game/VehicleTemplate/Blueprints/VehicleAdvPawn")); check(Vehicle);
    auto* Cast=P.Node<UK2Node_DynamicCast>(); Cast->TargetType=Vehicle->GeneratedClass; Cast->AllocateDefaultPins(); P.Exec(Consume,Cast); P.Link(Hit,TEXT("Other"),Cast,TEXT("Object"));
    auto* Root=P.Call(AActor::StaticClass(),TEXT("K2_GetRootComponent")); check(S->TryCreateConnection(Cast->GetCastResultPin(),P.Pin(Root,TEXT("self"))));
    auto* MeshCast=P.Node<UK2Node_DynamicCast>(); MeshCast->TargetType=USkeletalMeshComponent::StaticClass(); MeshCast->SetPurity(true); MeshCast->AllocateDefaultPins(); P.Link(Root,TEXT("ReturnValue"),MeshCast,TEXT("Object"));
    auto* OwnerBody=P.Get(TEXT("ThrowerBody")); auto* NotOwner=P.Math(TEXT("NotEqual_ObjectObject")); P.Link(Root,TEXT("ReturnValue"),NotOwner,TEXT("A")); P.Link(OwnerBody,TEXT("ThrowerBody"),NotOwner,TEXT("B"));
    auto* OtherCheck=P.Branch(); P.Link(Cast,TEXT("then"),OtherCheck,TEXT("execute")); P.Link(NotOwner,TEXT("ReturnValue"),OtherCheck,TEXT("Condition"));
    auto* Bone=P.Call(USkeletalMeshComponent::StaticClass(),TEXT("GetBoneName")); check(S->TryCreateConnection(MeshCast->GetCastResultPin(),P.Pin(Bone,TEXT("self")))); P.Val(Bone,TEXT("BoneIndex"),TEXT("0"));
    auto* Sim=P.Call(UPrimitiveComponent::StaticClass(),TEXT("IsSimulatingPhysics")); check(S->TryCreateConnection(MeshCast->GetCastResultPin(),P.Pin(Sim,TEXT("self")))); P.Link(Bone,TEXT("ReturnValue"),Sim,TEXT("BoneName"));
    auto* SimCheck=P.Branch(); P.Link(OtherCheck,TEXT("then"),SimCheck,TEXT("execute")); P.Link(Sim,TEXT("ReturnValue"),SimCheck,TEXT("Condition"));
    auto* Dir=P.Get(TEXT("LastThrowDirection")); auto* Force=P.Get(TEXT("PushImpulse")); auto* Impulse=P.Scale(Dir,TEXT("LastThrowDirection"),Force,TEXT("PushImpulse"));
    auto* Push=P.Call(UPrimitiveComponent::StaticClass(),TEXT("AddImpulse")); P.Link(SimCheck,TEXT("then"),Push,TEXT("execute")); check(S->TryCreateConnection(MeshCast->GetCastResultPin(),P.Pin(Push,TEXT("self")))); P.Link(Impulse,TEXT("ReturnValue"),Push,TEXT("Impulse")); P.Link(Bone,TEXT("ReturnValue"),Push,TEXT("BoneName")); P.Val(Push,TEXT("bVelChange"),TEXT("false"));
    P.Layout(); Compile(BP);
}
struct PushTestResult { float Mass; float ForwardVelocity; };
PushTestResult VerifyImpactPush(const TCHAR* CarPath,double Impulse)
{
    auto* Car=LoadObject<UBlueprint>(nullptr,CarPath); auto* Cube=LoadObject<UBlueprint>(nullptr,TEXT("/Game/VehicleTemplate/Blueprints/BP_ThrowableCube")); FAssetCompilingManager::Get().FinishAllCompilation();
    auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("ThrowPushVerification")); GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W); W->InitializeActorsForPlay(FURL());
    FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Owner=W->SpawnActor<APawn>(Car->GeneratedClass,FVector(-2000,0,1000),FRotator::ZeroRotator,SP);
    auto* Target=W->SpawnActor<APawn>(Car->GeneratedClass,FVector(500,0,1000),FRotator::ZeroRotator,SP);
    auto* Body=CastChecked<USkeletalMeshComponent>(Target->GetRootComponent()); Body->SetEnableGravity(false);
    auto* A=W->SpawnActor<AActor>(Cube->GeneratedClass,FVector(0,0,1000),FRotator::ZeroRotator,SP);
    auto* M=CastChecked<UPrimitiveComponent>(A->GetRootComponent());
    auto* Force=FindFProperty<FDoubleProperty>(Cube->GeneratedClass,TEXT("PushImpulse")); Force->SetPropertyValue_InContainer(A,Impulse);
    auto* Armed=FindFProperty<FBoolProperty>(Cube->GeneratedClass,TEXT("CanApplyThrowPush")); check(!Armed->GetPropertyValue_InContainer(A));
    struct { AActor* Holder; } Pickup{Owner}; A->ProcessEvent(A->FindFunctionChecked(TEXT("Pickup")),&Pickup);
    const FVector Direction=FVector(1,.2,0).GetSafeNormal(); const FVector Start=Body->Bounds.Origin-Direction*420;
    A->SetActorLocation(Start,false,nullptr,ETeleportType::TeleportPhysics);
    struct { FVector Direction; } Launch{Direction}; A->ProcessEvent(A->FindFunctionChecked(TEXT("Launch")),&Launch);
    check(Armed->GetPropertyValue_InContainer(A));
    W->SetBegunPlay(true); W->bShouldSimulatePhysics=true;
    for(int I=0;I<48;++I) { ++GFrameCounter; W->Tick(LEVELTICK_All,1.f/120.f); }
    checkf(!Armed->GetPropertyValue_InContainer(A),TEXT("Physical hit did not consume push"));
    const FVector V=Body->GetPhysicsLinearVelocity(Body->GetBoneName(0)); const float Along=FVector::DotProduct(V,Direction); const float Mass=Body->GetMass();
    UE_LOG(LogTemp,Display,TEXT("PUSH TEST car=%s impulse=%.0f mass=%.1f velocity=%s forward=%.2f"),CarPath,Impulse,Mass,*V.ToString(),Along);
    // A repeated hit notification must not queue the special push again.
    UFunction* Hit=A->FindFunctionChecked(TEXT("ReceiveHit")); TArray<uint8> Args; Args.SetNumZeroed(Hit->ParmsSize); FindFProperty<FObjectProperty>(Hit,TEXT("Other"))->SetObjectPropertyValue_InContainer(Args.GetData(),Target); A->ProcessEvent(Hit,Args.GetData());
    check(!Armed->GetPropertyValue_InContainer(A));
    for(int I=0;I<2;++I) { ++GFrameCounter; W->Tick(LEVELTICK_All,1.f/120.f); }
    checkf(FVector::DotProduct(Body->GetPhysicsLinearVelocity(),Direction)<Along+10,TEXT("Repeated hit applied another boost"));
    A->ProcessEvent(A->FindFunctionChecked(TEXT("Pickup")),&Pickup); check(!Armed->GetPropertyValue_InContainer(A)); A->ProcessEvent(A->FindFunctionChecked(TEXT("Launch")),&Launch); check(Armed->GetPropertyValue_InContainer(A));
    W->SetBegunPlay(false); W->DestroyWorld(false); GEngine->DestroyWorldContext(W); return {Mass,Along};
}
}
int32 ConfigureThrowPush()
{
    ThrowAim::AuthorImpactPush();
    auto* Cube=LoadObject<UBlueprint>(nullptr,TEXT("/Game/VehicleTemplate/Blueprints/BP_ThrowableCube"));
    const int32 PushIndex=FBlueprintEditorUtils::FindNewVariableIndex(Cube,TEXT("PushImpulse"));
    if(Cube->NewVariables[PushIndex].DefaultValue==TEXT("900000"))
    {
        Cube->NewVariables[PushIndex].DefaultValue=TEXT("3000000"); FindFProperty<FDoubleProperty>(Cube->GeneratedClass,TEXT("PushImpulse"))->SetPropertyValue_InContainer(Cube->GeneratedClass->GetDefaultObject(),3000000.0); ThrowAim::Compile(Cube);
    }
    for(const TCHAR* Path:{TEXT("/Game/VehicleTemplate/Blueprints/SportsCar/SportsCar_Pawn"),TEXT("/Game/VehicleTemplate/Blueprints/OffroadCar/OffroadCar_Pawn")})
    {
        auto Baseline=ThrowAim::VerifyImpactPush(Path,0); auto Boost=ThrowAim::VerifyImpactPush(Path,3000000);
        const float Expected=3000000.f/Boost.Mass;
        checkf(Boost.ForwardVelocity-Baseline.ForwardVelocity>Expected*.65f,TEXT("Additional directional push is too weak"));
        UE_LOG(LogTemp,Display,TEXT("PUSH VERIFY extra forward speed=%.1f cm/s expected impulse/mass=%.1f cm/s PASS"),Boost.ForwardVelocity-Baseline.ForwardVelocity,Expected);
    }
    return 0;
}


