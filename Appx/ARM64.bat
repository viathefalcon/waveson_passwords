%echo off
COPY ..\ARM64\Release\WPG.exe Layout\WPG.exe
makeappx pack /d Layout /p WPG.ARM64.msix
