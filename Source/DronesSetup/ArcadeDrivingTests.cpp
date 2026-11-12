#include "CoreMinimal.h"
#include "ArcadeDriveMath.h"
#include "Misc/AutomationTest.h"

namespace
{
    FVector TestDirection(FVector2D Input, float Yaw)
    {
        const auto Result = ArcadeDrive::Direction(Input.X, Input.Y, Yaw);
        return FVector(Result.X, Result.Y, 0);
    }
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcadeDirectionTest, "Drones.Arcade.Direction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FArcadeDirectionTest::RunTest(const FString& Parameters)
{
    const FVector2D Inputs[] = {{1,0}, {1,1}, {0,1}, {-1,1}, {-1,0}, {-1,-1}, {0,-1}, {1,-1}};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Inputs); ++Index)
    {
        const FVector Direction = TestDirection(Inputs[Index], 30.f);
        TestTrue(TEXT("All eight directions retain full speed"), FMath::IsNearlyEqual(Direction.Size(), 1.0, 0.0001));
        TestTrue(TEXT("Camera-relative angle"), FMath::Abs(FMath::FindDeltaAngleDegrees(
            Direction.Rotation().Yaw, 30.f + Index * 45.f)) < 0.01f);
    }
    TestTrue(TEXT("Analog magnitude retained"), FMath::IsNearlyEqual(TestDirection({0.3,0.4}, 90).Size(), 0.5, 0.0001));
    TestTrue(TEXT("Neutral remains zero"), TestDirection({}, 90).IsNearlyZero());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcadeTurnTest, "Drones.Arcade.Turning",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FArcadeTurnTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Wrap uses shortest arc"), ArcadeDrive::YawVelocity(179, -179, 14, 720, 1.f/60) > 0);
    TestTrue(TEXT("Opposite wrap uses shortest arc"), ArcadeDrive::YawVelocity(-179, 179, 14, 720, 1.f/60) < 0);
    TestEqual(TEXT("Zero delta is safe"), ArcadeDrive::YawVelocity(0, 90, 14, 720, 0), 0.f);
    TestTrue(TEXT("Hitch cannot overshoot"), FMath::Abs(ArcadeDrive::YawVelocity(0, 90, 14, 720, 0.5f) * 0.5f) <= 90.f);
    for (const int32 FPS : {30, 60, 120})
    {
        for (const float Target : {-180.f, -135.f, -90.f, -45.f, 0.f, 45.f, 90.f, 135.f, 180.f})
        {
            float Yaw = 0.f;
            const float Dt = 1.f / FPS;
            for (int32 Frame = 0; Frame < FPS * 2; ++Frame)
            {
                const float Speed = ArcadeDrive::YawVelocity(Yaw, Target, 14, 720, Dt);
                TestTrue(TEXT("Angular speed capped"), FMath::Abs(Speed) <= 720.f);
                Yaw += Speed * Dt;
            }
            TestTrue(TEXT("Held direction converges without circling"),
                FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, Target)) < 0.1f);
        }
    }
    // Turning the body must not rotate the desired movement vector with it.
    const FVector Right = TestDirection({0,1}, 0);
    TestTrue(TEXT("Right is a fixed world direction after the body turns"), Right.Equals(FVector(0,1,0), 0.0001));
    return true;
}
#endif
