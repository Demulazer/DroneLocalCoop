#include "BuildThrowablesCommandlet.h"
#include "Modules/ModuleManager.h"
#include "AssetToolsModule.h"
#include "Engine/Blueprint.h"
#include "Engine/StaticMesh.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_EnhancedInputAction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_Self.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_GetArrayItem.h"
#include "K2Node_MakeArray.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "FileHelpers.h"
#include "EngineUtils.h"
#include "UObject/SavePackage.h"
#include "Engine/Engine.h"
#include "Engine/StaticMeshActor.h"
#include "AssetCompilingManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, DronesSetupEditor)
namespace
{
const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
void Save(UObject* O)
{
    UPackage* P = O->GetOutermost(); P->MarkPackageDirty();
    FSavePackageArgs A; A.TopLevelFlags = RF_Public | RF_Standalone;
    check(UPackage::SavePackage(P, O, *FPackageName::LongPackageNameToFilename(P->GetName(), FPackageName::GetAssetPackageExtension()), A));
}
struct Graph
{
    UBlueprint* BP; UEdGraph* G; int32 Index = 0;
    template<class T> T* Node()
    {
        auto* N = NewObject<T>(G); G->AddNode(N, false, false); N->CreateNewGuid();
        N->NodePosX = (Index % 7) * 350; N->NodePosY = (Index / 7) * 350; ++Index; return N;
    }
    UEdGraphPin* Pin(UEdGraphNode* N, const TCHAR* Name)
    {
        auto* P = N->FindPin(Name); if (!P) { UE_LOG(LogTemp, Fatal, TEXT("Missing pin %s on %s"), Name, *N->GetNodeTitle(ENodeTitleType::FullTitle).ToString()); } return P;
    }
    void Link(UEdGraphNode* A, const TCHAR* AP, UEdGraphNode* B, const TCHAR* BPn)
    {
        checkf(Schema->TryCreateConnection(Pin(A, AP), Pin(B, BPn)), TEXT("Cannot connect %s to %s"), AP, BPn);
    }
    void Val(UEdGraphNode* N, const TCHAR* P, const TCHAR* V) { Schema->TrySetDefaultValue(*Pin(N,P), V); }
    UK2Node_CallFunction* Call(UClass* C, const TCHAR* F)
    {
        auto* N = Node<UK2Node_CallFunction>(); auto* Fn = C->FindFunctionByName(F); checkf(Fn,TEXT("Missing function %s"),F);
        N->SetFromFunction(Fn); N->AllocateDefaultPins(); return N;
    }
    UK2Node_CustomEvent* Event(const TCHAR* Name)
    {
        auto* N = Node<UK2Node_CustomEvent>(); N->CustomFunctionName = Name; N->AllocateDefaultPins(); return N;
    }
    UK2Node_VariableGet* Get(const TCHAR* Name)
    {
        auto* N = Node<UK2Node_VariableGet>(); N->VariableReference.SetSelfMember(Name); N->AllocateDefaultPins(); return N;
    }
    UK2Node_VariableSet* Set(const TCHAR* Name)
    {
        auto* N = Node<UK2Node_VariableSet>(); N->VariableReference.SetSelfMember(Name); N->AllocateDefaultPins(); return N;
    }
    UK2Node_Self* Self() { auto* N = Node<UK2Node_Self>(); N->AllocateDefaultPins(); return N; }
    void Exec(UEdGraphNode* A, UEdGraphNode* B) { Link(A,TEXT("then"),B,TEXT("execute")); }
};
FEdGraphPinType ObjectType(UClass* C) { FEdGraphPinType T; T.PinCategory = UEdGraphSchema_K2::PC_Object; T.PinSubCategoryObject = C; return T; }
void ResolveInput(UInputMappingContext* IMC)
{
    const auto Mappings=IMC->GetMappings();
    for(const auto& M:Mappings)
    {
        const FString Name=M.Action->GetName();
        const bool IsPickup=M.Key==EKeys::F || M.Key==EKeys::Gamepad_FaceButton_Bottom;
        const bool IsThrow=M.Key==EKeys::R || M.Key==EKeys::Gamepad_FaceButton_Left;
        if((IsPickup && Name!=TEXT("IA_Pickup")) || (IsThrow && Name!=TEXT("IA_Throw")))
        {
            FKey Replacement=M.Key==EKeys::F ? EKeys::G : M.Key==EKeys::R ? EKeys::Home : M.Key==EKeys::Gamepad_FaceButton_Bottom ? EKeys::Gamepad_FaceButton_Right : EKeys::Gamepad_FaceButton_Top;
            IMC->UnmapKey(M.Action,M.Key); IMC->MapKey(M.Action,Replacement);
            UE_LOG(LogTemp,Display,TEXT("INPUT conflict resolved: %s %s -> %s"),*Name,*M.Key.ToString(),*Replacement.ToString());
        }
    }
    Save(IMC);
}
void Compile(UBlueprint* B)
{
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(B); FCompilerResultsLog Log;
    FKismetEditorUtilities::CompileBlueprint(B, EBlueprintCompileOptions::None, &Log);
    checkf(Log.NumErrors == 0, TEXT("Blueprint compile failed: %s (%d errors)"), *B->GetName(), Log.NumErrors);
    Save(B);
}
UInputAction* Action(const TCHAR* Name, FKey A, FKey B, UInputMappingContext* IMC)
{
    FString Path = FString(TEXT("/Game/VehicleTemplate/Input/Actions/")) + Name;
    auto* IA = LoadObject<UInputAction>(nullptr, *Path);
    if (!IA) IA = NewObject<UInputAction>(CreatePackage(*Path), Name, RF_Public | RF_Standalone);
    IA->ValueType = EInputActionValueType::Boolean; IA->Triggers.Reset();
    IMC->UnmapAllKeysFromAction(IA); IMC->MapKey(IA,A); IMC->MapKey(IA,B); Save(IA); return IA;
}
int32 VerifyThrowables()
{
    ResolveInput(LoadObject<UInputMappingContext>(nullptr,TEXT("/Game/VehicleTemplate/Input/IMC_Default")));
    auto* Cube=LoadObject<UBlueprint>(nullptr,TEXT("/Game/VehicleTemplate/Blueprints/BP_ThrowableCube")); check(Cube);
    auto* Pawn=LoadObject<UBlueprint>(nullptr,TEXT("/Game/VehicleTemplate/Blueprints/VehicleAdvPawn")); check(Pawn);
    // Flying cubes temporarily use WorldDynamic, so the PhysicsBody pickup query
    // cannot take them while their latent flight completion is still pending.
    bool Patched=false;
    for(UEdGraph* G:Cube->UbergraphPages)
    {
        TArray<UEdGraphNode*> Nodes; for(UEdGraphNode* N:G->Nodes) Nodes.Add(N);
        bool HasObjectType=false;
        for(auto* N:Nodes) if(auto* F=Cast<UK2Node_CallFunction>(N)) HasObjectType |= F->FunctionReference.GetMemberName()==TEXT("SetCollisionObjectType");
        if(HasObjectType) continue;
        Graph C{Cube,G}; auto* Mesh=C.Get(TEXT("ThrowableMesh"));
        for(auto* N:Nodes)
        {
            if(auto* E=Cast<UK2Node_CustomEvent>(N); E && E->CustomFunctionName==TEXT("Launch"))
            {
                auto* Then=C.Pin(E,TEXT("then")); auto Links=Then->LinkedTo; Then->BreakAllPinLinks();
                auto* F=C.Call(UPrimitiveComponent::StaticClass(),TEXT("SetCollisionObjectType")); C.Link(Mesh,TEXT("ThrowableMesh"),F,TEXT("self")); C.Val(F,TEXT("Channel"),TEXT("ECC_WorldDynamic")); C.Exec(E,F);
                for(auto* L:Links) check(Schema->TryCreateConnection(C.Pin(F,TEXT("then")),L)); Patched=true;
            }
            if(auto* F=Cast<UK2Node_CallFunction>(N); F && F->FunctionReference.GetMemberName()==TEXT("SetEnableGravity") && C.Pin(F,TEXT("bGravityEnabled"))->DefaultValue==TEXT("true"))
            {
                auto* Restore=C.Call(UPrimitiveComponent::StaticClass(),TEXT("SetCollisionObjectType")); C.Link(Mesh,TEXT("ThrowableMesh"),Restore,TEXT("self")); C.Val(Restore,TEXT("Channel"),TEXT("ECC_PhysicsBody")); C.Exec(F,Restore);
            }
        }
    }
    for(UEdGraph* G:Cube->UbergraphPages)
    {
        int32 I=0; for(UEdGraphNode* N:G->Nodes) { N->NodePosX=(I%6)*700; N->NodePosY=(I/6)*800; ++I; }
    }
    if(Patched) Compile(Cube); else Save(Cube);
    // Give the input branches meaningful reusable entry points, also used by this smoke test.
    for (UEdGraph* G : Pawn->UbergraphPages)
    {
        Graph P{Pawn,G}; TArray<UK2Node_EnhancedInputAction*> Inputs;
        for(UEdGraphNode* N:G->Nodes) if(auto* I=Cast<UK2Node_EnhancedInputAction>(N))
            if(I->InputAction->GetName()==TEXT("IA_Pickup") || I->InputAction->GetName()==TEXT("IA_Throw")) Inputs.Add(I);
        for(auto* I:Inputs)
        {
            FName Name=I->InputAction->GetName()==TEXT("IA_Pickup") ? TEXT("TryPickup") : TEXT("TryThrow");
            if(Pawn->GeneratedClass->FindFunctionByName(Name)) continue;
            auto* Started=P.Pin(I,TEXT("Started")); auto Links=Started->LinkedTo; Started->BreakAllPinLinks();
            auto* E=P.Event(*Name.ToString());
            for(auto* L:Links) check(Schema->TryCreateConnection(P.Pin(E,TEXT("then")),L));
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Pawn);
            auto* Call=P.Call(Pawn->SkeletonGeneratedClass,*Name.ToString());
            P.Link(I,TEXT("Started"),Call,TEXT("execute"));
        }
    }
    for(UEdGraph* G:Pawn->UbergraphPages) if(G->GetName()==TEXT("Pickup and Throw"))
    {
        Graph P{Pawn,G}; TArray<UEdGraphNode*> Nodes; for(UEdGraphNode* N:G->Nodes) Nodes.Add(N);
        for(auto* N:Nodes) if(auto* Item=Cast<UK2Node_GetArrayItem>(N))
        {
            auto* Array=P.Pin(Item,TEXT("Array")); auto* Output=P.Pin(Item,TEXT("Output"));
            auto Sources=Array->LinkedTo; auto Targets=Output->LinkedTo;
            auto* Nearest=P.Call(UGameplayStatics::StaticClass(),TEXT("FindNearestActor"));
            for(auto* Src:Sources) check(Schema->TryCreateConnection(Src,P.Pin(Nearest,TEXT("ActorsToCheck"))));
            auto* Location=P.Call(AActor::StaticClass(),TEXT("K2_GetActorLocation")); P.Link(Location,TEXT("ReturnValue"),Nearest,TEXT("Origin"));
            Output->BreakAllPinLinks(); for(auto* Dest:Targets) check(Schema->TryCreateConnection(P.Pin(Nearest,TEXT("ReturnValue")),Dest));
            FBlueprintEditorUtils::RemoveNode(Pawn,Item,true);
        }
        int32 I=0; for(UEdGraphNode* N:G->Nodes) { N->NodePosX=(I%6)*700; N->NodePosY=(I/6)*800; ++I; }
    }
    Compile(Pawn);
    auto* Car=LoadObject<UBlueprint>(nullptr,TEXT("/Game/VehicleTemplate/Blueprints/SportsCar/SportsCar_Pawn")); check(Car); Compile(Car);
    Compile(LoadObject<UBlueprint>(nullptr,TEXT("/Game/VehicleTemplate/Blueprints/OffroadCar/OffroadCar_Pawn")));
    FAssetCompilingManager::Get().FinishAllCompilation();
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("ThrowableVerification"));
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* P1=World->SpawnActor<APawn>(Car->GeneratedClass,FVector(0,0,500),FRotator::ZeroRotator,SP); check(P1);
    auto* P2=World->SpawnActor<APawn>(Car->GeneratedClass,FVector(0,1500,500),FRotator::ZeroRotator,SP); check(P2);
    auto* A=World->SpawnActor<AActor>(Cube->GeneratedClass,FVector(200,0,500),FRotator::ZeroRotator,SP); check(A);
    auto* B=World->SpawnActor<AActor>(Cube->GeneratedClass,FVector(200,1500,500),FRotator::ZeroRotator,SP); check(B);
    auto* M=CastChecked<UPrimitiveComponent>(A->GetRootComponent());
    UE_LOG(LogTemp,Display,TEXT("TEST root=%s mass=%.2f collision=%d objecttype=%d"),*M->GetName(),M->GetMass(),int(M->GetCollisionEnabled()),int(M->GetCollisionObjectType()));
    check(FMath::IsNearlyEqual(M->GetMass(),75.f,.1f));
    auto* Held=FindFProperty<FObjectProperty>(Pawn->GeneratedClass,TEXT("HeldThrowable")); check(Held);
    P1->ProcessEvent(P1->FindFunctionChecked(TEXT("TryPickup")),nullptr);
    check(Held->GetObjectPropertyValue_InContainer(P1)==A);
    check(A->GetAttachParentActor()==P1 && !M->IsSimulatingPhysics() && !A->GetActorEnableCollision());
    P2->ProcessEvent(P2->FindFunctionChecked(TEXT("TryPickup")),nullptr);
    check(Held->GetObjectPropertyValue_InContainer(P2)==B);
    P1->SetActorLocation(FVector(50,0,500),false,nullptr,ETeleportType::TeleportPhysics); World->UpdateWorldComponents(false,false);
    check(A->GetActorLocation().Equals(P1->GetActorLocation()+FVector(0,230,100),.1));
    P1->ProcessEvent(P1->FindFunctionChecked(TEXT("TryPickup")),nullptr); check(Held->GetObjectPropertyValue_InContainer(P1)==A);
    World->SetBegunPlay(true); World->bShouldSimulatePhysics=true;
    const FVector Start=A->GetActorLocation();
    P1->ProcessEvent(P1->FindFunctionChecked(TEXT("TryThrow")),nullptr);
    check(!Held->GetObjectPropertyValue_InContainer(P1)); check(Held->GetObjectPropertyValue_InContainer(P2)==B);
    check(!A->GetAttachParentActor() && M->IsSimulatingPhysics());
    const FVector FlightDirection=M->GetPhysicsLinearVelocity().GetSafeNormal();
    check(FMath::IsNearlyEqual(M->GetPhysicsLinearVelocity().Size(),1000.f,.1f));
    // Exercise the latent graph and Chaos with a fixed 120 Hz step.
    for(int I=0;I<60;++I) { ++GFrameCounter; World->Tick(LEVELTICK_All,1.f/120.f); }
    const float Distance=FVector::DotProduct(A->GetActorLocation()-Start,FlightDirection);
    UE_LOG(LogTemp,Display,TEXT("TEST flight distance=%.2f cm velocity=%s gravity=%d"),Distance,*M->GetPhysicsLinearVelocity().ToString(),int(M->IsGravityEnabled()));
    checkf(FMath::IsNearlyEqual(Distance,500.f,18.f),TEXT("Unexpected throw range %.2f"),Distance);
    for(int I=0;I<12;++I) { ++GFrameCounter; World->Tick(LEVELTICK_All,1.f/120.f); }
    check(M->IsGravityEnabled());
    checkf(FVector::DotProduct(M->GetPhysicsLinearVelocity(),FlightDirection)>990.f && M->GetPhysicsLinearVelocity().Z < -50.f,TEXT("Forward inertia or falling gravity missing"));
    check(FVector::DotProduct(A->GetActorLocation()-Start,FlightDirection)>590.f);
    UE_LOG(LogTemp,Display,TEXT("TEST falling with forward inertia velocity=%s"),*M->GetPhysicsLinearVelocity().ToString());
    check(M->GetCollisionObjectType()==ECC_PhysicsBody);
    // Real Chaos contact against the other player's vehicle, no scripted push.
    auto* Target=CastChecked<UPrimitiveComponent>(P2->GetRootComponent());
    const FVector Aim=FVector(500,230,600);
    P1->SetActorLocation(FVector(-2000,0,500),false,nullptr,ETeleportType::TeleportPhysics);
    P2->SetActorLocation(Aim+FVector(400,0,0)-(Target->Bounds.Origin-P2->GetActorLocation()),false,nullptr,ETeleportType::TeleportPhysics);
    Target->SetPhysicsLinearVelocity(FVector::ZeroVector); Target->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    A->SetActorLocation(Aim,false,nullptr,ETeleportType::TeleportPhysics);
    struct { FVector Direction; } LaunchArgs{FVector::ForwardVector};
    A->ProcessEvent(A->FindFunctionChecked(TEXT("Launch")),&LaunchArgs);
    check(M->GetCollisionObjectType()==ECC_WorldDynamic);
    for(int I=0;I<34;++I) { ++GFrameCounter; World->Tick(LEVELTICK_All,1.f/120.f); }
    UE_LOG(LogTemp,Display,TEXT("TEST vehicle push velocity=%s"),*Target->GetPhysicsLinearVelocity().ToString());
    checkf(Target->GetPhysicsLinearVelocity().X>1.f,TEXT("Projectile did not push vehicle"));
    UE_LOG(LogTemp,Display,TEXT("THROWABLES VERIFY: pickup, carry, per-pawn ownership, release, range PASS"));
    World->SetBegunPlay(false); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return 0;
}
}
int32 ConfigureThrowAim();
int32 ConfigureThrowPush();
int32 UBuildThrowablesCommandlet::Main(const FString& Params)
{
    if(Params.Contains(TEXT("ConfigurePush"))) return ConfigureThrowPush();
    if(Params.Contains(TEXT("ConfigureAim"))) return ConfigureThrowAim();
    if(Params.Contains(TEXT("UpgradeFlight")))
    {
        auto* Cube=LoadObject<UBlueprint>(nullptr,TEXT("/Game/VehicleTemplate/Blueprints/BP_ThrowableCube")); check(Cube);
        const int32 Index=FBlueprintEditorUtils::FindNewVariableIndex(Cube,TEXT("FlightDuration")); check(Index!=INDEX_NONE);
        Cube->NewVariables[Index].DefaultValue=TEXT("0.5");
        auto* Duration=FindFProperty<FDoubleProperty>(Cube->GeneratedClass,TEXT("FlightDuration")); check(Duration);
        Duration->SetPropertyValue_InContainer(Cube->GeneratedClass->GetDefaultObject(),.5);
        for(UEdGraph* G:Cube->UbergraphPages)
        {
            TArray<UEdGraphNode*> Nodes; for(UEdGraphNode* N:G->Nodes) Nodes.Add(N);
            for(auto* N:Nodes) if(auto* F=Cast<UK2Node_CallFunction>(N); F && F->FunctionReference.GetMemberName()==TEXT("SetPhysicsLinearVelocity"))
            {
                auto* NewVel=F->FindPin(TEXT("NewVel"));
                if(!NewVel || NewVel->LinkedTo.Num()!=0) continue;
                auto Inputs=F->FindPin(TEXT("execute"))->LinkedTo; auto Outputs=F->FindPin(TEXT("then"))->LinkedTo;
                F->BreakAllNodeLinks();
                for(auto* In:Inputs) for(auto* Out:Outputs) check(Schema->TryCreateConnection(In,Out));
                FBlueprintEditorUtils::RemoveNode(Cube,F,true);
            }
        }
        Compile(Cube);
        return VerifyThrowables();
    }
    if(Params.Contains(TEXT("Verify"))) return VerifyThrowables();
    auto* IMC = LoadObject<UInputMappingContext>(nullptr,TEXT("/Game/VehicleTemplate/Input/IMC_Default")); check(IMC);
    auto* Pick = Action(TEXT("IA_Pickup"),EKeys::F,EKeys::Gamepad_FaceButton_Bottom,IMC);
    auto* Throw = Action(TEXT("IA_Throw"),EKeys::R,EKeys::Gamepad_FaceButton_Left,IMC); Save(IMC);
    const TCHAR* CubePath = TEXT("/Game/VehicleTemplate/Blueprints/BP_ThrowableCube");
    checkf(!LoadObject<UBlueprint>(nullptr,CubePath),TEXT("Already generated; restore the original assets before rebuilding."));
    auto* Cube = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(),CreatePackage(CubePath),TEXT("BP_ThrowableCube"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
    auto* MeshNode = Cube->SimpleConstructionScript->CreateNode(UStaticMeshComponent::StaticClass(),TEXT("ThrowableMesh"));
    Cube->SimpleConstructionScript->AddNode(MeshNode);
    auto* Mesh = CastChecked<UStaticMeshComponent>(MeshNode->ComponentTemplate);
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Mesh->SetRelativeScale3D(FVector(.4)); Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetCollisionProfileName(TEXT("PhysicsActor")); Mesh->SetSimulatePhysics(true);
    Mesh->SetMassOverrideInKg(NAME_None,75.f,true); Mesh->BodyInstance.bUseCCD = true;
    Mesh->SetLinearDamping(0.f); Mesh->SetAngularDamping(.5f);
    FEdGraphPinType Float; Float.PinCategory = UEdGraphSchema_K2::PC_Real; Float.PinSubCategory = UEdGraphSchema_K2::PC_Double;
    FBlueprintEditorUtils::AddMemberVariable(Cube,TEXT("ThrowSpeed"),Float,TEXT("1000.0"));
    FBlueprintEditorUtils::AddMemberVariable(Cube,TEXT("FlightDuration"),Float,TEXT("0.5"));
    for (auto& V : Cube->NewVariables) V.PropertyFlags |= CPF_Edit;
    auto* CG = FBlueprintEditorUtils::FindEventGraph(Cube); Graph C{Cube,CG};
    auto* Pickup = C.Event(TEXT("Pickup")); Pickup->CreateUserDefinedPin(TEXT("Holder"),ObjectType(AActor::StaticClass()),EGPD_Output);
    auto* CM = C.Get(TEXT("ThrowableMesh"));
    auto* SimOff = C.Call(UPrimitiveComponent::StaticClass(),TEXT("SetSimulatePhysics")); C.Val(SimOff,TEXT("bSimulate"),TEXT("false")); C.Link(CM,TEXT("ThrowableMesh"),SimOff,TEXT("self")); C.Exec(Pickup,SimOff);
    auto* CollisionOff = C.Call(AActor::StaticClass(),TEXT("SetActorEnableCollision")); C.Val(CollisionOff,TEXT("bNewActorEnableCollision"),TEXT("false")); C.Exec(SimOff,CollisionOff);
    auto* Attach = C.Call(AActor::StaticClass(),TEXT("K2_AttachToActor")); C.Link(Pickup,TEXT("Holder"),Attach,TEXT("ParentActor"));
    C.Val(Attach,TEXT("LocationRule"),TEXT("SnapToTarget")); C.Val(Attach,TEXT("RotationRule"),TEXT("SnapToTarget")); C.Val(Attach,TEXT("ScaleRule"),TEXT("KeepWorld")); C.Val(Attach,TEXT("bWeldSimulatedBodies"),TEXT("false")); C.Exec(CollisionOff,Attach);
    auto* Offset = C.Call(AActor::StaticClass(),TEXT("K2_SetActorRelativeLocation")); C.Val(Offset,TEXT("NewRelativeLocation"),TEXT("(X=0,Y=230,Z=100)")); C.Exec(Attach,Offset);
    auto* Launch = C.Event(TEXT("Launch"));
    FEdGraphPinType Vector; Vector.PinCategory=UEdGraphSchema_K2::PC_Struct; Vector.PinSubCategoryObject=TBaseStructure<FVector>::Get();
    Launch->CreateUserDefinedPin(TEXT("Direction"),Vector,EGPD_Output);
    auto* Detach = C.Call(AActor::StaticClass(),TEXT("K2_DetachFromActor"));
    for (auto* P : {TEXT("LocationRule"),TEXT("RotationRule"),TEXT("ScaleRule")}) C.Val(Detach,P,TEXT("KeepWorld")); C.Exec(Launch,Detach);
    auto* GravityOff = C.Call(UPrimitiveComponent::StaticClass(),TEXT("SetEnableGravity")); C.Val(GravityOff,TEXT("bGravityEnabled"),TEXT("false")); C.Link(CM,TEXT("ThrowableMesh"),GravityOff,TEXT("self")); C.Exec(Detach,GravityOff);
    auto* CollisionOn = C.Call(AActor::StaticClass(),TEXT("SetActorEnableCollision")); C.Val(CollisionOn,TEXT("bNewActorEnableCollision"),TEXT("true")); C.Exec(GravityOff,CollisionOn);
    auto* SimOn = C.Call(UPrimitiveComponent::StaticClass(),TEXT("SetSimulatePhysics")); C.Val(SimOn,TEXT("bSimulate"),TEXT("true")); C.Link(CM,TEXT("ThrowableMesh"),SimOn,TEXT("self")); C.Exec(CollisionOn,SimOn);
    auto* Speed = C.Get(TEXT("ThrowSpeed"));
    auto* Multiply = C.Call(UKismetMathLibrary::StaticClass(),TEXT("Multiply_VectorFloat")); C.Link(Launch,TEXT("Direction"),Multiply,TEXT("A")); C.Link(Speed,TEXT("ThrowSpeed"),Multiply,TEXT("B"));
    auto* Velocity = C.Call(UPrimitiveComponent::StaticClass(),TEXT("SetPhysicsLinearVelocity")); C.Link(CM,TEXT("ThrowableMesh"),Velocity,TEXT("self")); C.Link(Multiply,TEXT("ReturnValue"),Velocity,TEXT("NewVel")); C.Exec(SimOn,Velocity);
    auto* Delay = C.Call(UKismetSystemLibrary::StaticClass(),TEXT("Delay")); auto* Duration=C.Get(TEXT("FlightDuration")); C.Link(Duration,TEXT("FlightDuration"),Delay,TEXT("Duration")); C.Exec(Velocity,Delay);
    auto* GravityOn=C.Call(UPrimitiveComponent::StaticClass(),TEXT("SetEnableGravity")); C.Link(CM,TEXT("ThrowableMesh"),GravityOn,TEXT("self")); C.Val(GravityOn,TEXT("bGravityEnabled"),TEXT("true")); C.Exec(Delay,GravityOn);
    Compile(Cube);

    auto* Pawn = LoadObject<UBlueprint>(nullptr,TEXT("/Game/VehicleTemplate/Blueprints/VehicleAdvPawn")); check(Pawn);
    FBlueprintEditorUtils::AddMemberVariable(Pawn,TEXT("HeldThrowable"),ObjectType(Cube->GeneratedClass));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Pawn);
    auto* PG = FBlueprintEditorUtils::CreateNewGraph(Pawn,TEXT("Pickup and Throw"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(Pawn,PG); Graph P{Pawn,PG};
    auto Input = [&](UInputAction* IA) { auto* N=P.Node<UK2Node_EnhancedInputAction>(); N->InputAction=IA; N->AllocateDefaultPins(); return N; };
    auto* PE=Input(Pick); auto* Held=P.Get(TEXT("HeldThrowable")); auto* Valid=P.Call(UKismetSystemLibrary::StaticClass(),TEXT("IsValid")); P.Link(Held,TEXT("HeldThrowable"),Valid,TEXT("Object"));
    auto* Branch=P.Node<UK2Node_IfThenElse>(); Branch->AllocateDefaultPins(); P.Link(PE,TEXT("Started"),Branch,TEXT("execute")); P.Link(Valid,TEXT("ReturnValue"),Branch,TEXT("Condition"));
    auto* Overlap=P.Call(UKismetSystemLibrary::StaticClass(),TEXT("SphereOverlapActors")); P.Link(Branch,TEXT("else"),Overlap,TEXT("execute")); P.Val(Overlap,TEXT("SphereRadius"),TEXT("400"));
    Schema->TrySetDefaultObject(*P.Pin(Overlap,TEXT("ActorClassFilter")),Cube->GeneratedClass);
    auto* Location=P.Call(AActor::StaticClass(),TEXT("K2_GetActorLocation")); P.Link(Location,TEXT("ReturnValue"),Overlap,TEXT("SpherePos"));
    auto* Types=P.Node<UK2Node_MakeArray>(); Types->AllocateDefaultPins(); P.Link(Types,TEXT("Array"),Overlap,TEXT("ObjectTypes")); P.Val(Types,TEXT("[0]"),TEXT("ObjectTypeQuery4"));
    auto* Found=P.Node<UK2Node_IfThenElse>(); Found->AllocateDefaultPins(); P.Exec(Overlap,Found); P.Link(Overlap,TEXT("ReturnValue"),Found,TEXT("Condition"));
    auto* Item=P.Node<UK2Node_GetArrayItem>(); Item->AllocateDefaultPins(); P.Link(Overlap,TEXT("OutActors"),Item,TEXT("Array"));
    auto* Cast=P.Node<UK2Node_DynamicCast>(); Cast->TargetType=Cube->GeneratedClass; Cast->AllocateDefaultPins(); P.Link(Found,TEXT("then"),Cast,TEXT("execute")); P.Link(Item,TEXT("Output"),Cast,TEXT("Object"));
    auto* SetHeld=P.Set(TEXT("HeldThrowable")); P.Link(Cast,TEXT("then"),SetHeld,TEXT("execute")); P.Link(Cast,*Cast->GetCastResultPin()->PinName.ToString(),SetHeld,TEXT("HeldThrowable"));
    auto* Take=P.Call(Cube->GeneratedClass,TEXT("Pickup")); P.Exec(SetHeld,Take); P.Link(Held,TEXT("HeldThrowable"),Take,TEXT("self")); auto* Self=P.Self(); P.Link(Self,TEXT("self"),Take,TEXT("Holder"));
    auto* TE=Input(Throw); auto* TB=P.Node<UK2Node_IfThenElse>(); TB->AllocateDefaultPins(); P.Link(TE,TEXT("Started"),TB,TEXT("execute")); P.Link(Valid,TEXT("ReturnValue"),TB,TEXT("Condition"));
    auto* Toss=P.Call(Cube->GeneratedClass,TEXT("Launch")); P.Link(TB,TEXT("then"),Toss,TEXT("execute")); P.Link(Held,TEXT("HeldThrowable"),Toss,TEXT("self"));
    auto* Forward=P.Call(AActor::StaticClass(),TEXT("GetActorForwardVector")); P.Link(Forward,TEXT("ReturnValue"),Toss,TEXT("Direction"));
    auto* Clear=P.Set(TEXT("HeldThrowable")); P.Exec(Toss,Clear);
    Compile(Pawn);
    for (const TCHAR* Path : {TEXT("/Game/VehicleTemplate/Blueprints/SportsCar/SportsCar_Pawn"),TEXT("/Game/VehicleTemplate/Blueprints/OffroadCar/OffroadCar_Pawn")})
        if (auto* Child=LoadObject<UBlueprint>(nullptr,Path)) Compile(Child);
    auto* World=UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/VehicleTemplate/Maps/VehicleExampleMap")); check(World);
    for (TActorIterator<APlayerStart> It(World); It; ++It)
    {
        for (int I=0; I<4; ++I)
        {
            FVector Pos=It->GetActorLocation()+It->GetActorForwardVector()*(450.f+I*150.f)+It->GetActorRightVector()*300.f;
            FHitResult Hit; if(World->LineTraceSingleByChannel(Hit,Pos+FVector(0,0,1000),Pos-FVector(0,0,2000),ECC_Visibility)) Pos.Z=Hit.ImpactPoint.Z+25.f;
            auto* A=World->SpawnActor<AActor>(Cube->GeneratedClass,Pos,FRotator::ZeroRotator); check(A); A->SetActorLabel(FString::Printf(TEXT("Throwable Cube %d"),I+1));
        }
        break;
    }
    check(UEditorLoadingAndSavingUtils::SaveMap(World,TEXT("/Game/VehicleTemplate/Maps/VehicleExampleMap")));
    UE_LOG(LogTemp,Display,TEXT("THROWABLES: Blueprint generation and compile PASS"));
    const int32 Result=VerifyThrowables(); if(Result) return Result;
    const int32 AimResult=ConfigureThrowAim(); return AimResult ? AimResult : ConfigureThrowPush();
}










