param([string]$EngineRoot = 'C:/Program Files/Epic Games/UE_5.6')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$projectPath = Join-Path $projectRoot 'DronesSetup.uproject'
$editorPath = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$previousCache = [Environment]::GetEnvironmentVariable('UE-LocalDataCachePath', 'Process')
try {
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', (Join-Path $projectRoot 'DerivedDataCache'), 'Process')
    foreach ($map in @('VehicleExampleMap', 'VehicleOffroadExampleMap')) {
        $logPath = Join-Path $projectRoot "Saved/Logs/$map-SplitScreen.log"
        $started = Get-Date
        # Synthetic input must be isolated from live hardware (including stick drift).
        & $editorPath $projectPath "/Game/VehicleTemplate/Maps/$map" -game -SplitScreenTest '-DisablePlugins=RawInput,XInputDevice' -unattended -nullrhi -nosound -UTF8Output -DDC=InstalledNoZenLocalFallback "-abslog=$logPath"
        $exitCode = $LASTEXITCODE
        if (!(Test-Path -LiteralPath $logPath) -or (Get-Item -LiteralPath $logPath).LastWriteTime -lt $started) {
            throw "Unreal did not produce a fresh test log on $map (exit $exitCode). Tests did not run."
        }
        $logText = Get-Content -LiteralPath $logPath -Raw
        if ($exitCode -ne 0 -or $logText -notmatch 'Split-screen validation: PASS' -or $logText -notmatch 'Gamepad assignment validation: PASS' -or $logText -notmatch 'Stick movement validation: PASS' -or $logText -notmatch 'Arcade coasting validation: PASS' -or $logText -notmatch 'Omnidirectional movement validation: PASS' -or $logText -match 'Accessed None|Error:') {
            throw "Split-screen check failed on $map. See $logPath"
        }
        Write-Host "$map : PASS"
    }
}
finally {
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', $previousCache, 'Process')
}
