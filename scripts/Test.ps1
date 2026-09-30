param([switch]$Calibrate)
$ErrorActionPreference = 'Stop'
$ProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$VsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $VsWhere)) { throw 'Visual Studio Installer (vswhere.exe) was not found.' }
$VsInstall = & $VsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $VsInstall) { throw 'Install Visual Studio 2022 with Desktop development with C++.' }
$MsBuild = Join-Path $VsInstall 'MSBuild\Current\Bin\amd64\MSBuild.exe'
$Artifacts = Join-Path $ProjectRoot 'artifacts'
New-Item -ItemType Directory -Path $Artifacts -Force | Out-Null
$TestProject = Join-Path $ProjectRoot 'tests\WinApiTests.vcxproj'
& $MsBuild $TestProject /p:Configuration=Release /p:Platform=x64 /m /v:minimal /nologo
if ($LASTEXITCODE -ne 0) { throw "Test build failed ($LASTEXITCODE)." }
& (Join-Path $Artifacts 'tests\CapSwitchTests.exe') | Tee-Object -FilePath (Join-Path $Artifacts 'tests.txt')
if ($LASTEXITCODE -ne 0) { throw "Regression tests failed ($LASTEXITCODE)." }

if ($Calibrate) {
    # Reintroduce the observed defects in a disposable source copy, never in production.
    $Source = Get-Content -LiteralPath (Join-Path $ProjectRoot 'CapSwitch\main.cpp') -Raw
    $Anchor = '    // A suppressed key has no usable async state.'
    if (-not $Source.Contains($Anchor)) { throw 'Watchdog mutation anchor changed.' }
    $Source = $Source.Replace($Anchor, "    if (g_capsDown.load() && (GetAsyncKeyState(VK_CAPITAL) & 0x8000) == 0) { ResetInputState(); }`n$Anchor")
    $Anchor = '        DestroyMenu(submenu);'
    if (-not $Source.Contains($Anchor)) { throw 'Submenu mutation anchor changed.' }
    $Source = $Source.Replace($Anchor, '        /* Deliberately leak the unattached submenu for calibration. */')
    Set-Content -LiteralPath (Join-Path $Artifacts 'mutant.cpp') -Value $Source -Encoding utf8
    & $MsBuild $TestProject /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:TestDefines=CAPSWITCH_MUTANT "/p:OutDir=$Artifacts\mutant\" "/p:IntDir=$Artifacts\mutant-obj\" /m /v:minimal /nologo
    if ($LASTEXITCODE -ne 0) { throw "Calibration build failed ($LASTEXITCODE)." }
    $CalibrationOutput = & (Join-Path $Artifacts 'mutant\CapSwitchTests.exe')
    $CalibrationExit = $LASTEXITCODE
    $CalibrationOutput | Tee-Object -FilePath (Join-Path $Artifacts 'calibration.txt')
    if ($CalibrationExit -ne 1 -or
        -not ($CalibrationOutput -match '^FAIL: 30-second hold') -or
        -not ($CalibrationOutput -match '^FAIL: 1000 failed submenu')) {
        throw 'Calibration did not detect both deliberately reintroduced defects.'
    }
    Write-Output 'Calibration passed: tests detected both deliberately reintroduced defects.'
}
