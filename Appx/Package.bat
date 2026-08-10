@echo off
if "%~1"=="" (
	echo Usage: %~nx0 ^<architecture^>
	exit /b 1
)

set "ARCHITECTURE=%~1"
for /f "usebackq delims=" %%A in (`powershell -NoProfile -Command "$env:ARCHITECTURE.ToLowerInvariant()"`) do set "ARCHITECTURE_LOWER=%%A"
COPY ..\%ARCHITECTURE%\Release\WPG.exe Layout\WPG.exe
COPY ..\%ARCHITECTURE%\Release\Core.dll Layout\Core.dll

powershell -NoProfile -Command "$path = (Resolve-Path 'Layout\AppxManifest.xml').Path; [xml]$manifest = Get-Content -LiteralPath $path; $manifest.Package.Identity.ProcessorArchitecture = $env:ARCHITECTURE_LOWER; $manifest.Save($path)"
if errorlevel 1 exit /b 1

mkdir /p Packages
makeappx pack /d Layout /p .\Packages\WPG.%ARCHITECTURE%.msix
