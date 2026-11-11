#include "StickHalfAxisModifier.h"

FInputActionValue UStickHalfAxisModifier::ModifyRaw_Implementation(
    const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime)
{
    const float Directed = CurrentValue.Get<float>() * (bNegativeDirection ? -1.f : 1.f);
    const float Threshold = FMath::Clamp(DeadZone, 0.f, 0.95f);
    const float Value = FMath::Clamp((Directed - Threshold) / (1.f - Threshold), 0.f, 1.f);
    return FInputActionValue(FMath::Pow(Value, FMath::Max(Exponent, 0.1f)));
}
