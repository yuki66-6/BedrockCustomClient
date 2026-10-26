<#
.SYNOPSIS
  Configure, compile, teste et empaquette BedrockCustomClient (MSVC x64 + CMake).

.EXAMPLE
  .\build.ps1                 # Release + tests
  .\build.ps1 -Config Debug
  .\build.ps1 -Package        # crée dist\BedrockCustomClient-<version>-win64.zip
#>
param(
    [ValidateSet('Release', 'Debug', 'RelWithDebInfo')]
    [string]$Config = 'Release',
    [string]$BuildDir = '',
    [switch]$NoTests,
    [switch]$Package,
    [switch]$Clean
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot

# Chemins longs : MSVC/NMake échouent au-delà de 260 caractères. Si le projet est
# profondément imbriqué, on compile dans un dossier court sous %TEMP%.
if (-not $BuildDir) {
    if ($root.Length -gt 120) { $BuildDir = Join-Path $env:TEMP 'bcc-build' }
    else { $BuildDir = Join-Path $root 'build' }
}
$BuildDir = Join-Path $BuildDir $Config

function Find-CMake {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    # CMake portable (zip officiel Kitware) dans un dossier court : voir README.
    foreach ($dir in (Join-Path $env:LOCALAPPDATA 'BedrockCustomClient-tools'), (Join-Path $root '.tools')) {
        $local = Get-ChildItem $dir -Recurse -Filter cmake.exe -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($local) { return $local.FullName }
    }
    $vs = Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\2022\*\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($vs) { return $vs.FullName }
    throw "CMake introuvable. Installez CMake >= 3.24 (voir README)."
}

function Find-VcVars {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $path = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($path) { $bat = Join-Path $path 'VC\Auxiliary\Build\vcvars64.bat'; if (Test-Path $bat) { return $bat } }
    }
    foreach ($ed in 'Community', 'Professional', 'Enterprise', 'BuildTools') {
        $bat = "C:\Program Files\Microsoft Visual Studio\2022\$ed\VC\Auxiliary\Build\vcvars64.bat"
        if (Test-Path $bat) { return $bat }
    }
    throw "Visual Studio 2022 (outils C++ x64) introuvable."
}

$cmake = Find-CMake
$vcvars = Find-VcVars
Write-Host "CMake  : $cmake"
Write-Host "vcvars : $vcvars"
Write-Host "Build  : $BuildDir"

if ($Clean -and (Test-Path $BuildDir)) { Remove-Item -Recurse -Force $BuildDir }
New-Item -ItemType Directory -Force $BuildDir | Out-Null

# Exécute une commande dans l'environnement MSVC x64 (vcvars64) sans le persister.
# Passe par un fichier .cmd temporaire : évite les pièges de guillemets de PowerShell 5.1.
function Invoke-InVcEnv([string]$command) {
    $tmp = Join-Path $env:TEMP ("bcc-" + [guid]::NewGuid().ToString('N') + '.cmd')
    $body = "@echo off`r`nchcp 65001 >nul`r`ncall `"$vcvars`" >nul 2>&1 || exit /b 1`r`n$command 2>&1`r`nexit /b %ERRORLEVEL%`r`n"
    [IO.File]::WriteAllText($tmp, $body, (New-Object Text.UTF8Encoding $false))
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue' # la sortie d'erreur des outils natifs n'est pas fatale
    try {
        & cmd.exe /d /c $tmp
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prev
        Remove-Item $tmp -ErrorAction SilentlyContinue
    }
    if ($code -ne 0) { throw "Échec ($code) : $command" }
}

$distDir = Join-Path $root 'dist'
Invoke-InVcEnv "`"$cmake`" -S `"$root`" -B `"$BuildDir`" -G `"NMake Makefiles`" -DCMAKE_BUILD_TYPE=$Config -DCMAKE_INSTALL_PREFIX=`"$distDir\BedrockCustomClient`""
Invoke-InVcEnv "`"$cmake`" --build `"$BuildDir`""

if (-not $NoTests) {
    Invoke-InVcEnv "cd /d `"$BuildDir`" && `"$(Split-Path $cmake)\ctest.exe`" --output-on-failure"
}

if ($Package) {
    if (Test-Path "$distDir\BedrockCustomClient") { Remove-Item -Recurse -Force "$distDir\BedrockCustomClient" }
    Invoke-InVcEnv "`"$cmake`" --install `"$BuildDir`""
    $version = (Select-String -Path (Join-Path $root 'CMakeLists.txt') -Pattern 'VERSION (\d+\.\d+\.\d+)').Matches[0].Groups[1].Value
    $zip = Join-Path $distDir "BedrockCustomClient-$version-win64.zip"
    if (Test-Path $zip) { Remove-Item $zip }
    # Zip construit entrée par entrée : sous PowerShell 5.1, Compress-Archive et
    # ZipFile.CreateFromDirectory écrivent des « \ » dans les chemins (non standard).
    Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
    $src = (Resolve-Path "$distDir\BedrockCustomClient").Path
    $archive = [IO.Compression.ZipFile]::Open($zip, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($file in Get-ChildItem $src -Recurse -File) {
            $entry = $file.FullName.Substring($src.Length + 1).Replace('\', '/')
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $file.FullName, $entry,
                [IO.Compression.CompressionLevel]::Optimal) | Out-Null
        }
    } finally {
        $archive.Dispose()
    }
    Write-Host "Paquet : $zip"
}
Write-Host "Exécutable : $BuildDir\bin\BedrockCustomClient.exe"
