#Requires -Version 7.2
[CmdletBinding()]
param(
    [ValidateSet('Portable','Importer','Native')][string]$Mode = 'Portable',
    [string]$BuildDirectory,
    [string]$DependencyPrefix,
    [string]$VcpkgToolchain,
    [string]$Generator,
    [string]$CMakePath = 'cmake',
    [string]$CTestPath = 'ctest',
    [ValidateRange(1,16)][int]$Jobs = 4,
    [ValidateSet('Release','Debug','RelWithDebInfo')][string]$Configuration = 'Release',
    [switch]$PrintOnly
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $BuildDirectory) { $BuildDirectory = Join-Path $repository ('build-local-' + $Mode.ToLowerInvariant()) }
$BuildDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BuildDirectory)
if ($Mode -eq 'Native' -and -not $IsWindows) { throw 'Native mode requires Windows and the existing patched gRPC dependencies.' }
if ($Mode -ne 'Portable' -and -not $DependencyPrefix) {
    throw 'Specify -DependencyPrefix: nlohmann_json install for Importer, or the existing patched x64-windows install for Native.'
}
if ($DependencyPrefix) { $DependencyPrefix = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($DependencyPrefix) }
if ($VcpkgToolchain) { $VcpkgToolchain = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($VcpkgToolchain) }
if ($Mode -eq 'Native' -and -not $PrintOnly) {
    if (-not (Test-Path -LiteralPath (Join-Path $DependencyPrefix 'include/grpc/pas_windows_read_config.h'))) {
        throw 'The prefix does not contain the required PAS patched gRPC header. Use the existing patched x64-windows prefix; do not substitute stock gRPC.'
    }
}
$configure = @('-S',$repository,'-B',$BuildDirectory,"-DCMAKE_BUILD_TYPE=$Configuration",'-DBUILD_TESTING=ON')
if ($Generator) { $configure += @('-G',$Generator) }
$resolvedGenerator = $Generator
$cache = Join-Path $BuildDirectory 'CMakeCache.txt'
if (-not $resolvedGenerator -and (Test-Path -LiteralPath $cache -PathType Leaf)) {
    $match = [regex]::Match([IO.File]::ReadAllText($cache),'(?m)^CMAKE_GENERATOR:INTERNAL=([^\r\n]+)')
    if ($match.Success) { $resolvedGenerator = $match.Groups[1].Value }
}
if (-not $resolvedGenerator) { $resolvedGenerator = $env:CMAKE_GENERATOR }
if ($IsWindows -and $resolvedGenerator -and $resolvedGenerator.StartsWith('Visual Studio')) { $configure += @('-A','x64') }
if ($DependencyPrefix) { $configure += "-DCMAKE_PREFIX_PATH=$DependencyPrefix" }
if ($VcpkgToolchain) {
    $configure += @("-DCMAKE_TOOLCHAIN_FILE=$VcpkgToolchain",'-DVCPKG_MANIFEST_INSTALL=OFF')
    if ($DependencyPrefix) { $configure += "-DVCPKG_INSTALLED_DIR=$([IO.Path]::GetDirectoryName($DependencyPrefix))" }
}
$options = @{
    RHYTHM_BUILD_CHART_IMPORT = ($Mode -ne 'Portable')
    RHYTHM_BUILD_VISUAL_CANDIDATES = ($Mode -ne 'Portable')
    RHYTHM_BUILD_SUPERVISOR = ($Mode -ne 'Portable')
    RHYTHM_BUILD_CALIBRATION_TRIAL = ($Mode -eq 'Native')
    RHYTHM_BUILD_PAS_CORE = ($Mode -eq 'Native')
    RHYTHM_BUILD_PAS_GRPC = ($Mode -eq 'Native')
    RHYTHM_BUILD_PAS_TESTS = ($Mode -eq 'Native')
    RHYTHM_BUILD_PAS_BRIDGE = ($Mode -eq 'Native')
    RHYTHM_BUILD_SESSION_RUNNER = ($Mode -eq 'Native')
    RHYTHM_BUILD_AUDIO_SYNC = $false
    RHYTHM_BUILD_AUDIO_CAPTURE = $false
}
foreach ($name in ($options.Keys | Sort-Object)) {
    $value = if ($options[$name]) { 'ON' } else { 'OFF' }
    $configure += "-D${name}=$value"
}
$commands = @(
    @{ Program=$CMakePath; Arguments=$configure },
    @{ Program=$CMakePath; Arguments=@('--build',$BuildDirectory,'--config',$Configuration,'--parallel',[string]$Jobs) },
    @{ Program=$CTestPath; Arguments=@('--test-dir',$BuildDirectory,'-C',$Configuration,'--output-on-failure') }
)
if ($PrintOnly) {
    [ordered]@{ mode=$Mode; build_directory=$BuildDirectory; device_access=$false; commands=$commands } | ConvertTo-Json -Depth 8
    exit 0
}
foreach ($command in $commands) {
    if (-not (Get-Command $command.Program -ErrorAction SilentlyContinue)) { throw "Tool not found: $($command.Program)" }
}
foreach ($command in $commands) {
    Write-Host ('>>> ' + $command.Program + ' ' + ($command.Arguments -join ' '))
    & $command.Program @($command.Arguments)
    if ($LASTEXITCODE -ne 0) { throw "Build or offline tests failed ($LASTEXITCODE). See output above." }
}
Write-Host "Completed $Mode build and offline tests: $BuildDirectory"
if ($Mode -eq 'Native') {
    Write-Host 'Create a new runtime archive and launch configuration before connecting this binary to an existing calibrated panel.'
}
