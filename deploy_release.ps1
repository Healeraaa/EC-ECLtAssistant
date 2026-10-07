[CmdletBinding()]
param(
    [string]$QtBin = 'D:\Qt\Qt5.14.2\5.14.2\msvc2017_64\bin',
    [string]$MsBuild = 'D:\VS2022\VS2022\MSBuild\Current\Bin\MSBuild.exe',
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = [IO.Path]::GetFullPath($PSScriptRoot)
$solutionPath = Join-Path $repositoryRoot 'SerialPortAssistant.sln'
$versionHeader = Join-Path $repositoryRoot 'SerialPortAssistant\AppVersion.h'
$versionText = Get-Content -LiteralPath $versionHeader -Raw
if ($versionText -notmatch '#define\s+EC_ECL_APP_VERSION\s+"([^"]+)"') {
    throw '无法从 AppVersion.h 读取版本号。'
}
$version = $Matches[1]
$packageName = "EC-ECL-Recorder-v$version-win64"
$distRoot = [IO.Path]::GetFullPath((Join-Path $repositoryRoot 'dist'))
$packageDirectory = [IO.Path]::GetFullPath((Join-Path $distRoot $packageName))
$zipPath = [IO.Path]::GetFullPath((Join-Path $distRoot "$packageName.zip"))

if ((-not $packageDirectory.StartsWith($distRoot, [StringComparison]::OrdinalIgnoreCase)) `
    -or (-not $zipPath.StartsWith($distRoot, [StringComparison]::OrdinalIgnoreCase))) {
    throw '发布输出路径校验失败。'
}

$deployTool = Join-Path $QtBin 'windeployqt.exe'
if (!(Test-Path -LiteralPath $MsBuild)) { throw "找不到 MSBuild：$MsBuild" }
if (!(Test-Path -LiteralPath $deployTool)) { throw "找不到 windeployqt：$deployTool" }

if (!$SkipBuild) {
    & $MsBuild $solutionPath /t:Build /p:Configuration=Release /p:Platform=x64 /v:minimal
    if ($LASTEXITCODE -ne 0) { throw "Release 编译失败，退出码：$LASTEXITCODE" }
}

$sourceExecutable = Join-Path $repositoryRoot 'x64\Release\SerialPortAssistant.exe'
if (!(Test-Path -LiteralPath $sourceExecutable)) {
    throw "找不到 Release 程序：$sourceExecutable"
}

New-Item -ItemType Directory -Path $distRoot -Force | Out-Null
if (Test-Path -LiteralPath $packageDirectory) {
    Remove-Item -LiteralPath $packageDirectory -Recurse -Force
}
New-Item -ItemType Directory -Path $packageDirectory | Out-Null

$targetExecutable = Join-Path $packageDirectory 'EC-ECL Recorder.exe'
Copy-Item -LiteralPath $sourceExecutable -Destination $targetExecutable
& $deployTool --release --compiler-runtime --no-translations $targetExecutable
if ($LASTEXITCODE -ne 0) { throw "windeployqt 失败，退出码：$LASTEXITCODE" }

# windeployqt 在未加载 VS 开发环境时无法定位 VC 运行库，因此再做一次显式收集。
$msBuildDirectory = Split-Path -Parent $MsBuild
$visualStudioRoot = [IO.Path]::GetFullPath((Join-Path $msBuildDirectory '..\..\..'))
$redistRoot = Join-Path $visualStudioRoot 'VC\Redist\MSVC'
$redistVersion = Get-ChildItem -LiteralPath $redistRoot -Directory |
    Where-Object { $_.Name -match '^\d+\.\d+\.\d+$' } |
    Sort-Object { [Version]$_.Name } -Descending |
    Select-Object -First 1
if (!$redistVersion) { throw "找不到 Visual C++ 运行库目录：$redistRoot" }
$crtDirectory = Join-Path $redistVersion.FullName 'x64\Microsoft.VC143.CRT'
if (!(Test-Path -LiteralPath $crtDirectory)) {
    throw "找不到 x64 Visual C++ 运行库：$crtDirectory"
}
Get-ChildItem -LiteralPath $crtDirectory -Filter '*.dll' -File |
    Copy-Item -Destination $packageDirectory

$requiredFiles = @(
    'EC-ECL Recorder.exe',
    'Qt5Core.dll',
    'Qt5Gui.dll',
    'Qt5Widgets.dll',
    'Qt5Charts.dll',
    'Qt5SerialPort.dll',
    'msvcp140.dll',
    'vcruntime140.dll',
    'vcruntime140_1.dll',
    'platforms\qwindows.dll'
)
foreach ($requiredFile in $requiredFiles) {
    if (!(Test-Path -LiteralPath (Join-Path $packageDirectory $requiredFile))) {
        throw "发布包缺少依赖：$requiredFile"
    }
}

Copy-Item -LiteralPath (Join-Path $repositoryRoot 'README.md') -Destination $packageDirectory
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'RELEASE_NOTES.md') -Destination $packageDirectory
Set-Content -LiteralPath (Join-Path $packageDirectory 'VERSION.txt') -Encoding UTF8 -Value @(
    "EC-ECL Recorder v$version"
    "Build: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
    'Platform: Windows x64'
)

$manifest = Get-ChildItem -LiteralPath $packageDirectory -Recurse -File | ForEach-Object {
    [PSCustomObject]@{
        Path = $_.FullName.Substring($packageDirectory.Length + 1)
        Size = $_.Length
        SHA256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    }
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $packageDirectory 'manifest.json') -Encoding UTF8

if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
Compress-Archive -LiteralPath $packageDirectory -DestinationPath $zipPath -CompressionLevel Optimal

Write-Output "Portable directory: $packageDirectory"
Write-Output "ZIP package: $zipPath"
