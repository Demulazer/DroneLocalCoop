#include "LocalMultiplayerSubsystem.h"
#include "ArcadeCoastingComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "EnhancedPlayerInput.h"
#include "InputAction.h"
#include "InputKeyEventArgs.h"
#include "LocalMultiplayerViewportClient.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"
#include "HAL/PlatformTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogLocalMultiplayer, Log, All);

bool ULocalMultiplayerSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
    return Type == EWorldType::Game || Type == EWorldType::PIE;
}

void ULocalMultiplayerSubsystem::OnWorldBeginPlay(UWorld& World)
{
    Super::OnWorldBeginPlay(World);
    if (World.GetNetMode() != NM_Standalone)
    {
        return;
    }
    if (auto* Viewport = Cast<ULocalMultiplayerViewportClient>(World.GetGameViewport()))
    {
        Viewport->ResetGamepadAssignments();
    }
    // Wait for the template game mode to possess player one's vehicle first.
    World.GetTimerManager().SetTimer(SetupTimer, this,
        &ThisClass::SetupPlayers, 0.1f, true);
}

void ULocalMultiplayerSubsystem::SetupPlayers()
{
    UWorld* World = GetWorld();
    UGameInstance* Instance = World->GetGameInstance();
    AGameModeBase* Mode = World->GetAuthGameMode();
    APlayerController* First = UGameplayStatics::GetPlayerController(World, 0);
    if (!Instance || !Mode || !First || !First->GetPawn())
    {
        if (++SetupAttempts >= 100)
        {
            World->GetTimerManager().ClearTimer(SetupTimer);
            UE_LOG(LogLocalMultiplayer, Error, TEXT("Local multiplayer: player one's vehicle was not ready."));
        }
        return;
    }
    World->GetTimerManager().ClearTimer(SetupTimer);

    if (Instance->GetNumLocalPlayers() < 2)
    {
        // The template maps have only one start. Give the second vehicle its own
        // clear start alongside the first, using the current map's pawn class.
        APawn* FirstPawn = First->GetPawn();
        const FVector Origin = FirstPawn->GetActorLocation();
        const FVector Right = FirstPawn->GetActorRightVector();
        APlayerStart* SecondStart = nullptr;
        for (int32 Attempt = 1; Attempt <= 8; ++Attempt)
        {
            FVector Location = Origin + Right * (500.f * Attempt);
            FRotator Rotation = FirstPawn->GetActorRotation();
            if (World->FindTeleportSpot(FirstPawn, Location, Rotation))
            {
                SecondStart = World->SpawnActor<APlayerStart>(Location, Rotation);
                break;
            }
        }
        if (!SecondStart)
        {
            UE_LOG(LogLocalMultiplayer, Error, TEXT("No clear start for the second vehicle."));
            return;
        }
        FString Error;
        ULocalPlayer* Second = Instance->CreateLocalPlayer(1, Error, true);
        if (!Second)
        {
            UE_LOG(LogLocalMultiplayer, Error, TEXT("Cannot create player two: %s"), *Error);
            SecondStart->Destroy();
            return;
        }
        APlayerController* Controller = Second->GetPlayerController(World);
        if (Controller && !Controller->GetPawn())
        {
            Mode->RestartPlayerAtPlayerStart(Controller, SecondStart);
        }
    }

    if (UGameViewportClient* Viewport = Instance->GetGameViewportClient())
    {
        Viewport->SetForceDisableSplitscreen(false);
        Viewport->LayoutPlayers();
    }
    UInputMappingContext* Mapping = LoadObject<UInputMappingContext>(nullptr,
        TEXT("/Game/VehicleTemplate/Input/IMC_Default.IMC_Default"));
    for (ULocalPlayer* Player : Instance->GetLocalPlayers())
    {
        APlayerController* Controller = Player->GetPlayerController(World);
        if (!Controller || !Controller->GetPawn())
        {
            continue;
        }
        Controller->SetViewTarget(Controller->GetPawn());
        Controller->SetInputMode(FInputModeGameOnly());
        Controller->bShowMouseCursor = false;
        APawn* Pawn = Controller->GetPawn();
        if (!Pawn->FindComponentByClass<UArcadeCoastingComponent>())
        {
            auto* Coasting = NewObject<UArcadeCoastingComponent>(Pawn);
            Pawn->AddInstanceComponent(Coasting);
            Coasting->RegisterComponent();
            Coasting->AddTickPrerequisiteActor(Pawn);
            Coasting->AddTickPrerequisiteActor(Controller);
        }
        if (UEnhancedInputLocalPlayerSubsystem* Input =
            Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            if (Mapping && !Input->HasMappingContext(Mapping))
            {
                Input->AddMappingContext(Mapping, 0);
            }
        }
    }
    // Let the Blueprint controllers finish creating their HUDs.
    World->GetTimerManager().SetTimer(VerifyTimer, this,
        &ThisClass::VerifyPlayers, 1.f, false);
}

