#include "ArcadeCoastingComponent.h"

#include "ArcadeDrivingSettings.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/PrimitiveComponent.h"

UArcadeCoastingComponent::UArcadeCoastingComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UArcadeCoastingComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    const auto* Settings = GetDefault<UArcadeDrivingSettings>();
    auto* Vehicle = GetOwner()->FindComponentByClass<UChaosWheeledVehicleMovementComponent>();
    if (!Settings->bEnableArcadeCoasting || !Vehicle
        || FMath::Abs(Vehicle->GetThrottleInput()) > 0.01f
        || FMath::Abs(Vehicle->GetBrakeInput()) > 0.01f)
    {
        return;
    }
    bool bGrounded = false;
    for (int32 Wheel = 0; Wheel < Vehicle->GetNumWheels(); ++Wheel)
    {
        bGrounded |= Vehicle->GetWheelState(Wheel).bInContact;
    }
    auto* Body = Cast<UPrimitiveComponent>(Vehicle->UpdatedComponent);
    if (!bGrounded || !Body || !Body->IsSimulatingPhysics())
    {
        return;
    }
    FVector Velocity = Body->GetPhysicsLinearVelocity();
    // Exponential decay has the same response at different frame rates.
    const float Factor = FMath::Exp(-FMath::Max(0.f, Settings->CoastDecayRate) * DeltaTime);
    Velocity.X *= Factor;
    Velocity.Y *= Factor;
    if (Velocity.SizeSquared2D() < FMath::Square(Settings->StopSpeed))
    {
        Velocity.X = Velocity.Y = 0.f;
    }
    // Preserve vertical velocity and disable this assistance in the air.
    Body->SetPhysicsLinearVelocity(Velocity);
}

