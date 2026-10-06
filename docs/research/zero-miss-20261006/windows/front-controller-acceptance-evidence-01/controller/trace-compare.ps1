$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$pkg=Join-Path $repo 'out/windows-handoff/hold-front-package-01'
function Decision($text){
 $row=ConvertFrom-Json -InputObject $text -AsHashtable
 foreach($key in @('host_decode_start_ns','host_pixels_ready_ns','host_recognition_complete_ns','host_bridge_complete_ns','host_complete_ns')){[void]$row.Remove($key)}
 foreach($key in @('host_start_ns','host_end_ns')){[void]$row.current_front_sidecar.Remove($key)}
 $row|ConvertTo-Json -Depth 50 -Compress
}
$reference=Join-Path $pkg 'pixels-release-controller-02.json.rows.jsonl'
$rows=[IO.File]::ReadAllLines($reference);if($rows.Count -ne 256){throw 'fresh-trace-denominator'}
$canonical=@($rows|ForEach-Object {Decision $_})
$checked=@()
foreach($name in @('pixels-release-03','pixels-debug-02','pixels-asan-02','pixels-release-controller-01')){
 $path=Join-Path $pkg ($name+'.json.rows.jsonl');$before=[IO.File]::ReadAllLines($path)
 if($before.Count -ne 256){throw 'other-trace-denominator'}
 for($i=0;$i -lt 256;$i++){if((Decision $before[$i]) -cne $canonical[$i]){throw ('decision-or-geometry-diff '+$name+' row '+$i)}}
 $checked+=@{trace=$path;rows=256;decision_and_all_sidecar_geometry_equal=$true;sha256=(Get-FileHash $path).Hash.ToLowerInvariant();process_stop_preserved=($name -eq 'pixels-release-controller-01')}
}
$value=@{schema='pas.front-controller-cross-configuration-trace-audit.v1';passed=$true;fresh_reference=$reference;fresh_reference_sha256=(Get-FileHash $reference).Hash.ToLowerInvariant();compared=$checked;excluded='five original host timestamps and two new sidecar host timestamps only';physical_gold=0}
$s=[IO.FileStream]::new((Join-Path $PSScriptRoot 'trace-comparison.json'),[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
try{$bytes=[Text.Encoding]::UTF8.GetBytes(($value|ConvertTo-Json -Depth 10)+"`n");$s.Write($bytes);$s.Flush($true)}finally{$s.Dispose()}
'all 256 decision + sidecar rows equal across three delivered configurations and both reviewer runs'
