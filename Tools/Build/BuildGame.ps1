<#
.SYNOPSIS
    Builds, cooks and packages GameJam for Windows into Builds\, then checks the package holds everything the game loads.

.DESCRIPTION
    Why a packaged build used to miss things the editor shows: Unreal packages what the cooked levels reference, but the game
    also loads assets by path from Project Settings (skins, animations, effects, music stems). GJAssetManager now adds those
    to every cook. This script runs the whole pipeline and proves the result:

      1. refuses to run while the editor has the project open (unsaved work would be left out, and it locks files);
      2. finds the engine (-EngineDir, %UE_ROOT%, the .uproject's engine association, or the default install);
      3. cooks every game map (DefaultGame.ini MapsToCook plus any level found outside third-party packs, so a new menu
         map is never forgotten);
      4. runs BuildCookRun: build, cook, stage, pak, archive to Builds\<Name>-<Config>\Windows;
      5. checks every package in Saved\GJCookManifest.txt was cooked, and that the agent snapshots were staged;
      6. on Development builds, starts the packaged game headless and runs gj.VerifyContent, which loads every asset the
         game reaches by path, and scans that log for load failures.

    Exit code 0 only when the build and every check pass.

.EXAMPLE
    .\BuildGame.bat
    Development build plus all checks (the default; good for testing).

.EXAMPLE
    .\BuildGame.bat -Config Shipping -Zip
    Final build: Shipping, no debug files, prerequisites installer included, zipped for sharing.
#>
[CmdletBinding()]
param(
    [ValidateSet('Development', 'Shipping', 'DebugGame')]
    [string]$Config = 'Development',
    # Folder name under Builds\ (the archive goes to Builds\<Name>-<Config>).
    [string]$Name = 'GameJam',
    # Unreal Engine root (the folder holding Engine\). Default: %UE_ROOT%, then the .uproject's engine association.
    [string]$EngineDir = '',
    # Rebuild and recook everything from scratch.
    [switch]$Clean,
    # Zip the packaged game to Builds\<Name>-<Config>.zip.
    [switch]$Zip,
    # Skip launching the packaged game (Development / DebugGame only).
    [switch]$SkipSmokeTest,
    # Build even though an editor has the project open (unsaved changes will not be in the build).
    [switch]$AllowEditorOpen
)

$ErrorActionPreference = 'Stop'
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$ProjectDir = Join-Path $RepoRoot 'GameJam'
$Project = Join-Path $ProjectDir 'GameJam.uproject'
$OutDir = Join-Path $RepoRoot "Builds\$Name-$Config"
$LogDir = Join-Path $RepoRoot 'Builds\Logs'
$Stamp = Get-Date -Format 'yyyyMMdd-HHmmss'

function Step([string]$Text) { Write-Host "`n== $Text" -ForegroundColor Cyan }
function Fail([string]$Text) { Write-Host "`nBUILD FAILED: $Text" -ForegroundColor Red; exit 1 }
function Ok([string]$Text) { Write-Host "   OK  $Text" -ForegroundColor Green }

if (-not (Test-Path $Project)) { Fail "no project at $Project" }
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

# --- 1. The editor must not have this project open --------------------------------------------------------------------
Step 'Checking that the editor is closed'
$Editors = @(Get-CimInstance Win32_Process -Filter "Name like 'UnrealEditor%'" -ErrorAction SilentlyContinue |
    Where-Object { $_.CommandLine -and $_.CommandLine -match [regex]::Escape('GameJam.uproject') })
if ($Editors.Count -gt 0) {
    if ($AllowEditorOpen) {
        Write-Host '   The editor has the project open: unsaved changes will NOT be in this build (-AllowEditorOpen).' -ForegroundColor Yellow
    } else {
        Fail 'the Unreal Editor has GameJam open. Save all (Ctrl+Shift+S), close it, and run again (or pass -AllowEditorOpen).'
    }
} else { Ok 'no editor has the project open' }

# --- 2. Engine and its bundled .NET ------------------------------------------------------------------------------------
Step 'Finding the engine'
if (-not $EngineDir) { $EngineDir = $env:UE_ROOT }
if (-not $EngineDir) {
    $Association = (Get-Content $Project -Raw | ConvertFrom-Json).EngineAssociation
    if ($Association -match '^\{.*\}$') {
        # A registered build (GUID): HKCU\Software\Epic Games\Unreal Engine\Builds
        $Builds = Get-ItemProperty 'HKCU:\Software\Epic Games\Unreal Engine\Builds' -ErrorAction SilentlyContinue
        if ($Builds -and $Builds.$Association) { $EngineDir = $Builds.$Association }
    } elseif ($Association) {
        # A launcher install ("5.7"): HKLM\SOFTWARE\EpicGames\Unreal Engine\<version>
        $Key = Get-ItemProperty "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$Association" -ErrorAction SilentlyContinue
        if ($Key) { $EngineDir = $Key.InstalledDirectory }
    }
}
if (-not $EngineDir) { $EngineDir = 'C:\Program Files\Epic Games\UE_5.7' }
$RunUAT = Join-Path $EngineDir 'Engine\Build\BatchFiles\RunUAT.bat'
if (-not (Test-Path $RunUAT)) { Fail "RunUAT.bat not found under '$EngineDir'. Pass -EngineDir or set UE_ROOT." }
Ok "engine: $EngineDir"

# The engine's bundled .NET: build tools fail with "You must install .NET" on machines without a system .NET 8.
$DotNet = Get-ChildItem (Join-Path $EngineDir 'Engine\Binaries\ThirdParty\DotNet') -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending | ForEach-Object { Join-Path $_.FullName 'win-x64' } |
    Where-Object { Test-Path (Join-Path $_ 'dotnet.exe') } | Select-Object -First 1
if ($DotNet) {
    $env:DOTNET_ROOT = $DotNet
    $env:PATH = "$DotNet;$env:PATH"
    $env:DOTNET_MULTILEVEL_LOOKUP = '0'
    $env:DOTNET_ROLL_FORWARD = 'LatestMajor'
    Ok "bundled .NET: $DotNet"
}

# --- 3. Maps to cook ---------------------------------------------------------------------------------------------------
Step 'Collecting the maps to cook'
$Maps = New-Object System.Collections.Generic.List[string]
$GameIni = Join-Path $ProjectDir 'Config\DefaultGame.ini'
foreach ($Line in (Get-Content $GameIni)) {
    if ($Line -match '^\+?MapsToCook=\(FilePath="([^"]+)"\)') { if (-not $Maps.Contains($Matches[1])) { $Maps.Add($Matches[1]) } }
}
# Plus every level outside third-party packs and template samples: a new map (a menu, a new club floor) is never left out.
$NotGame = @('AD_Basic_Dances', 'FirstPersonRifleAnimations', 'Sparks_Embers', 'M16A1', 'NW_MuzzleFX', 'Vefects', 'Fab',
    'Variant_Horror', 'Variant_Shooter', 'Developers', 'Collections', 'StarterContent', 'LevelPrototyping')
$Content = Join-Path $ProjectDir 'Content'
Get-ChildItem $Content -Recurse -Filter '*.umap' | ForEach-Object {
    $Relative = $_.FullName.Substring($Content.Length + 1) -replace '\\', '/'
    $Top = $Relative.Split('/')[0]
    if ($NotGame -notcontains $Top -and $Relative -notmatch '__External') {
        $Path = '/Game/' + ($Relative -replace '\.umap$', '')
        if (-not $Maps.Contains($Path)) { $Maps.Add($Path) }
    }
}
if ($Maps.Count -eq 0) { Fail 'no maps to cook' }
$Maps | ForEach-Object { Write-Host "   $_" }

# --- 4. Build, cook, stage, package ------------------------------------------------------------------------------------
# The cook runs the editor with the project's editor modules: rebuild them first, or C++ pulled since the editor last
# compiled (a teammate's new classes) is missing from the cook.
Step 'Compiling the editor modules (GameJamEditor)'
$BuildBat = Join-Path $EngineDir 'Engine\Build\BatchFiles\Build.bat'
$PreviousPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
& $BuildBat GameJamEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReloadFromIDE 2>&1 | ForEach-Object { "   $_" }
$EditorExit = $LASTEXITCODE
$ErrorActionPreference = $PreviousPreference
if ($EditorExit -ne 0) { Fail "the editor modules did not compile (exit $EditorExit)" }
Ok 'editor modules up to date'

Step "BuildCookRun ($Config) -> $OutDir"
$Manifest = Join-Path $ProjectDir 'Saved\GJCookManifest.txt'
Remove-Item $Manifest -ErrorAction SilentlyContinue
$UatLog = Join-Path $LogDir "BuildCookRun-$Name-$Config-$Stamp.log"
$UatArgs = @(
    'BuildCookRun', "-project=$Project", '-noP4', '-utf8output', '-unattended',
    '-platform=Win64', "-clientconfig=$Config", '-target=GameJam',
    '-build', '-cook', '-stage', '-pak', '-archive', "-archivedirectory=$OutDir",
    "-map=$($Maps -join '+')", '-prereqs'
)
if ($Config -eq 'Shipping') { $UatArgs += '-nodebuginfo' }
if ($Clean) { $UatArgs += '-clean' }
Write-Host "   log: $UatLog"
# Windows PowerShell 5.1 turns a native tool's stderr into error records; keep going and judge by the exit code instead.
$PreviousPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$Writer = New-Object System.IO.StreamWriter($UatLog, $false, (New-Object System.Text.UTF8Encoding($false)))   # UTF-8, searchable
try {
    & $RunUAT @UatArgs 2>&1 | ForEach-Object { $Line = "$_"; $Writer.WriteLine($Line); $Line }
    $UatExit = $LASTEXITCODE
} finally {
    $Writer.Close()
    $ErrorActionPreference = $PreviousPreference
}
if ($UatExit -ne 0) { Fail "BuildCookRun exited with $UatExit (see $UatLog)" }
Ok 'built, cooked and packaged'

# --- 5. Everything the game loads by path was cooked; the agent snapshots were staged ---------------------------------
Step 'Checking the cooked content'
if (-not (Test-Path $Manifest)) {
    Fail "the cook did not write Saved\GJCookManifest.txt: is AssetManagerClassName=/Script/GameJam.GJAssetManager set in DefaultEngine.ini?"
}
$Cooked = Join-Path $ProjectDir 'Saved\Cooked\Windows\GameJam\Content'
$Missing = @()
$Packages = @(Get-Content $Manifest | Where-Object { $_ -match '^/Game/' })
foreach ($Package in $Packages) {
    $Relative = $Package.Substring('/Game/'.Length) -replace '/', '\'
    if (-not ((Test-Path "$Cooked\$Relative.uasset") -or (Test-Path "$Cooked\$Relative.umap"))) { $Missing += $Package }
}
if ($Missing.Count -gt 0) {
    $Missing | ForEach-Object { Write-Host "   MISSING $_" -ForegroundColor Red }
    Fail "$($Missing.Count) of $($Packages.Count) packages the game loads by path were not cooked (does the asset exist in this checkout?)"
}
Ok "all $($Packages.Count) packages the game loads by path are cooked"

$GameDir = Join-Path $OutDir 'Windows\GameJam'
$Snapshots = @(Get-ChildItem (Join-Path $GameDir 'Content\AgentModels') -Recurse -Filter '*.bin' -ErrorAction SilentlyContinue)
if ($Snapshots.Count -eq 0) { Fail 'no agent snapshots (.bin) staged under Content\AgentModels: the enemies would have no brain' }
Ok "$($Snapshots.Count) agent snapshot files staged"

# --- 6. Smoke test: run the packaged game and load everything ------------------------------------------------------------
if ($Config -ne 'Shipping' -and -not $SkipSmokeTest) {
    Step 'Smoke test: packaged game, gj.VerifyContent'
    $Exe = Get-ChildItem (Join-Path $GameDir 'Binaries\Win64') -Filter 'GameJam*.exe' | Select-Object -First 1
    if (-not $Exe) { Fail "no game executable under $GameDir\Binaries\Win64" }
    $GameLog = Join-Path $GameDir 'Saved\Logs\VerifyContent.log'
    Remove-Item $GameLog -ErrorAction SilentlyContinue
    $Game = Start-Process -FilePath $Exe.FullName -PassThru -WindowStyle Minimized -ArgumentList @(
        '-nullrhi', '-nosound', '-unattended', '-nosplash', '-log=VerifyContent.log', '-ExecCmds="gj.VerifyContent quit"')
    if (-not $Game.WaitForExit(600000)) { $Game.Kill(); Fail 'the packaged game did not finish gj.VerifyContent within 10 minutes' }
    if (-not (Test-Path $GameLog)) { Fail "the packaged game wrote no log ($GameLog)" }
    $Problems = @(Select-String -Path $GameLog -Pattern 'VerifyContent: MISSING', 'Failed to find object', 'LoadPackage: SkipPackage', 'CDO Constructor.*Failed')
    $Problems | ForEach-Object { Write-Host "   $($_.Line.Trim())" -ForegroundColor Red }
    if (-not (Select-String -Path $GameLog -Pattern 'VerifyContent: PASS' -Quiet)) { Fail "gj.VerifyContent did not pass (see $GameLog)" }
    if ($Problems.Count -gt 0) { Fail "$($Problems.Count) load problems in the packaged game (see $GameLog)" }
    Ok ((Select-String -Path $GameLog -Pattern 'VerifyContent: PASS').Line -replace '^.*VerifyContent: ', 'VerifyContent: ')
} elseif ($Config -eq 'Shipping') {
    Write-Host "`n   Shipping builds have no console or log, so the in-game check runs on Development builds (same cook)." -ForegroundColor Yellow
}

# --- Done ----------------------------------------------------------------------------------------------------------------
$Windows = Join-Path $OutDir 'Windows'
$SizeMB = [math]::Round(((Get-ChildItem $Windows -Recurse -File | Measure-Object Length -Sum).Sum / 1MB), 0)
if ($Zip) {
    Step 'Zipping'
    $ZipPath = "$OutDir.zip"
    Remove-Item $ZipPath -ErrorAction SilentlyContinue
    Compress-Archive -Path (Join-Path $Windows '*') -DestinationPath $ZipPath
    Ok "zip: $ZipPath"
}
Write-Host "`nBUILD OK ($Config, $SizeMB MB): $Windows" -ForegroundColor Green
Write-Host "   Players run: $(Join-Path $Windows 'GameJam.exe')"
exit 0
