. "$PSScriptRoot/r3-common.ps1"
if(!(Test-Path "$batchPath/source-binding-before-cost.json")){throw 'cost freeze prerequisite'}
if(Test-Path "$batchPath/qualification.json"){throw 'qualification already settled; no retry'}
$binding=Get-Content "$batchPath/source-binding-before-cost.json" -Raw|ConvertFrom-Json
foreach($f in $binding.files){if((R3Hash $f.path) -ne $f.sha256){throw "frozen input changed $($f.path)"}}
$runs=@();$stopped=$false;$reason='';$gate="$outPath/build-B0/Release/r3_gate.exe";$audit="$outPath/build-B0/Release/r3_audit.exe"
function Cost([string]$name,[string]$role,[string]$load) {
 if((R3Bytes $batchPath)+80MB+32MB -gt 2GB -or (Get-PSDrive C).Free -lt 5GB+80MB+32MB){throw 'run reserve'}
 R3Save "$name-preflight.json" @{capacity=(R3Capacity);source_binding_sha256=(R3Hash "$batchPath/source-binding-before-cost.json");reserve_bytes=80MB}
 $code=R3Run $name "$outPath/build-$role/Release/r3_meter.exe" @('cost',$load,"$batchPath/$name",'normal') @(0,1,2)
 if(!(Test-Path "$batchPath/$name/summary.json")){return @{name=$name;role=$role;load=$load;exit=$code;normal_gate=$false;raw_integrity=$false;reason='collector failed before summary; all partial artifacts preserved'}}
 $auditExit=R3Run "$name-audit" $audit @("$batchPath/$name","$batchPath/$name-audit.json") @(0,1)
 $s=Get-Content "$batchPath/$name/summary.json" -Raw|ConvertFrom-Json
 if((R3Bytes "$batchPath/$name") -gt 80MB){throw 'run physical cap violated'}
 $checkExit=R3Run "$name-check" $gate @('check',"$batchPath/$name") @(0,2)
 return @{name=$name;role=$role;load=$load;exit=$code;normal_gate=($code -eq 0 -and $checkExit -eq 0);raw_integrity=($auditExit -eq 0);summary_sha256=(R3Hash "$batchPath/$name/summary.json");file_bytes=(R3Bytes "$batchPath/$name")}
}
:AA foreach($load in @('rgb','owner')){foreach($i in 1..4){$r=Cost "aa-$load-$i" 'B0' $load;$runs+=$r;if(!$r.normal_gate -or !$r.raw_integrity){$stopped=$true;$reason="$($r.name) necessary normal or integrity gate insufficient";break AA}}}
$noise=$false;$comparison=$false
if(!$stopped){$code=R3Run 'noise' $gate @('noise',$batchPath) @(0,2);$noise=$code -eq 0;if(!$noise){$stopped=$true;$reason='A/A measurement noise adequacy failed'}}
if(!$stopped){:AB foreach($load in @('rgb','owner')){foreach($b in 1..2){$i=0;foreach($role in @('B0','B1','B1','B0')){$i++;$r=Cost "ab-$load-$b-$i-$role" $role $load;$runs+=$r;if(!$r.normal_gate -or !$r.raw_integrity){$stopped=$true;$reason="$($r.name) necessary normal or integrity gate insufficient";break AB}}}}
 if(!$stopped){$code=R3Run 'evaluate' $gate @('evaluate',$batchPath) @(0,2);$comparison=$code -eq 0;if(!$comparison){$reason='candidate run-quantile difference gate failed'}}
}
R3Save 'qualification.json' @{state=$(if($comparison){'cost-self-check-pass-awaiting-controller'}else{'not-ready'});runs=$runs;AA_runs=@($runs|Where-Object name -like 'aa-*').Count;ABBA_runs=@($runs|Where-Object name -like 'ab-*').Count;stress_runs=0;noise_gate=$noise;candidate_comparison_gate=$comparison;stop_reason=$reason;no_retry=$true;measurement_repairs_after_cost=0;gate_threshold_changed=$false;cost_frozen_sources_unchanged=$true;capacity=(R3Capacity)}
Write-Output "qualification settled: $reason; no further cost runs"
