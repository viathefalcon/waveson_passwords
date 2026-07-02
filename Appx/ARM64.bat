%echo off
COPY ..\ARM64\Release\WPG.exe Layout\WPG.exe
COPY ..\ARM64\Release\wpg_core.dll Layout\wpg_core.dll
makeappx pack /d Layout /p WPG.ARM64.msix
