#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "Misc/EngineVersionComparison.h"
#include "LocalMultiplayerViewportClient.generated.h"

/** Assigns physical gamepads to local players in join order, regardless of brand. */
UCLASS()
class DRONESSETUP_API ULocalMultiplayerViewportClient : public UGameViewportClient
{
    GENERATED_BODY()
public:
    virtual bool InputKey(const FInputKeyEventArgs& Args) override;
#if UE_VERSION_OLDER_THAN(5, 6, 0)
    virtual bool InputAxis(FViewport* InViewport, FInputDeviceId Device, FKey Key,
        float Delta, float DeltaTime, int32 NumSamples = 1, bool bGamepad = false) override;
#else
    virtual bool InputAxis(const FInputKeyEventArgs& Args) override;
#endif
    static void RegisterSonyKeys();
    virtual void PostRender(UCanvas* Canvas) override;
    void ResetGamepadAssignments();
private:
    FString GetGamepadSource(bool bSony, FInputDeviceId Device) const;
    APlayerController* GetAssignedPlayer(const FString& Source) const;
    bool AssignGamepad(const FString& Source);
    TMap<FString, int32> GamepadAssignments;
};