void ULocalMultiplayerSubsystem::VerifyPlayers()
{
    UWorld* World = GetWorld();
    UGameInstance* Instance = World->GetGameInstance();
    // The template adds VehicleUI to the full viewport. Constrain each owned
    // instance to its local player's rectangle instead.
    UClass* VehicleUI = LoadClass<UUserWidget>(nullptr,
        TEXT("/Game/VehicleTemplate/Blueprints/VehicleUI.VehicleUI_C"));
    TSet<ULocalPlayer*> PlayersWithHUD;
    for (TObjectIterator<UUserWidget> It; It; ++It)
    {
        if (VehicleUI && It->GetWorld() == World && It->IsA(VehicleUI)
            && It->IsInViewport() && It->GetOwningLocalPlayer())
        {
            It->RemoveFromParent();
            It->AddToPlayerScreen();
            PlayersWithHUD.Add(It->GetOwningLocalPlayer());
        }
    }
    bool bValid = Instance->GetNumLocalPlayers() == 2;
    APawn* PreviousPawn = nullptr;
    for (ULocalPlayer* Player : Instance->GetLocalPlayers())
    {
        APlayerController* Controller = Player->GetPlayerController(World);
        APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
        auto* Input = Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
        UInputMappingContext* Mapping = LoadObject<UInputMappingContext>(nullptr,
            TEXT("/Game/VehicleTemplate/Input/IMC_Default.IMC_Default"));
        bValid &= Pawn && Pawn != PreviousPawn && Pawn->GetController() == Controller
            && Controller->GetViewTarget() == Pawn && Input && Mapping
            && Input->HasMappingContext(Mapping)
            && FMath::IsNearlyEqual(Player->Size.X * Player->Size.Y, 0.5f)
            && Cast<UEnhancedInputComponent>(Pawn->InputComponent)
            && PlayersWithHUD.Contains(Player);
        if (PreviousPawn && Pawn)
        {
            bValid &= FVector::Dist(Pawn->GetActorLocation(), PreviousPawn->GetActorLocation()) > 300.f;
        }
        UE_LOG(LogLocalMultiplayer, Display,
            TEXT("Player %d: vehicle=%s view=%s origin=(%.1f,%.1f) size=(%.1f,%.1f)"),
            Player->GetControllerId(), *GetNameSafe(Pawn),
            *GetNameSafe(Controller ? Controller->GetViewTarget() : nullptr),
            Player->Origin.X, Player->Origin.Y, Player->Size.X, Player->Size.Y);
        PreviousPawn = Pawn;
    }
    UE_LOG(LogLocalMultiplayer, Display, TEXT("Split-screen validation: %s"), bValid ? TEXT("PASS") : TEXT("FAIL"));
    if (FParse::Param(FCommandLine::Get(), TEXT("SplitScreenTest")))
    {
        bLayoutValid = bValid;
        auto* Viewport = Cast<ULocalMultiplayerViewportClient>(Instance->GetGameViewportClient());
        if (!bValid || !Viewport)
        {
            FPlatformMisc::RequestExitWithStatus(false, 1);
            return;
        }
        RoutingTestCase = 0;
        StartGamepadRoutingTest();
    }
}

