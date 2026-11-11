#include "Modules/ModuleManager.h"
#include "LocalMultiplayerViewportClient.h"

class FDronesSetupModule : public FDefaultGameModuleImpl
{
public:
    virtual void StartupModule() override
    {
        ULocalMultiplayerViewportClient::RegisterSonyKeys();
    }
};

IMPLEMENT_PRIMARY_GAME_MODULE(FDronesSetupModule, DronesSetup, "DronesSetup");
