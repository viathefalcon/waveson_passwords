%echo off
COPY ..\ARM64\Release\WPG.exe Layout\WPG.exe
COPY ..\ARM64\Release\Core.dll Layout\Core.dll
makeappx pack /d Layout /p WPG.ARM64.msix
