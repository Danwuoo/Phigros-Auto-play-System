param([Parameter(Mandatory)][string]$SourceTrace)
$ErrorActionPreference='Stop'
$pkg=$PSScriptRoot
foreach($kind in @('epoch','geometry','front-rgb')) {
 $target=Join-Path $pkg ('negative-'+$kind+'.rows.jsonl')
 $writer=[IO.StreamWriter]::new([IO.FileStream]::new($target,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read),[Text.UTF8Encoding]::new($false))
 $changed=0
 try {
  foreach($line in [IO.File]::ReadLines($SourceTrace)) {
   $row=$line|ConvertFrom-Json -AsHashtable
   if($row.ordinal -eq 3030) {
    $result=$row.current_front_sidecar.results|Where-Object candidate_id -eq 2
    if(@($result).Count -ne 1){throw 'single-control-witness'}
    if($kind -eq 'epoch'){$result.context.epoch=2}
    elseif($kind -eq 'geometry'){$result.context.geometry=2}
    else {$result.edge_samples[0].inside.rgb[0]=156}
    $writer.WriteLine(($row|ConvertTo-Json -Depth 50 -Compress));$changed++
   } else {$writer.WriteLine($line)}
  }
 } finally {$writer.Flush();$writer.Dispose()}
 if($changed -ne 1){throw 'exactly-one-control-row'}
 $value=@{source=$SourceTrace;source_sha256=(Get-FileHash $SourceTrace).Hash.ToLowerInvariant();
  control=$target;control_sha256=(Get-FileHash $target).Hash.ToLowerInvariant();kind=$kind;changed_rows=$changed;
  field=$(if($kind -eq 'front-rgb'){'3030 candidate2 edge_samples[0].inside.rgb[0]:155->156'}else{'3030 candidate2 context.'+$kind+':1->2'});
  policy_use=$false;original_png_changed=$false}
 $json=Join-Path $pkg ('negative-'+$kind+'-mutation.json')
 $s=[IO.FileStream]::new($json,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
 try{$bytes=[Text.Encoding]::UTF8.GetBytes(($value|ConvertTo-Json -Depth 8));$s.Write($bytes);$s.Flush($true)}finally{$s.Dispose()}
}