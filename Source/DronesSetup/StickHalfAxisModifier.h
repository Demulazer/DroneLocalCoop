#pragma once

#include "CoreMinimal.h"
#include "InputModifiers.h"
#include "StickHalfAxisModifier.generated.h"

/** Turns one direction of a signed stick axis into a positive pedal value. */
UCLASS(NotBlueprintable, meta=(DisplayName="Stick Half Axis"))
class DRONESSETUP_API UStickHalfAxisModifier : public UInputModifier
{
    GENERATED_BODY()
public:
    /** Off: forward/up. On: backward/down. The opposite direction produces zero. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stick")
    bool bNegativeDirection = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stick", meta=(ClampMin="0.0", ClampMax="0.95"))
    float DeadZone = 0.15f;

    /** 1 = linear; greater than 1 gives finer control near the centre. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stick", meta=(ClampMin="0.1", ClampMax="5.0"))
    float Exponent = 1.f;

protected:
    virtual FInputActionValue ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput,
        FInputActionValue CurrentValue, float DeltaTime) override;
};
