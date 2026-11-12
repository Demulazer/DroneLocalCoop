#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "LocalMultiplayerViewportClient.generated.h"

/** Assigns physical gamepads to local players in join order, regardless of brand. */
UCLASS()
class DRONESSETUP_API ULocalMultiplayerViewportClient : public UGameViewportClient
{
    GENERATED_BODY()
public:
    virtual bool InputKey(const FInputKeyEventArgs& Args) override;
    virtual bool InputAxis(const FInputKeyEventArgs& Args) override;
    static void RegisterSonyKeys();
    virtual void PostRender(UCanvas* Canvas) override;
    void ResetGamepadAssignments();
private:
    FString GetGamepadSource(bool bSony, FInputDeviceId Device) const;
    APlayerController* GetAssignedPlayer(const FString& Source) const;
    bool AssignGamepad(const FString& Source);
    TMap<FString, int32> GamepadAssignments;
};
