$ErrorActionPreference='Stop'
$repoPath=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$batchPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi'
$spec=Get-Content -LiteralPath "$batchPath/normalized.json" -Raw|ConvertFrom-Json -AsHashtable
function Signature($s){$b=[Text.Encoding]::UTF8.GetBytes(($s|ConvertTo-Json -Depth 25 -Compress));$h=[Security.Cryptography.SHA256]::HashData($b);return [BitConverter]::ToUInt64($h,0).ToString()}
$result=@()
foreach($c in $spec.cases){
 $fs=@();foreach($f in $c.frames){$ds=@();foreach($q in $f.queries){
  $cap=$true;$body=($q.kind -ne 'tap');$contact='geometry';$depth=$q.depth
  if($c.id -in @('V00','V01')){$cap=$false}
  if($c.id -eq 'V07' -and $f.time_ns -eq 50000000){$cap=$false}
  if($q.front[1] -ge 640){$cap=$false}
  if($f.time_ns -eq $c.frames[-1].time_ns){
   if($c.id -eq 'V05'){$contact='occluded';$cap=$false}
   if($c.id -eq 'V06'){$body=$false;$cap=$false;$contact=if($c.name -like '*effect'){'occluded'}else{'absent'}}
  }
  $ds+=@{query=$q;body=$body;contact_measurement=$contact;front_end=$cap;rear_end=$true;measured_depth=$depth;rgb_signature=(Signature @{shapes=$f.shapes;effects=$f.effects;clear=$f.clear;lines=$f.lines});descriptor_signature=(Signature @{q=$q;cap=$cap;body=$body});provenance='test-only declared measured descriptor, independent of RGB extractor; no physical labels'}
 };$fs+=@{time_ns=$f.time_ns;sequence=$f.sequence;context=$f.context;source_valid=$f.source_valid;descriptors=$ds;lines=$f.lines;declared_usage=$f.declared_usage}}
 $result+=@{name=$c.name;frames=$fs;guard=$c.guard}
}
$p="$batchPath/typed-inputs.json";if(Test-Path -LiteralPath $p){throw 'typed fixture exists'}
[IO.File]::WriteAllText($p,(@{schema='bvi.typed.v1';source='explicit synthetic measured descriptor fixtures; no RGB recognition claim';oracle_read=$false;cases=$result}|ConvertTo-Json -Depth 55)+"`n",[Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText("$batchPath/typed-binding.json",(@{phase='typed fixture materialization before any candidate build/run; RGB normalized/oracle binding predates candidate source';sha256=(Get-FileHash -LiteralPath $p).Hash.ToLowerInvariant();cases=$result.Count;oracle_read=$false}|ConvertTo-Json)+"`n",[Text.UTF8Encoding]::new($false))
Write-Output "typed descriptor fixtures=$($result.Count)"
