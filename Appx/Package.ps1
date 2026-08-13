param(
    [Parameter(Mandatory = $true)]
    [string]$Architecture,

    [Parameter(Mandatory = $true)]
    [string]$Version
)

$ArchitectureLower = $Architecture.ToLowerInvariant()

msbuild '..\waveson_passwords.sln' /t:Build /p:Configuration=Release /p:Platform=$Architecture
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$tempLayout = Join-Path ([System.IO.Path]::GetTempPath()) ([System.Guid]::NewGuid().ToString())
Copy-Item 'Layout' $tempLayout -Recurse -Force

try {
    Copy-Item "..\$Architecture\Release\WPG.exe" (Join-Path $tempLayout 'WPG.exe') -Force
    Copy-Item "..\$Architecture\Release\Core.dll" (Join-Path $tempLayout 'Core.dll') -Force

    $manifestPath = Join-Path $tempLayout 'AppxManifest.xml'
    [xml]$manifest = Get-Content -LiteralPath $manifestPath
    $manifest.Package.Identity.ProcessorArchitecture = $ArchitectureLower
    $manifest.Package.Identity.Version = $Version
    $manifest.Save($manifestPath)

    New-Item -ItemType Directory -Path 'Packages' -Force | Out-Null

    makeappx pack /d $tempLayout /p ".\Packages\WPG.$Architecture.msix"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
finally {
    Remove-Item $tempLayout -Recurse -Force -ErrorAction SilentlyContinue
}