namespace
{
    // RawInput sends both DS4s with device zero. Exercise its physical polling
    // scope too, rather than giving test DS4s artificially different user ids.
    void SendTestGamepad(ULocalMultiplayerViewportClient* Viewport, bool bSony,
        int32 PhysicalId, FKey Key, float Value, bool bAxis)
    {
        FInputDeviceScope Scope(nullptr,
            bSony ? TEXT("RawInput") : TEXT("XInput"),
            PhysicalId,
            FString::Printf(TEXT("TestController%d"), PhysicalId));

        const int32 DeviceId = bSony ? 0 : 20 + PhysicalId;
        const FInputDeviceId InputDevice =
            FInputDeviceId::CreateFromInternalId(DeviceId);

        const FKey RawKey = bSony
            ? FKey(*(TEXT("DS4Raw_") + Key.ToString()))
            : Key;

        const uint64 Timestamp = FPlatformTime::Cycles64();

        if (bAxis)
        {
            const FInputKeyEventArgs AxisArgs(
                nullptr,
                InputDevice,
                RawKey,
                Value,
                0.016f,
                1,
                Timestamp
            );

            Viewport->InputAxis(AxisArgs);
        }
        else
        {
            const FInputKeyEventArgs KeyArgs(
                nullptr,
                InputDevice,
                RawKey,
                Value > 0.f ? IE_Pressed : IE_Released,
                Value,
                false,
                Timestamp
            );

            Viewport->InputKey(KeyArgs);
        }
    }

    bool TestSourceIsSony(int32 Scenario, int32 Player)
    {
        const bool Sources[4][2] = {{false, true}, {true, false}, {false, false}, {true, true}};
        return Sources[Scenario][Player];
    }
}

void ULocalMultiplayerSubsystem::StartGamepadRoutingTest()
{
    auto* Viewport = CastChecked<ULocalMultiplayerViewportClient>(GetWorld()->GetGameViewport());
    Viewport->ResetGamepadAssignments();
    for (int32 Index = 0; Index < 2; ++Index)
    {
        const bool bSony = TestSourceIsSony(RoutingTestCase, Index);
        SendTestGamepad(Viewport, bSony, Index, EKeys::Gamepad_FaceButton_Bottom, 1.f, false);
        SendTestGamepad(Viewport, bSony, Index, EKeys::Gamepad_FaceButton_Bottom, 0.f, false);
        // Repeated join presses must not take the other player's slot.
        SendTestGamepad(Viewport, bSony, Index, EKeys::Gamepad_FaceButton_Bottom, 1.f, false);
        SendTestGamepad(Viewport, bSony, Index, EKeys::Gamepad_FaceButton_Bottom, 0.f, false);
        SendTestGamepad(Viewport, bSony, Index, EKeys::Gamepad_RightTriggerAxis, Index ? 0.8f : 0.35f, true);
        SendTestGamepad(Viewport, bSony, Index, EKeys::Gamepad_LeftX, Index ? 0.7f : -0.55f, true);
        SendTestGamepad(Viewport, bSony, Index, EKeys::Gamepad_LeftShoulder, Index ? 1.f : 0.f, false);
    }
    // Additional unassigned devices cannot steer either player.
    SendTestGamepad(Viewport, true, 99, EKeys::Gamepad_FaceButton_Bottom, 1.f, false);
    SendTestGamepad(Viewport, true, 99, EKeys::Gamepad_RightTriggerAxis, 1.f, true);
    SendTestGamepad(Viewport, false, 99, EKeys::Gamepad_LeftX, 1.f, true);
    GetWorld()->GetTimerManager().SetTimer(VerifyTimer, this,
        &ThisClass::VerifyGamepadRouting, 0.15f, false);
}

