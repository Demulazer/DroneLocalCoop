#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LocalMultiplayerSubsystem.generated.h"

/** Creates the second couch co-op player without replacing the vehicle Blueprint game modes. */
UCLASS()
class DRONESSETUP_API ULocalMultiplayerSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void OnWorldBeginPlay(UWorld& World) override;
    virtual void Deinitialize() override;
protected:
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
private:
    void SetupPlayers();
    void VerifyPlayers();
    void StartGamepadRoutingTest();
    void VerifyGamepadRouting();
    void StartStickMovementTest();
    void VerifyStickMovement();
    void StartCoastingTest();
    void VerifyCoasting();
    TArray<FVector> CoastingStartPositions;
    int32 StickTestPhase = 0;
    int32 RoutingTestCase = 0;
    bool bLayoutValid = false;
    FTimerHandle SetupTimer;
    FTimerHandle VerifyTimer;
    int32 SetupAttempts = 0;
};
