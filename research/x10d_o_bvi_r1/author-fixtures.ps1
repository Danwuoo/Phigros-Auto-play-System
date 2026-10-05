$ErrorActionPreference='Stop'
$repoPath=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$batchPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r1'
$oldPath=Join-Path (Split-Path $batchPath) 'hold-ownership-x10d-o-bvi'
function ReadJson($p){Get-Content -LiteralPath $p -Raw|ConvertFrom-Json -AsHashtable}
function Clone($x){$x|ConvertTo-Json -Depth 30|ConvertFrom-Json -AsHashtable}
function Save($name,$v){$p=Join-Path $batchPath $name;if(Test-Path -LiteralPath $p){throw "exists $p"};[IO.File]::WriteAllText($p,($v|ConvertTo-Json -Depth 30)+"`n",[Text.UTF8Encoding]::new($false))}
function Hash($p){(Get-FileHash -LiteralPath $p).Hash.ToLowerInvariant()}
$spec=ReadJson "$oldPath/normalized-execution.json";$oldTyped=ReadJson "$oldPath/typed-execution.json";$oracle=ReadJson "$oldPath/oracle.json"
# Independent fixture authoring: explicit rails in declarative synthetic recipe;
# no candidate extraction, expected output, or physical object label is consulted.
$typed=Clone $oldTyped;$augment=[Collections.Generic.List[object]]::new()
for($ci=0;$ci -lt 69;$ci++){
 for($fi=0;$fi -lt $typed.cases[$ci].frames.Count;$fi++){
  $f=$spec.cases[$ci].frames[$fi];$tf=$typed.cases[$ci].frames[$fi]
  for($di=0;$di -lt $tf.descriptors.Count;$di++){
   $d=$tf.descriptors[$di];$q=$d.query;$rails=$false
   foreach($s in $f.shapes){if($s.rails -and ($s.front|ConvertTo-Json -Compress) -eq ($q.front|ConvertTo-Json -Compress) -and $s.width -eq $q.width -and $s.angle -eq $q.angle){$rails=$true}}
   $d.left=$d.right=[bool]($rails -and $d.body);$d.rails_provenance='R1 author-declared bilateral synthetic side rails, matching query geometry; test-only, not candidate output or physical gold'
   $augment.Add(@{case=$typed.cases[$ci].name;frame=$fi;descriptor=$di;left=$d.left;right=$d.right})
  }
 }
}
Save 'typed-r1.json' $typed
Save 'typed-provenance-diff.json' @{original_sha256=(Hash "$oldPath/typed-execution.json");added_fields=@('left','right','rails_provenance');effect_layer='RGB/e2e only; original typed effect not supplied';changes=@($augment.ToArray());old_fields_unchanged=$true}
$template=Clone (($spec.cases|Where-Object name -eq 'V02-continuation').frames[2])
function Frame([long]$time,[int[]]$xs=@(308)){
 $f=Clone $template;$f.time_ns=$time;$f.sequence=200000000+$time;$f.queries=@();$f.shapes=@();$f.effects=@();$f.clear=@()
 foreach($x in $xs){$q=[ordered]@{front=@($x,520);width=140;depth=200;angle=0;kind='hold'};$f.queries+=,$q;$s=Clone $q;$s.rails=$true;$f.shapes+=,$s}
 return $f
}
function TypedFrame($f){
 $ds=@();foreach($q in $f.queries){$sig=([long]$q.front[0]).ToString();$ds+=,@{query=(Clone $q);body=($f.shapes.Count -gt 0);front_end=($f.shapes.Count -gt 0);rear_end=($f.shapes.Count -gt 0);measured_depth=200;descriptor_signature=$sig;rgb_signature=$sig;left=($f.shapes.Count -gt 0);right=($f.shapes.Count -gt 0);rails_provenance='independent R1 straight bilateral rail fixture';provenance='independent R1 declarative straight endpoint descriptor; no candidate output';contact_measurement=if($f.shapes.Count -gt 0){'geometry'}else{'absent'}}}
 $t=@{time_ns=$f.time_ns;sequence=$f.sequence;context=(Clone $f.context);source_valid=$f.source_valid;lines=(Clone $f.lines);descriptors=$ds;expected_frame_byte_count=1228800;measured_frame_byte_count=1228800}
 if($f.Contains('guard_patch')){$t.guard_patch=Clone $f.guard_patch}
 return $t
}
$cases=[Collections.Generic.List[object]]::new()
function AddCase($name,$frames,$expected,[bool]$active=$false,$steps=@(),[bool]$receipt=$false){
 $g=@{execution=if($active){'known_down'}else{'never_executed'};prefix=if($active){1}else{0};receipt_unknown=$false;contact_id=3;attachment_query=if($active){0}else{-1};free_contacts=4;now_ns=[long]$frames[-1].time_ns+1000000;last_contact_ns=0;gate_deadline_ns=2000000000;plan_deadline_ns=2000000000}
 $cases.Add(@{name=$name;id='R1';frames=@($frames);typed_frames=@($frames|ForEach-Object{TypedFrame $_});guard=$g;expected=$expected;expected_steps=@($steps);update_receipt=$receipt})
}
$a=@((Frame 0 @(300,320)),(Frame 10000000 @(310)),(Frame 20000000 @(310)),(Frame 30000000 @(310)),(Frame 40000000 @(310)),(Frame 50000000 @(310)),(Frame 60000000 @(310)))
AddCase 'R01-count-eviction-ambiguity' $a @{relation=@('continued');alternatives=1;independent=1;usable=$false;down=@($false);equivalent_to='R01-without-oldest'} $false @(@{},@{},@{},@{},@{},@{relation=@('ambiguous');alternatives=2},@{relation=@('continued');alternatives=1})
AddCase 'R01-without-oldest' @($a[1..6]) @{relation=@('continued');alternatives=1;independent=1;usable=$false;down=@($false)}
$a=@((Frame 0 @(300)),(Frame 20000000 @(304)),(Frame 40000000 @(308)),(Frame 50000000 @(308)),(Frame 60000000 @(308)),(Frame 70000000 @(308)),(Frame 80000000 @(308)))
AddCase 'R02-count-eviction-confirmation' $a @{independent=2;span_ns=20000000;usable=$false;down=@($false);equivalent_to='R02-without-oldest'} $false @(@{},@{},@{},@{},@{},@{independent=3;span_ns=40000000;usable=$true;down=@($true)},@{independent=2;span_ns=20000000;usable=$false;down=@($false)})
AddCase 'R02-without-oldest' @($a[1..6]) @{independent=2;span_ns=20000000;usable=$false;down=@($false)}
$a=@((Frame 0 @(300,320)),(Frame 30000000 @(310)),(Frame 60000000 @(310)),(Frame 90000000 @(310)),(Frame 90000001 @(310)))
AddCase 'R03-age-eviction-ambiguity' $a @{relation=@('continued');alternatives=1;equivalent_to='R03-without-oldest'} $false @(@{},@{},@{},@{relation=@('ambiguous');alternatives=2},@{relation=@('continued');alternatives=1})
AddCase 'R03-without-oldest' @($a[1..4]) @{relation=@('continued');alternatives=1}
$a=@((Frame 0 @(300)),(Frame 30000000 @(304)),(Frame 60000000 @(308)),(Frame 90000000 @(308)),(Frame 90000001 @(308)))
AddCase 'R04-age-eviction-confirmation' $a @{independent=2;span_ns=30000000;usable=$false;down=@($false);equivalent_to='R04-without-oldest'} $false @(@{},@{},@{},@{independent=3;span_ns=60000000;usable=$true},@{independent=2;span_ns=30000000;usable=$false})
AddCase 'R04-without-oldest' @($a[1..4]) @{independent=2;span_ns=30000000;usable=$false}
AddCase 'R05-ABA' @((Frame 0 @(300)),(Frame 20000000 @(304)),(Frame 40000000 @(300))) @{independent=2;first_independent_ns=0;last_independent_ns=20000000;span_ns=20000000;freshness_ns=1000000;usable=$false;down=@($false)}
AddCase 'R06-duplicate-span-extension' @((Frame 0 @(300)),(Frame 10000000 @(304)),(Frame 20000000 @(308)),(Frame 40000000 @(308))) @{independent=3;first_independent_ns=0;last_independent_ns=20000000;span_ns=20000000;freshness_ns=1000000;usable=$false;down=@($false)}
AddCase 'R07-independent30-positive' @((Frame 0 @(300)),(Frame 10000000 @(304)),(Frame 30000000 @(308))) @{independent=3;span_ns=30000000;usable=$true;down=@($true);move=$false}
$steady=@((Frame 0 @(300)),(Frame 20000000 @(304)),(Frame 40000000 @(308)));foreach($ms in @(50,60,70,80,90,100,110,120)){$steady+=,(Frame ([long]$ms*1000000) @(308))}
function StationarySteps($fs){@($fs|ForEach-Object{@{move=$true;refresh=$true;release=$false;down=@($false);contact_id=3;receipt_prefix=1;last_contact_after_ns=[long]$_.time_ns+1000000}})}
AddCase 'R08-stationary-count-window' $steady @{independent=1;span_ns=0;usable=$false;move=$true;refresh=$true;release=$false;down=@($false);max_samples=6;max_window_ns=90000000;contact_id=3} $true (StationarySteps $steady) $true
$age=@((Frame 0 @(300)),(Frame 30000000 @(304)),(Frame 60000000 @(308)),(Frame 90000000 @(308)),(Frame 120000001 @(308)),(Frame 150000001 @(308)))
AddCase 'R09-stationary-age-window' $age @{independent=1;span_ns=0;usable=$false;move=$true;refresh=$true;release=$false;down=@($false);max_samples=6;max_window_ns=90000000;contact_id=3} $true (StationarySteps $age) $true
foreach($kind in @('loss','line-ambiguity')){
 $fs=Clone $steady;$steps=StationarySteps $steady
 foreach($ms in @(140,160,180)){$f=Frame ([long]$ms*1000000) @(308);if($kind -eq 'loss'){$f.shapes=@()}else{$l=Clone $f.lines[0];$l.id=9;$f.lines+=,$l};$fs+=,$f;$steps+=,@{move=$false;refresh=$false;release=($ms -eq 180);down=@($false);contact_id=3;receipt_prefix=1;last_contact_after_ns=121000000}}
 AddCase "R10-stationary-$kind" $fs @{move=$false;refresh=$false;release=$true;down=@($false)} $true $steps $true
}
foreach($kind in @('unknown','completed','plan-expired','gate-expired','source-invalid')){
 $fs=Clone $steady;$steps=StationarySteps $steady;$f=Frame 140000000 @(308);$patch=@{}
 switch($kind){'unknown'{$patch=@{execution='unknown_down';prefix=1;receipt_unknown=$true}} 'completed'{$patch=@{execution='completed_up';prefix=2}} 'plan-expired'{$patch=@{plan_deadline_ns=141000000}} 'gate-expired'{$patch=@{gate_deadline_ns=141000000}} 'source-invalid'{$f.source_valid=$false}}
 $f.guard_patch=$patch;$fs+=,$f;$completed=$kind -eq 'completed'
 $steps+=,@{move=$false;refresh=$false;release=(-not $completed);down=@($false);contact_id=3;receipt_prefix=if($completed){2}else{1};last_contact_after_ns=121000000;completed_persists=$completed}
 AddCase "R11-stationary-$kind" $fs @{move=$false;refresh=$false;release=(-not $completed);down=@($false);completed_persists=$completed} $true $steps $true
}
# References deliberately point forward: harness must resolve after all rows.
Save 'r1-cases.json' @{schema='bvi.r1.cases.v1';authoring='declarative geometry and independent expected, before candidate';cases=@($cases.ToArray());schema_controls=@(@{name='typo-required-field';expected=@{mov=$true};reject=$true},@{name='unknown-metadata';expected=@{permutaton='contact-id'};reject=$true},@{name='uncovered-required';expected=@{usable=$true};remove_coverage='usable';reject=$true},@{name='wrong-contact-output';kind='contact';guard_contact_id=4;actual_contact_id=1;reject=$true})}
$coverage=@{}
foreach($f in @('body','contact','cap','association','physical','line_id','hit_line_error_max','hit_body_required','current_body_preserved')){$coverage[$f]=@('rgb','typed','e2e')}
$coverage.effect=@('rgb','e2e')
foreach($f in @('relation','independent','alternatives','usable','max_samples','max_window_ns','witness_retained','rear','past_line_count','span_ns','first_independent_ns','last_independent_ns','freshness_ns')){$coverage[$f]=@('typed','e2e')}
foreach($f in @('move','down','refresh','release','lifecycle_invalid','completed_persists','completed_from_geometry','geometry_only_completion','up_at_second','up_at_third','contact_id','receipt_prefix','last_contact_after_ns')){$coverage[$f]=@('lifecycle','e2e')}
foreach($f in @('qualification_invalid','equal_to','equivalent_to')){$coverage[$f]=@('rgb','typed','lifecycle','e2e')}
$rows=@();foreach($c in @($oracle.cases)+@($cases.ToArray())){foreach($f in $c.expected.Keys){if($f -eq 'permutation'){$layers=@('metadata')}elseif($coverage.ContainsKey($f)){$layers=$coverage[$f]}else{throw "no coverage $f"};$rows+=,@{case=$c.name;field=$f;layers=$layers;handler=$f}};for($i=0;$i -lt $c.expected_steps.Count;$i++){foreach($f in $c.expected_steps[$i].Keys){if(-not $coverage.ContainsKey($f)){throw "step coverage $f"};$rows+=,@{case=$c.name;step=$i;field=$f;layers=$coverage[$f];handler=$f}}}}
Save 'expected-coverage.json' @{schema='bvi.expected.coverage.v1';handlers=$coverage;metadata=@{permutation=@('runtime-ids','anchor-ids','region-order','line-order','contact-id')};rows=$rows;original_cases=69;original_supplemental=22;new_cases=$cases.Count;schema_controls=4;effect_typed_applicable=$false;contact_mapping=@{from=1;to=4}}
Save 'specification-freeze.json' @{phase='before candidate source repair';old_oracle=@{path="$oldPath/oracle.json";sha256=(Hash "$oldPath/oracle.json")};files=@('r1-cases.json','typed-r1.json','typed-provenance-diff.json','expected-coverage.json'|ForEach-Object{$p=Join-Path $batchPath $_;@{path=$p;bytes=(Get-Item $p).Length;sha256=(Hash $p)}});contract=@{path="$PSScriptRoot/CONTRACT.md";sha256=(Hash "$PSScriptRoot/CONTRACT.md")};protocol=@{path="$PSScriptRoot/PROTOCOL.md";sha256=(Hash "$PSScriptRoot/PROTOCOL.md")};original_denominator=69;supplemental=22;new_cases=$cases.Count;schema_controls=4;utc=[DateTime]::UtcNow.ToString('o')}
Write-Output (@{new_cases=$cases.Count;original=69;supplemental=22;schema_controls=4;coverage_rows=$rows.Count;candidate_executed=$false}|ConvertTo-Json -Compress)
