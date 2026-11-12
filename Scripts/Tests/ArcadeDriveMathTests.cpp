#include "../../Source/DronesSetup/ArcadeDriveMath.h"
#include <cstdio>
#include <cstdlib>

int main()
{
    int Checks = 0;
    const auto Check = [&Checks](bool Condition, const char* Name)
    {
        ++Checks;
        if (!Condition)
        {
            std::fprintf(stderr, "FAIL: %s\n", Name);
            std::exit(1);
        }
    };
    constexpr double Pi = 3.14159265358979323846;
    const double Inputs[8][2] = {{1,0}, {1,1}, {0,1}, {-1,1}, {-1,0}, {-1,-1}, {0,-1}, {1,-1}};
    for (double Reference : {-170.0, 0.0, 30.0, 90.0, 179.0})
    {
        for (int Index = 0; Index < 8; ++Index)
        {
            const auto D = ArcadeDrive::Direction(Inputs[Index][0], Inputs[Index][1], Reference);
            Check(std::abs(std::hypot(D.X, D.Y) - 1.0) < 1e-9, "cardinal/diagonal speed");
            const double Angle = std::atan2(D.Y, D.X) * 180.0 / Pi;
            Check(std::abs(std::remainder(Angle - Reference - Index * 45, 360.0)) < 1e-8, "camera-relative direction");
        }
    }
    const auto Analog = ArcadeDrive::Direction(0.3, 0.4, 45);
    Check(std::abs(std::hypot(Analog.X, Analog.Y) - 0.5) < 1e-9, "analog magnitude");
    const auto Neutral = ArcadeDrive::Direction(0, 0, 120);
    Check(Neutral.X == 0 && Neutral.Y == 0, "neutral input");
    Check(ArcadeDrive::YawVelocity(179, -179, 14, 720, 1.f/60) > 0, "shortest positive arc");
    Check(ArcadeDrive::YawVelocity(-179, 179, 14, 720, 1.f/60) < 0, "shortest negative arc");
    Check(ArcadeDrive::YawVelocity(0, 90, 14, 720, 0) == 0, "zero timestep");
    Check(ArcadeDrive::YawVelocity(0, 90, 14, 0, 0.016f) == 0, "zero turn limit");
    Check(ArcadeDrive::YawVelocity(0, 90, 0, 720, 0.016f) == 0, "zero response");
    Check(ArcadeDrive::YawVelocity(0, 90, 14, 720, 0.5f) * 0.5f <= 90, "hitch does not overshoot");
    for (int FPS : {30, 60, 120})
    {
        for (float Target : {-180.f, -135.f, -90.f, -45.f, 0.f, 45.f, 90.f, 135.f, 180.f})
        {
            float Yaw = 0;
            const float Dt = 1.f / FPS;
            for (int Frame = 0; Frame < FPS * 3; ++Frame)
            {
                const float OldError = std::abs(std::remainder(Target - Yaw, 360.f));
                const float Rate = ArcadeDrive::YawVelocity(Yaw, Target, 14, 720, Dt);
                Check(std::abs(Rate) <= 720, "turn rate capped");
                Yaw += Rate * Dt;
                Check(std::abs(std::remainder(Target - Yaw, 360.f)) <= OldError + 1e-4f, "converges without oscillation");
                if (Frame == FPS - 1)
                {
                    Check(std::abs(std::remainder(Target - Yaw, 360.f)) < 0.1f, "turn completed within one second");
                }
            }
            Check(std::abs(std::remainder(Target - Yaw, 360.f)) < 0.1f, "held direction stops spinning");
            // Reverse the command without resetting the body's heading.
            const float Reversed = Target + 180.f;
            for (int Frame = 0; Frame < FPS; ++Frame)
            {
                Yaw += ArcadeDrive::YawVelocity(Yaw, Reversed, 14, 720, Dt) * Dt;
            }
            Check(std::abs(std::remainder(Reversed - Yaw, 360.f)) < 0.1f, "180-degree reversal");
        }
    }
    std::printf("PASS: %d checks (8 directions, 5 camera headings, analog input, wrap, reversal, 30/60/120 FPS)\n", Checks);
    return 0;
}
