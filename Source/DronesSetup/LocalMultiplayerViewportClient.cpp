#include "LocalMultiplayerViewportClient.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "InputKeyEventArgs.h"
#include "InputCoreTypes.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(LogGamepadAssignment, Log, All);

namespace
{
    constexpr TCHAR SonyPrefix[] = TEXT("DS4Raw_");

    FKey TranslateSonyKey(FKey Key)
    {
        const FString Name = Key.ToString();
        return Name.StartsWith(SonyPrefix) ? FKey(*Name.RightChop(7)) : EKeys::Invalid;
    }
}

void ULocalMultiplayerViewportClient::RegisterSonyKeys()
{
    const FKey Axes[] = { EKeys::Gamepad_LeftX, EKeys::Gamepad_LeftY,
        EKeys::Gamepad_RightX, EKeys::Gamepad_RightY,
        EKeys::Gamepad_LeftTriggerAxis, EKeys::Gamepad_RightTriggerAxis };
    const FKey Buttons[] = { EKeys::Gamepad_FaceButton_Left, EKeys::Gamepad_FaceButton_Bottom,
        EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_FaceButton_Top,
        EKeys::Gamepad_LeftShoulder, EKeys::Gamepad_RightShoulder,
        EKeys::Gamepad_LeftTrigger, EKeys::Gamepad_RightTrigger,
        EKeys::Gamepad_Special_Left, EKeys::Gamepad_Special_Right,
        EKeys::Gamepad_LeftThumbstick, EKeys::Gamepad_RightThumbstick };
    auto Register = [](FKey Key, uint32 Flags)
    {
        const FString Name = FString(SonyPrefix) + Key.ToString();
        EKeys::AddKey(FKeyDetails(FKey(*Name), FText::FromString(Name), Flags));
    };
    for (FKey Key : Axes)
    {
        Register(Key, FKeyDetails::GamepadKey | FKeyDetails::Axis1D);
    }
    for (FKey Key : Buttons)
    {
        Register(Key, FKeyDetails::GamepadKey);
    }
}

FString ULocalMultiplayerViewportClient::GetGamepadSource(bool bSony, FInputDeviceId Device) const
{
    if (bSony)
    {
        // RawInput emits user/device zero for every HID controller. Its polling
        // scope still identifies the physical controller, including two DS4s.
        if (const FInputDeviceScope* Scope = FInputDeviceScope::GetCurrent())
        {
            if (!Scope->HardwareDeviceIdentifier.IsEmpty())
            {
                return TEXT("Sony:") + Scope->HardwareDeviceIdentifier;
            }
            return FString::Printf(TEXT("Sony:handle:%d"), Scope->HardwareDeviceHandle);
        }
        return FString::Printf(TEXT("Sony:device:%d"), Device.GetId());
    }
    return FString::Printf(TEXT("Gamepad:device:%d"), Device.GetId());
}

APlayerController* ULocalMultiplayerViewportClient::GetAssignedPlayer(const FString& Source) const
{
    const UGameInstance* Instance = GetGameInstance();
    const int32* Index = GamepadAssignments.Find(Source);
    if (!Instance || !Index || !Instance->GetLocalPlayers().IsValidIndex(*Index))
    {
        return nullptr;
    }
    return Instance->GetLocalPlayers()[*Index]->GetPlayerController(GetWorld());
}

bool ULocalMultiplayerViewportClient::AssignGamepad(const FString& Source)
{
    const UGameInstance* Instance = GetGameInstance();
    if (!Instance || Instance->GetNumLocalPlayers() < 2 || GamepadAssignments.Contains(Source))
    {
        return false;
    }
    if (GamepadAssignments.Num() < 2)
    {
        const int32 Index = GamepadAssignments.Num();
        GamepadAssignments.Add(Source, Index);
        UE_LOG(LogGamepadAssignment, Display, TEXT("Gamepad joined player %d: %s"), Index + 1, *Source);
        return true;
    }
    return false;
}

void ULocalMultiplayerViewportClient::ResetGamepadAssignments()
{
    GamepadAssignments.Reset();
    if (UGameInstance* Instance = GetGameInstance())
    {
        for (ULocalPlayer* Player : Instance->GetLocalPlayers())
        {
            if (APlayerController* Controller = Player->GetPlayerController(GetWorld()))
            {
                Controller->FlushPressedKeys();
            }
        }
    }
}

