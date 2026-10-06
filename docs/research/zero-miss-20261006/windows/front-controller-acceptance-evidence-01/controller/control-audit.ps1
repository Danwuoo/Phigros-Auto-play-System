$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$pkg=Join-Path $repo 'out/windows-handoff/hold-front-package-01'
$reports=@()
foreach($kind in @('epoch','geometry','front-rgb')){
 $mutation=Get-Content (Join-Path $pkg ('negative-'+$kind+'-mutation.json')) -Raw|ConvertFrom-Json
 if((Get-FileHash $mutation.source).Hash.ToLowerInvariant() -cne $mutation.source_sha256 -or
    (Get-FileHash $mutation.control).Hash.ToLowerInvariant() -cne $mutation.control_sha256){throw 'control-SHA'}
 $reference=[IO.File]::ReadAllLines($mutation.source);$changed=[IO.File]::ReadAllLines($mutation.control)
 if($reference.Count -ne 256 -or $changed.Count -ne 256){throw 'control-denominator'}
 $differences=0
 for($i=0;$i -lt 256;$i++){
  if($reference[$i] -ceq $changed[$i]){continue}
  $before=ConvertFrom-Json -InputObject $reference[$i] -AsHashtable
  $after=ConvertFrom-Json -InputObject $changed[$i] -AsHashtable
  if($before.ordinal -ne 3030 -or $after.ordinal -ne 3030){throw 'control-changed-wrong-frame'}
  $a=@($before.current_front_sidecar.results|Where-Object candidate_id -eq 2)
  $b=@($after.current_front_sidecar.results|Where-Object candidate_id -eq 2)
  if($a.Count -ne 1 -or $b.Count -ne 1){throw 'control-witness'}
  if($kind -eq 'front-rgb'){
   if($a[0].edge_samples[0].inside.rgb[0] -ne 155 -or $b[0].edge_samples[0].inside.rgb[0] -ne 156){throw 'control-RGB-values'}
   $b[0].edge_samples[0].inside.rgb[0]=$a[0].edge_samples[0].inside.rgb[0]
  }else{
   if($a[0].context[$kind] -ne 1 -or $b[0].context[$kind] -ne 2){throw 'control-context-values'}
   $b[0].context[$kind]=$a[0].context[$kind]
  }
  if(($before|ConvertTo-Json -Depth 50 -Compress) -cne ($after|ConvertTo-Json -Depth 50 -Compress)){throw 'control-other-field-changed'}
  $differences++
 }
 if($differences -ne 1){throw 'control-not-single-change'}
 $reports+=@{kind=$kind;compared_rows=256;changed_rows=$differences;only_intended_field_changed=$true;source_sha256=$mutation.source_sha256;control_sha256=$mutation.control_sha256}
}
$value=@{schema='pas.front-controller-exact-negative-input-audit.v1';passed=$true;controls=$reports;original_png_changed=$false}
$path=Join-Path $PSScriptRoot 'control-input-audit.json';$s=[IO.FileStream]::new($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
try{$bytes=[Text.Encoding]::UTF8.GetBytes(($value|ConvertTo-Json -Depth 10)+"`n");$s.Write($bytes);$s.Flush($true)}finally{$s.Dispose()}
'single-field controls independently verified=3'
