#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArcadeCoastingComponent.generated.h"

/** Omnidirectional arcade drive. Kept under its original name for asset compatibility. */
UCLASS(ClassGroup=(Vehicle), meta=(BlueprintSpawnableComponent))
class DRONESSETUP_API UArcadeCoastingComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UArcadeCoastingComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;
    float GetMovementReferenceYaw() const;

private:
    // A camera boom independent of body yaw avoids a feedback loop while turning.
    UPROPERTY(Transient)
    TObjectPtr<class USpringArmComponent> MovementCameraArm;
    float InitialReferenceYaw = 0.f;
    UPROPERTY(Transient)
    TObjectPtr<class UInputAction> ThrottleAction;
    UPROPERTY(Transient)
    TObjectPtr<class UInputAction> BrakeAction;
    UPROPERTY(Transient)
    TObjectPtr<class UInputAction> SteeringAction;
};
