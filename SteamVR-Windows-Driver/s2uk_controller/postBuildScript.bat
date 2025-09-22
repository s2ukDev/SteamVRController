@echo off

:: Get Steam directory
for /f "tokens=2*" %%A in ('reg query "HKLM\SOFTWARE\WOW6432Node\Valve\Steam" /v InstallPath 2^>nul') do (
    set STEAM_PATH=%%B
)

if not defined STEAM_PATH (
    echo Steam installation path wasn't' found in registry!
    echo Make sure you have Steam installed!
    pause
    exit /b
)

set STEAMVR_DIR=%STEAM_PATH%\steamapps\common\SteamVR
set SRC_DIR=%cd%\..\s2uk_controller\resources
set DEST_DIR=%STEAMVR_DIR%\drivers\s2ukController\resources
set BINARIES_PATH=%STEAMVR_DIR%\drivers\s2ukController\bin\win64
set DLL_PATH=%cd%\..\x64\Release

echo Copying driver resources from: %SRC_DIR% to: %DEST_DIR%

:: Create output folders
if not exist "%BINARIES_PATH%" mkdir "%BINARIES_PATH%"
if not exist "%DEST_DIR%" mkdir "%DEST_DIR%"

:: Copy resorces
xcopy "%SRC_DIR%\*" "%DEST_DIR%\" /E /Y /I

:: Move driver.vrdrivermanifest one dir up
if exist "%DEST_DIR%\driver.vrdrivermanifest" (
    move /Y "%DEST_DIR%\driver.vrdrivermanifest" "%STEAMVR_DIR%\drivers\s2ukController\"
)

echo Copying driver DLL from: %DLL_PATH% to %BINARIES_PATH%

:: Copy dll
xcopy "%DLL_PATH%\driver_s2ukController.dll" "%BINARIES_PATH%\" /E /Y /I

echo.
echo Done!

:: doing a "PAUSE" isn't the best idea!
timeout /t 2 /nobreak >nul

:: we need to run "EXIT" because we are running from MSBUILD's conhost.exe!
exit