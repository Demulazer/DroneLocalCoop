#pragma once

#include <algorithm>
#include <cmath>

// No engine dependency: the very same calculations can be tested without
// starting the editor. X = forward, Y = right in the camera reference frame.
namespace ArcadeDrive
{
    struct FDirection
    {
        double X;
        double Y;
    };

    inline FDirection Direction(double Forward, double Right, double ReferenceYaw)
    {
        constexpr double DegreesToRadians = 0.017453292519943295;
        const double Scale = std::max(1.0, std::hypot(Forward, Right));
        const double Angle = ReferenceYaw * DegreesToRadians;
        return {(Forward * std::cos(Angle) - Right * std::sin(Angle)) / Scale,
                (Forward * std::sin(Angle) + Right * std::cos(Angle)) / Scale};
    }

    inline float YawVelocity(float CurrentYaw, float TargetYaw, float Response,
        float MaxSpeed, float DeltaTime)
    {
        if (DeltaTime <= 0.f)
        {
            return 0.f;
        }
        const float Error = std::remainder(TargetYaw - CurrentYaw, 360.f);
        // Exponential convergence, shortest arc, and no overshoot on long frames.
        const float Step = Error * (1.f - std::exp(-std::max(0.f, Response) * DeltaTime));
        const float Limit = std::max(0.f, MaxSpeed);
        return std::clamp(Step / DeltaTime, -Limit, Limit);
    }
}
