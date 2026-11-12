@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 Scripts\Tests\ArcadeDriveMathTests.cpp /Fe:Saved\Tests\ArcadeDriveMathTests.exe /Fo:Saved\Tests\ArcadeDriveMathTests.obj
if errorlevel 1 exit /b 1
Saved\Tests\ArcadeDriveMathTests.exe