void ULocalMultiplayerSubsystem::VerifyGamepadRouting()
{
    bool bValid = bLayoutValid;
    UInputAction* Throttle = LoadObject<UInputAction>(nullptr,
        TEXT("/Game/VehicleTemplate/Input/Actions/IA_Throttle.IA_Throttle"));
    UInputAction* Steering = LoadObject<UInputAction>(nullptr,
        TEXT("/Game/VehicleTemplate/Input/Actions/IA_Steering.IA_Steering"));
    UInputAction* Handbrake = LoadObject<UInputAction>(nullptr,
        TEXT("/Game/VehicleTemplate/Input/Actions/IA_Handbrake.IA_Handbrake"));
    for (int32 Index = 0; Index < 2; ++Index)
    {
        APlayerController* Controller = UGameplayStatics::GetPlayerController(GetWorld(), Index);
        auto* Input = Controller ? Cast<UEnhancedPlayerInput>(Controller->PlayerInput) : nullptr;
        if (!Input || !Throttle || !Steering || !Handbrake)
        {
            bValid = false;
            continue;
        }
        const float Gas = Input->GetActionValue(Throttle).Get<float>();
        const float Turn = Input->GetActionValue(Steering).Get<float>();
        // The template's handbrake action can use a one-frame pressed trigger.
        // Check the held key after the input frame instead of that transient action.
        const bool Brake = Input->GetKeyValue(EKeys::Gamepad_LeftShoulder) > 0.5f;
        bValid &= FMath::IsNearlyEqual(Gas, Index ? 0.8f : 0.35f, 0.02f)
            && FMath::IsNearlyEqual(Turn, Index ? 0.7f : -0.55f, 0.02f)
            && Brake == (Index == 1);
        UE_LOG(LogLocalMultiplayer, Display, TEXT("Routing case %d player %d (%s): throttle=%.2f steering=%.2f handbrake=%d"),
            RoutingTestCase, Index, TestSourceIsSony(RoutingTestCase, Index) ? TEXT("PS4") : TEXT("Xbox"), Gas, Turn, Brake);
    }
    if (bValid && ++RoutingTestCase < 4)
    {
        StartGamepadRoutingTest();
        return;
    }
    UE_LOG(LogLocalMultiplayer, Display, TEXT("Gamepad assignment validation: %s"), bValid ? TEXT("PASS") : TEXT("FAIL"));
    if (!bValid)
    {
        FPlatformMisc::RequestExitWithStatus(false, 1);
        return;
    }
    StickTestPhase = 0;
    StartStickMovementTest();
}

void ULocalMultiplayerSubsystem::StartStickMovementTest()
{
    auto* Viewport = CastChecked<ULocalMultiplayerViewportClient>(GetWorld()->GetGameViewport());
    // The final routing scenario has two separately joined Sony controllers.
    for (int32 Index = 0; Index < 2; ++Index)
    {
        SendTestGamepad(Viewport, true, Index, EKeys::Gamepad_RightTriggerAxis, 0.f, true);
        SendTestGamepad(Viewport, true, Index, EKeys::Gamepad_LeftTriggerAxis, 0.f, true);
        const float Value = StickTestPhase == 2 ? 0.f
            : ((Index == StickTestPhase) ? 0.8f : -0.8f);
        SendTestGamepad(Viewport, true, Index, EKeys::Gamepad_LeftY, Value, true);
    }
    GetWorld()->GetTimerManager().SetTimer(VerifyTimer, this,
        &ThisClass::VerifyStickMovement, 0.15f, false);
}

