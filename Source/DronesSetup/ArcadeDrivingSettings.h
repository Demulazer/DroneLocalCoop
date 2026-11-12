#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ArcadeDrivingSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Arcade Driving"))
class DRONESSETUP_API UArcadeDrivingSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    virtual FName GetCategoryName() const override { return TEXT("Game"); }

    /** Maximum planar speed in any direction, including diagonals. */
    UPROPERTY(Config, EditAnywhere, Category="Movement", meta=(ClampMin="0.0", Units="cm/s"))
    float MaxSpeed = 1800.f;

    /** Velocity response while moving; higher values give tighter direction changes. */
    UPROPERTY(Config, EditAnywhere, Category="Movement", meta=(ClampMin="0.0"))
    float MovementResponse = 12.f;

    /** How quickly the body faces the requested movement direction. */
    UPROPERTY(Config, EditAnywhere, Category="Turning", meta=(ClampMin="0.0"))
    float TurnResponse = 14.f;

    /** Maximum yaw speed, independent of forward speed or wheel steering. */
    UPROPERTY(Config, EditAnywhere, Category="Turning", meta=(ClampMin="0.0", Units="deg/s"))
    float MaxTurnSpeed = 720.f;

    /** Allow horizontal steering during jumps without changing gravity. */
    UPROPERTY(Config, EditAnywhere, Category="Movement", meta=(ClampMin="0.0", ClampMax="1.0"))
    float AirControl = 0.35f;

    UPROPERTY(Config, EditAnywhere, Category="Coasting")
    bool bEnableArcadeCoasting = true;

    /** Higher = faster stopping when neither forward nor reverse is held. */
    UPROPERTY(Config, EditAnywhere, Category="Coasting", meta=(ClampMin="0.0", UIMax="20.0"))
    float CoastDecayRate = 10.f;

    /** Snap the remaining horizontal speed to zero below this value, in cm/s. */
    UPROPERTY(Config, EditAnywhere, Category="Coasting", meta=(ClampMin="0.0", Units="cm/s"))
    float StopSpeed = 5.f;
};
