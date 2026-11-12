#include "ArcadeCoastingComponent.h"

#include "ArcadeDrivingSettings.h"
#include "ArcadeDriveMath.h"
#include "Camera/CameraComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"

UArcadeCoastingComponent::UArcadeCoastingComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UArcadeCoastingComponent::BeginPlay()
{
    Super::BeginPlay();
    InitialReferenceYaw = GetOwner()->GetActorRotation().Yaw;
    // Preserve the view at activation, but stop the chase camera from inheriting
    // the body's turn. Existing look-around may still rotate the boom explicitly.
    TArray<USpringArmComponent*> Arms;
    GetOwner()->GetComponents(Arms);
    for (auto* Arm : Arms)
    {
        const FRotator ViewRotation = Arm->GetTargetRotation();
        Arm->SetUsingAbsoluteRotation(true);
        Arm->SetWorldRotation(ViewRotation);
        if (!MovementCameraArm || Arm->TargetArmLength > MovementCameraArm->TargetArmLength)
        {
            MovementCameraArm = Arm;
        }
    }
    if (MovementCameraArm)
    {
        InitialReferenceYaw = MovementCameraArm->GetTargetRotation().Yaw;
    }
    else if (auto* Camera = GetOwner()->FindComponentByClass<UCameraComponent>())
    {
        // A directly attached camera must also remain independent of body yaw.
        const FRotator ViewRotation = Camera->GetComponentRotation();
        Camera->SetUsingAbsoluteRotation(true);
        Camera->SetWorldRotation(ViewRotation);
        InitialReferenceYaw = ViewRotation.Yaw;
    }
    ThrottleAction = LoadObject<UInputAction>(nullptr,
        TEXT("/Game/VehicleTemplate/Input/Actions/IA_Throttle.IA_Throttle"));
    BrakeAction = LoadObject<UInputAction>(nullptr,
        TEXT("/Game/VehicleTemplate/Input/Actions/IA_Brake.IA_Brake"));
    SteeringAction = LoadObject<UInputAction>(nullptr,
        TEXT("/Game/VehicleTemplate/Input/Actions/IA_Steering.IA_Steering"));
    if (auto* Vehicle = GetOwner()->FindComponentByClass<UChaosWheeledVehicleMovementComponent>())
    {
        // Keep suspension and collision, but remove engine and tyre forces that
        // otherwise fight strafing. Run before the vehicle submits physics input.
        Vehicle->EnableMechanicalSim(false);
        Vehicle->EnableWheelFriction(false);
        Vehicle->AddTickPrerequisiteComponent(this);
    }
    ensureMsgf(ThrottleAction && BrakeAction && SteeringAction,
        TEXT("Arcade movement requires the template movement Input Actions."));
}

float UArcadeCoastingComponent::GetMovementReferenceYaw() const
{
    // Keep the chase camera as the reference when switching to a cockpit camera.
    return MovementCameraArm ? MovementCameraArm->GetTargetRotation().Yaw : InitialReferenceYaw;
}

void UArcadeCoastingComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    auto* Pawn = Cast<APawn>(GetOwner());
    auto* Vehicle = GetOwner()->FindComponentByClass<UChaosWheeledVehicleMovementComponent>();
    auto* Body = Vehicle ? Cast<UPrimitiveComponent>(Vehicle->UpdatedComponent) : nullptr;
    if (!Pawn || !Vehicle || !Body || !Body->IsSimulatingPhysics() || DeltaTime <= 0.f)
    {
        return;
    }
    auto* Controller = Cast<APlayerController>(Pawn->GetController());
    auto* Input = Controller && Controller->IsLocalController()
        && !Controller->IsMoveInputIgnored()
        ? Cast<UEnhancedPlayerInput>(Controller->PlayerInput) : nullptr;
    const auto Read = [Input](const UInputAction* Action)
    {
        return Input && Action ? Input->GetActionValue(Action).Get<float>() : 0.f;
    };
    // Read actions, not the filtered Chaos throttle/brake: those carry reverse-
    // gear delays and cannot describe a two-dimensional movement request.
    FVector2D Stick(Read(ThrottleAction) - Read(BrakeAction), Read(SteeringAction));
    if (Vehicle->GetHandbrakeInput())
    {
        Stick = FVector2D::ZeroVector;
    }
    const float Magnitude = Stick.Size();
    if (Magnitude > 1.f)
    {
        Stick /= Magnitude;
    }

    // Blueprint handlers can still write these inputs; they no longer drive the
    // vehicle. Clear them after input dispatch, before Chaos' component tick.
    Vehicle->SetThrottleInput(0.f);
    Vehicle->SetBrakeInput(0.f);
    Vehicle->SetSteeringInput(0.f);

    bool bGrounded = false;
    for (int32 Wheel = 0; Wheel < Vehicle->GetNumWheels(); ++Wheel)
    {
        // Update the live simulation too, including after a Blueprint reset
        // recreates physics. Enable flags alone are insufficient in some UE versions.
        Vehicle->SetWheelFrictionMultiplier(Wheel, 0.f);
        bGrounded |= Vehicle->GetWheelState(Wheel).bInContact;
    }
    const auto* Settings = GetDefault<UArcadeDrivingSettings>();
    const bool bMoving = !Stick.IsNearlyZero();
    const auto Planar = ArcadeDrive::Direction(Stick.X, Stick.Y, GetMovementReferenceYaw());
    const FVector Direction(Planar.X, Planar.Y, 0.f);
    FVector AngularVelocity = Body->GetPhysicsAngularVelocityInDegrees();
    AngularVelocity.Z = bMoving ? ArcadeDrive::YawVelocity(
        Pawn->GetActorRotation().Yaw, Direction.Rotation().Yaw,
        Settings->TurnResponse, Settings->MaxTurnSpeed, DeltaTime) : 0.f;
    // Turn the physical body, not just its visual mesh. Retain roll/pitch physics.
    Body->SetPhysicsAngularVelocityInDegrees(AngularVelocity);
    if (!bMoving && (!Settings->bEnableArcadeCoasting || !bGrounded))
    {
        return;
    }
    // Translation starts immediately, even at a 90/180-degree heading error.
    // The body catches up without forcing a car-like circular trajectory.
    const FVector Target = Direction * FMath::Max(0.f, Settings->MaxSpeed);
    FVector Velocity = Body->GetPhysicsLinearVelocity();
    const float Rate = bMoving ? Settings->MovementResponse : Settings->CoastDecayRate;
    const float Control = bGrounded ? 1.f : FMath::Clamp(Settings->AirControl, 0.f, 1.f);
    const float Alpha = 1.f - FMath::Exp(-FMath::Max(0.f, Rate) * Control * DeltaTime);
    Velocity.X = FMath::Lerp(Velocity.X, Target.X, Alpha);
    Velocity.Y = FMath::Lerp(Velocity.Y, Target.Y, Alpha);
    if (!bMoving && Velocity.SizeSquared2D() < FMath::Square(Settings->StopSpeed))
    {
        Velocity.X = Velocity.Y = 0.f;
    }
    // Physics still resolves walls, impacts, suspension and vertical motion.
    Body->SetPhysicsLinearVelocity(Velocity);
}