void ULocalMultiplayerSubsystem::VerifyStickMovement()
{
    bool bValid = true;
    UInputAction* Throttle = LoadObject<UInputAction>(nullptr,
        TEXT("/Game/VehicleTemplate/Input/Actions/IA_Throttle.IA_Throttle"));
    UInputAction* Brake = LoadObject<UInputAction>(nullptr,
        TEXT("/Game/VehicleTemplate/Input/Actions/IA_Brake.IA_Brake"));
    for (int32 Index = 0; Index < 2; ++Index)
    {
        auto* Controller = UGameplayStatics::GetPlayerController(GetWorld(), Index);
        auto* Input = Controller ? Cast<UEnhancedPlayerInput>(Controller->PlayerInput) : nullptr;
        if (!Input || !Throttle || !Brake)
        {
            bValid = false;
            continue;
        }
        const float Gas = Input->GetActionValue(Throttle).Get<float>();
        const float Stop = Input->GetActionValue(Brake).Get<float>();
        bValid &= StickTestPhase == 2 ? FMath::IsNearlyZero(Gas) && FMath::IsNearlyZero(Stop)
            : Index == StickTestPhase ? Gas > 0.5f && FMath::IsNearlyZero(Stop)
            : FMath::IsNearlyZero(Gas) && Stop > 0.5f;
        UE_LOG(LogLocalMultiplayer, Display, TEXT("Stick phase %d player %d: throttle=%.2f brake=%.2f"),
            StickTestPhase, Index, Gas, Stop);
    }
    if (bValid && ++StickTestPhase < 3)
    {
        StartStickMovementTest();
        return;
    }
    UE_LOG(LogLocalMultiplayer, Display, TEXT("Stick movement validation: %s"), bValid ? TEXT("PASS") : TEXT("FAIL"));
    if (!bValid)
    {
        FPlatformMisc::RequestExitWithStatus(false, 1);
        return;
    }
    StartCoastingTest();
}

void ULocalMultiplayerSubsystem::StartCoastingTest()
{
    auto* Viewport = CastChecked<ULocalMultiplayerViewportClient>(GetWorld()->GetGameViewport());
    CoastingStartPositions.Reset();
    for (int32 Index = 0; Index < 2; ++Index)
    {
        SendTestGamepad(Viewport, true, Index, EKeys::Gamepad_LeftShoulder, 0.f, false);
        SendTestGamepad(Viewport, true, Index, EKeys::Gamepad_LeftX, 0.f, true);
        APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), Index);
        auto* Movement = Pawn->FindComponentByClass<UChaosWheeledVehicleMovementComponent>();
        auto* Body = Movement ? Cast<UPrimitiveComponent>(Movement->UpdatedComponent) : nullptr;
        if (!Body)
        {
            FPlatformMisc::RequestExitWithStatus(false, 1);
            return;
        }
        Body->SetPhysicsLinearVelocity(Pawn->GetActorForwardVector() * 1000.f);
        CoastingStartPositions.Add(Pawn->GetActorLocation());
    }
    GetWorld()->GetTimerManager().SetTimer(VerifyTimer, this,
        &ThisClass::VerifyCoasting, 0.45f, false);
}

void ULocalMultiplayerSubsystem::VerifyCoasting()
{
    bool bValid = CoastingStartPositions.Num() == 2;
    for (int32 Index = 0; Index < 2 && bValid; ++Index)
    {
        APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), Index);
        auto* Movement = Pawn->FindComponentByClass<UChaosWheeledVehicleMovementComponent>();
        auto* Body = Movement ? Cast<UPrimitiveComponent>(Movement->UpdatedComponent) : nullptr;
        const float Speed = Body ? Body->GetPhysicsLinearVelocity().Size2D() : 1000.f;
        const float Distance = FVector::Dist2D(CoastingStartPositions[Index], Pawn->GetActorLocation());
        bValid &= Speed < 120.f && Distance < 200.f
            && Pawn->FindComponentByClass<UArcadeCoastingComponent>() != nullptr;
        UE_LOG(LogLocalMultiplayer, Display, TEXT("Coasting player %d: speed 1000 -> %.1f cm/s, travel %.1f cm in 0.45 s"),
            Index, Speed, Distance);
    }
    UE_LOG(LogLocalMultiplayer, Display, TEXT("Arcade coasting validation: %s"), bValid ? TEXT("PASS") : TEXT("FAIL"));
    FPlatformMisc::RequestExitWithStatus(false, bValid ? 0 : 1);
}

void ULocalMultiplayerSubsystem::Deinitialize()
{
    GetWorld()->GetTimerManager().ClearTimer(SetupTimer);
    GetWorld()->GetTimerManager().ClearTimer(VerifyTimer);
    Super::Deinitialize();
}

