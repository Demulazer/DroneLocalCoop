#pragma once
#include "Commandlets/Commandlet.h"
#include "BuildThrowablesCommandlet.generated.h"

// Editor-only authoring tool. Gameplay lives in the generated Blueprint graphs.
UCLASS()
class UBuildThrowablesCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    virtual int32 Main(const FString& Params) override;
};
