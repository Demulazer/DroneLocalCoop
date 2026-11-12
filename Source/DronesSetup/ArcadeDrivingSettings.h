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

    UPROPERTY(Config, EditAnywhere, Category="Coasting")
    bool bEnableArcadeCoasting = true;

    /** Higher = faster stopping when neither forward nor reverse is held. */
    UPROPERTY(Config, EditAnywhere, Category="Coasting", meta=(ClampMin="0.0", UIMax="20.0"))
    float CoastDecayRate = 10.f;

    /** Snap the remaining horizontal speed to zero below this value, in cm/s. */
    UPROPERTY(Config, EditAnywhere, Category="Coasting", meta=(ClampMin="0.0", Units="cm/s"))
    float StopSpeed = 5.f;
};
