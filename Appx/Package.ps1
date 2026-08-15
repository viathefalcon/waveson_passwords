param(
    [Parameter(Mandatory = $false)]
    [string[]]$Architectures = @('arm64', 'x64')
)

New-Item -ItemType Directory -Path 'Packages' -Force | Out-Null
Remove-Item -Path 'Packages\*' -Recurse -Force -ErrorAction SilentlyContinue

$bundleVersion = $null

foreach ($architecture in $Architectures) {
    $ArchitectureLower = $architecture.ToLowerInvariant()

    msbuild '..\waveson_passwords.sln' /t:Build /p:Configuration=Release /p:Platform=$architecture
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $tempLayout = Join-Path ([System.IO.Path]::GetTempPath()) ([System.Guid]::NewGuid().ToString())
    Copy-Item 'Layout' $tempLayout -Recurse -Force

    try {
        # Get the version from the executable
        $exePath = Resolve-Path "..\$architecture\Release\WPG.exe"
        $Version = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($exePath).ProductVersion
        if ([string]::IsNullOrWhiteSpace($Version)) {
            throw "WPG.exe does not contain a ProductVersion resource."
        }

        $bundleVersion = $Version

        Copy-Item $exePath (Join-Path $tempLayout 'WPG.exe') -Force
        Copy-Item "..\$architecture\Release\Core.dll" (Join-Path $tempLayout 'Core.dll') -Force

        $manifestPath = Join-Path $tempLayout 'AppxManifest.xml'
        [xml]$manifest = Get-Content -LiteralPath $manifestPath
        $manifest.Package.Identity.ProcessorArchitecture = $ArchitectureLower
        $manifest.Package.Identity.Version = $Version
        $manifest.Save($manifestPath)

        makeappx pack /d $tempLayout /p ".\Packages\WPG.$architecture.msix"
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    finally {
        Remove-Item $tempLayout -Recurse -Force -ErrorAction SilentlyContinue
    }
}

makeappx bundle /d '.\Packages' /bv $bundleVersion /p '.\WPG.msixbundle' /o
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
