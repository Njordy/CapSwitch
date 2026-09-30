param([switch]$SkipTests)
$ErrorActionPreference = 'Stop'
$ProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $SkipTests) { & (Join-Path $PSScriptRoot 'Test.ps1') }
$VersionHeader = Get-Content -LiteralPath (Join-Path $ProjectRoot 'CapSwitch\version.h') -Raw
if ($VersionHeader -notmatch '#define CAPSWITCH_VERSION_STRING "(\d+\.\d+\.\d+)"') { throw 'Invalid version header.' }
$Version = $Matches[1]
$VsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $VsWhere)) { throw 'Visual Studio Installer (vswhere.exe) was not found.' }
$VsInstall = & $VsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $VsInstall) { throw 'Install Visual Studio 2022 with Desktop development with C++.' }
$MsBuild = Join-Path $VsInstall 'MSBuild\Current\Bin\amd64\MSBuild.exe'
$Artifacts = Join-Path $ProjectRoot 'artifacts'
$BuildDirectory = Join-Path $Artifacts 'build'
$BundleDirectory = Join-Path $Artifacts "CapSwitch-$Version-windows-x64"
$ReleaseDirectory = Join-Path $Artifacts "release-$Version"
New-Item -ItemType Directory -Path $BuildDirectory,$BundleDirectory,$ReleaseDirectory -Force | Out-Null
& $MsBuild (Join-Path $ProjectRoot 'CapSwitch.sln') /p:Configuration=Release /p:Platform=x64 "/p:OutDir=$BuildDirectory\" "/p:IntDir=$Artifacts\obj\" /m /v:minimal /nologo "/flp:logfile=$Artifacts\release-build.log;verbosity=normal"
if ($LASTEXITCODE -ne 0) { throw "Release build failed ($LASTEXITCODE)." }
$BuiltExe = Join-Path $BuildDirectory 'CapSwitch.exe'
if ((Get-Item -LiteralPath $BuiltExe).VersionInfo.FileVersion -ne $Version) { throw 'EXE version does not match version.h.' }
Copy-Item -LiteralPath $BuiltExe -Destination (Join-Path $BundleDirectory 'CapSwitch.exe')
Copy-Item -LiteralPath (Join-Path $ProjectRoot 'docs\PORTABLE-README.txt') -Destination (Join-Path $BundleDirectory 'README.txt')
Copy-Item -LiteralPath (Join-Path $ProjectRoot 'CHANGELOG.md') -Destination (Join-Path $BundleDirectory 'CHANGELOG.md')
$ReleaseExe = Join-Path $ReleaseDirectory "CapSwitch-$Version-windows-x64.exe"
$ReleaseZip = Join-Path $ReleaseDirectory "CapSwitch-$Version-windows-x64.zip"
Copy-Item -LiteralPath $BuiltExe -Destination $ReleaseExe
# Explicit inputs prevent stray files from entering the public release archive.
Compress-Archive -LiteralPath (Join-Path $BundleDirectory 'CapSwitch.exe'),(Join-Path $BundleDirectory 'README.txt'),(Join-Path $BundleDirectory 'CHANGELOG.md') -DestinationPath $ReleaseZip -Force
$ChecksumLines = @($ReleaseExe,$ReleaseZip) | ForEach-Object {
    $Digest = (Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash.ToLowerInvariant()
    "$Digest  $([IO.Path]::GetFileName($_))"
}
Set-Content -LiteralPath (Join-Path $ReleaseDirectory 'SHA256SUMS.txt') -Value $ChecksumLines -Encoding ascii
Copy-Item -LiteralPath (Join-Path $ProjectRoot 'docs\RELEASE-NOTES.md') -Destination (Join-Path $ReleaseDirectory 'RELEASE-NOTES.md')
Write-Output "Release $Version ready: $ReleaseDirectory"
