$ErrorActionPreference='Stop'
$repoPath=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$batchPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi'
$s=Get-Content -LiteralPath "$batchPath/normalized.json" -Raw|ConvertFrom-Json -AsHashtable
foreach($c in $s.cases){
 if($c.id -eq 'V00'){foreach($f in $c.frames){$f.lines[0].center=@(320,575);$f.sequence=2}}
 if($c.id -eq 'V14' -and $c.guard.execution -eq 'unknown_down'){$c.guard.receipt_unknown=$true}
 if($c.id -eq 'V18' -and $c.name -like '*region-order'){$c.guard.attachment_query=$c.frames[0].queries.Count-1}
}
$p="$batchPath/normalized-execution.json";if(Test-Path -LiteralPath $p){throw 'execution fixture exists'}
[IO.File]::WriteAllText($p,($s|ConvertTo-Json -Depth 60)+"`n",[Text.UTF8Encoding]::new($false))
$t=Get-Content -LiteralPath "$batchPath/typed-inputs.json" -Raw|ConvertFrom-Json -AsHashtable
for($i=0;$i -lt $t.cases.Count;$i++){$t.cases[$i].guard=$s.cases[$i].guard;for($k=0;$k -lt $t.cases[$i].frames.Count;$k++){$f=$t.cases[$i].frames[$k];$sf=$s.cases[$i].frames[$k];$f.lines=$sf.lines;$f.sequence=$sf.sequence;$f.measured_frame_byte_count=[long]$sf.stride*$sf.height+$sf.rgb_byte_delta;$f.expected_frame_byte_count=[long]$sf.stride*$sf.height}}
foreach($pair in @(@('V00-whole','V00-touching'),@('V01-whole','V01-touching'))){$a=@($t.cases|Where-Object name -eq $pair[0])[0];$b=@($t.cases|Where-Object name -eq $pair[1])[0];for($i=0;$i -lt $a.frames.Count;$i++){$b.frames[$i].descriptors=$a.frames[$i].descriptors}}
$p="$batchPath/typed-execution.json";if(Test-Path -LiteralPath $p){throw 'typed execution exists'}
[IO.File]::WriteAllText($p,($t|ConvertTo-Json -Depth 60)+"`n",[Text.UTF8Encoding]::new($false))
$r=@{phase='pre build/execution authoring correction; oracle expectations unchanged';original_normalized_retained=$true;original_typed_retained=$true;oracle_sha256=(Get-FileHash "$batchPath/oracle.json").Hash.ToLowerInvariant();normalized_execution_sha256=(Get-FileHash "$batchPath/normalized-execution.json").Hash.ToLowerInvariant();typed_execution_sha256=(Get-FileHash "$batchPath/typed-execution.json").Hash.ToLowerInvariant();changes=@('V00 restore original line y575/frame sequence2','V14 enum unknown includes consistent unknown receipt','V18 reverse query order also remaps test-only attachment index','V00/V01 typed signature must be identical measured inputs, not private renderer object-list hash','typed byte-count provenance explicitly rejects invalid measurement frame')}
[IO.File]::WriteAllText("$batchPath/author-corrections.json",($r|ConvertTo-Json -Depth 10)+"`n",[Text.UTF8Encoding]::new($false))
Write-Output 'authoring input corrections saved as new files; oracle unchanged'
