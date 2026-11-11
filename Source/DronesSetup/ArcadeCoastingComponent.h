#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArcadeCoastingComponent.generated.h"

/** Quickly removes coasting on the ground without applying reverse throttle. */
UCLASS(ClassGroup=(Vehicle), meta=(BlueprintSpawnableComponent))
class DRONESSETUP_API UArcadeCoastingComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UArcadeCoastingComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;
};