void ULocalMultiplayerViewportClient::PostRender(UCanvas* Canvas)
{
    Super::PostRender(Canvas);
    UGameInstance* Instance = GetGameInstance();
    if (!Canvas || !Instance || !GetWorld() || !GetWorld()->IsGameWorld())
    {
        return;
    }
    for (int32 Index = 0; Index < Instance->GetNumLocalPlayers() && Index < 2; ++Index)
    {
        if (GamepadAssignments.Num() > Index)
        {
            continue;
        }
        ULocalPlayer* Player = Instance->GetLocalPlayers()[Index];
        const FString Prompt = FString::Printf(TEXT("ИГРОК %d: нажмите A (Xbox) / крестик (PS4)"), Index + 1);
        const float X = Player->Origin.X * Canvas->SizeX + 20.f;
        const float Y = Player->Origin.Y * Canvas->SizeY + 30.f;
        Canvas->SetDrawColor(FColor::Yellow);
        Canvas->DrawText(GEngine->GetMediumFont(), Prompt, X, Y);
        Canvas->DrawText(GEngine->GetSmallFont(), TEXT("Первое подключение — игрок 1. F2 — выбрать геймпады заново."), X, Y + 25.f);
    }
}

bool ULocalMultiplayerViewportClient::InputKey(const FInputKeyEventArgs& Args)
{
    if (Args.Key == EKeys::F2 && Args.Event == IE_Pressed && !IgnoreInput())
    {
        ResetGamepadAssignments();
        return true;
    }
    const FKey SonyKey = TranslateSonyKey(Args.Key);
    const bool bSony = SonyKey.IsValid();
    const FKey Key = bSony ? SonyKey : Args.Key;
    if (!bSony && !Key.ToString().StartsWith(TEXT("Gamepad_")))
    {
        return Super::InputKey(Args);
    }
    if (!IgnoreInput())
    {
        const FString Source = GetGamepadSource(bSony, Args.InputDevice);
        if (Key == EKeys::Gamepad_FaceButton_Bottom && Args.Event == IE_Pressed
            && AssignGamepad(Source))
        {
            return true;
        }
        if (APlayerController* Player = GetAssignedPlayer(Source))
        {
#if UE_VERSION_OLDER_THAN(5, 6, 0)
            Player->InputKey(FInputKeyParams(Key, Args.Event, Args.AmountDepressed, true, Args.InputDevice));
#else
            const FInputKeyEventArgs PlayerArgs(
                Args.Viewport,
                Args.InputDevice,
                Key,
                Args.Event,
                Args.AmountDepressed,
                Args.bIsTouchEvent,
                Args.EventTimestamp
            );

            Player->InputKey(PlayerArgs);
#endif
        }
    }
    // An unassigned/third gamepad must never fall through to player one.
    return true;
}


#if UE_VERSION_OLDER_THAN(5, 6, 0)
bool ULocalMultiplayerViewportClient::InputAxis(FViewport* InViewport, FInputDeviceId Device,
    FKey AxisKey, float Delta, float DeltaTime, int32 NumSamples, bool bGamepad)
#else
bool ULocalMultiplayerViewportClient::InputAxis(const FInputKeyEventArgs& Args)
#endif
{
#if !UE_VERSION_OLDER_THAN(5, 6, 0)
    const FInputDeviceId Device = Args.InputDevice;
    const FKey AxisKey = Args.Key;
#endif
    const FKey SonyKey = TranslateSonyKey(AxisKey);
    const bool bSony = SonyKey.IsValid();
    const FKey Key = bSony ? SonyKey : AxisKey;

    // Обычный ввод клавиатуры и мыши передаём движку.
    if (!bSony && !Key.ToString().StartsWith(TEXT("Gamepad_")))
    {
#if UE_VERSION_OLDER_THAN(5, 6, 0)
        return Super::InputAxis(InViewport, Device, AxisKey, Delta, DeltaTime, NumSamples, bGamepad);
#else
        return Super::InputAxis(Args);
#endif
    }

    if (!IgnoreInput())
    {
        const FString Source = GetGamepadSource(bSony, Device);

        if (APlayerController* Player = GetAssignedPlayer(Source))
        {
#if UE_VERSION_OLDER_THAN(5, 6, 0)
            Player->InputKey(FInputKeyParams(Key, Delta, DeltaTime, NumSamples, true, Device));
#else
            const FInputKeyEventArgs PlayerArgs(
                Args.Viewport,
                Args.InputDevice,
                Key,
                Args.AmountDepressed,
                Args.DeltaTime,
                Args.NumSamples,
                Args.EventTimestamp
            );

            Player->InputKey(PlayerArgs);
#endif
        }
    }

    // Не передаём ввод незарегистрированного геймпада игроку 1.
    return true;
}

