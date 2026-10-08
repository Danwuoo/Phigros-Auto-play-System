param([Parameter(Mandatory)][string]$Output)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$Output=[IO.Path]::GetFullPath($Output)
if(-not $Output.StartsWith((Join-Path $repo 'out/windows-handoff/'),[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $Output)){throw 'fresh main-local environment output required'}
$variables=[ordered]@{}
foreach($name in @('INCLUDE','LIB','LIBPATH','PATH','VCToolsInstallDir','VCToolsVersion','WindowsSdkDir','WindowsSDKVersion','UniversalCRTSdkDir','UCRTVersion','VSINSTALLDIR','VCINSTALLDIR','DevEnvDir','VisualStudioVersion')){
 $value=[Environment]::GetEnvironmentVariable($name,'Process')
 if(-not $value -or $value.Length -gt 16384 -or $value -match '["%\r\n]'){throw ('missing or nonliteral compiler environment '+$name)}
 $variables[$name]=$value
}
if($variables.VCToolsVersion -cne '14.51.36231' -or $variables.VisualStudioVersion -cne '18.0'){throw 'pinned compiler environment required'}
$bytes=[Text.Encoding]::UTF8.GetBytes((@{schema='pas.msvc-environment.v1';variables=$variables;variable_count=14;device_operations=0;live_ready=$false}|ConvertTo-Json -Depth 4)+"`n")
if($bytes.Length -gt 131072){throw 'compiler environment byte bound'}
$handle=[IO.FileStream]::new($Output,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
try{$handle.Write($bytes);$handle.Flush($true)}finally{$handle.Dispose()}
