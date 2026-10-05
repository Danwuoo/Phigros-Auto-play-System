$ErrorActionPreference='Stop'
$repoPath=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$batchPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi'
function Clone($o){$o|ConvertTo-Json -Depth 50|ConvertFrom-Json -AsHashtable}
function Shape($x,$y,$d=200,$a=0,$w=140,$caps=$true,$kind='hold'){@{front=@($x,$y);depth=$d;angle=$a;width=$w;rails=$caps;kind=$kind}}
function Line($a=0,$x=320,$y=500,$len=640,$id=7){@{center=@($x,$y);angle=$a;length=$len;id=$id}}
function Frame($t,$shapes,$lines=@((Line))){@{time_ns=[long]$t;sequence=[long]($t+100000000);context=@{epoch=1;generation=1;geometry=1;rotation=0};source_valid=$true;width=640;height=640;stride=1920;rgb_byte_delta=0;shapes=@($shapes);queries=@($shapes|ForEach-Object{@{front=$_.front;width=$_.width;depth=$_.depth;angle=$_.angle;kind=$_.kind}});lines=@($lines);effects=@();clear=@()}}
$cases=[Collections.Generic.List[object]]::new();$oracles=[Collections.Generic.List[object]]::new()
function Add($id,$suffix,$frames,$expected,$guard=@{}){
 $name="$id-$suffix";$g=@{now_ns=[long]51000000;gate_deadline_ns=[long]200000000;plan_deadline_ns=[long]200000000;execution='known_down';prefix=1;contact_id=1;attachment_query=0;free_contacts=4;last_contact_ns=[long]30000000};foreach($k in $guard.Keys){$g[$k]=$guard[$k]};$cases.Add(@{id=$id;name=$name;frames=@($frames);guard=$g})
 $oracles.Add(@{name=$name;expected=$expected})
}
$times=@(10000000,30000000,50000000)
$v02=@(for($i=0;$i -lt 3;$i++){Frame $times[$i] @((Shape (300+4*$i) (@(490,500,504)[$i])) )})
$v03=@(for($i=0;$i -lt 3;$i++){Frame $times[$i] @((Shape 320 (@(580,600,640)[$i]) 20),(Shape 320 (@(450,480,500)[$i]) 120))})
$v07=@(Frame 10000000 @((Shape 320 500 60),(Shape 320 420 120));Frame 30000000 @((Shape 320 500 40),(Shape 320 440 120));Frame 50000000 @((Shape 320 500 180 0 140 $false)))
Add 'V00' 'whole' @((Frame 50000000 @((Shape 320 575 300 0 140 $false)))) @{body=@($true);physical='unknown';move=$false;down=@($false)} @{execution='never_executed';prefix=0}
Add 'V00' 'touching' @((Frame 50000000 @((Shape 320 575 175 0 140 $false),(Shape 320 400 125 0 140 $false)))) @{body=@($true);equal_to='V00-whole';physical='unknown';move=$false;down=@($false)} @{execution='never_executed';prefix=0}
# The touching world has exactly the same measured query as the whole world, not two private renderer objects.
$cases[$cases.Count-1].frames[0].queries=@(Clone $cases[$cases.Count-2].frames[0].queries[0])
foreach($world in @('whole','touching')){
 $fs=@(foreach($t in $times){if($world -eq 'whole'){Frame $t @((Shape 320 500 300 0 140 $false))}else{$f=Frame $t @((Shape 320 500 175 0 140 $false),(Shape 320 325 125 0 140 $false));$f.queries=@(@{front=@(320,500);depth=300;width=140;angle=0;kind='hold'});$f}})
 Add 'V01' $world $fs @{body=@($true);physical='unknown';move=$false;down=@($false);equal_to=if($world -eq 'touching'){'V01-whole'}else{$null}}
}
Add 'V02' 'continuation' (Clone $v02) @{body=@($true);contact=@('supported');cap=@($true);relation=@('continued');move=$true;down=@($false)}
$stationary=Clone $v02;$stationary+=Clone $v02[2];$stationary[3].time_ns=70000000;$stationary[3].sequence=170000000
Add 'V02' 'stationary-current' $stationary @{body=@($true);contact=@('supported');independent=3;move=$true;down=@($false)} @{now_ns=71000000}
Add 'V03' 'incoming' (Clone $v03) @{body=@($true,$true);contact=@('absent','supported');relation=@('continued','continued');move=$false;down=@($false,$true)}
$fs=Clone $v02;for($i=1;$i -lt 3;$i++){$fs[$i].effects=@(@{polygon=@(@((281+4*$i),(440+10*$i)),@((319+4*$i),(440+10*$i)),@((319+4*$i),(460+10*$i)),@((281+4*$i),(460+10*$i)));rgba=@(255,225,80,89)})}
Add 'V04' 'contained-effect' $fs @{body=@($true);contact=@('supported');move=$true;down=@($false);effect=$true}
$fs=Clone $v02;$fs[2].effects=@(@{polygon=@(@(220,485),@(400,485),@(400,520),@(220,520));rgba=@(255,225,80,255)})
Add 'V05' 'opaque-contact' $fs @{body=@($true);contact=@('occluded');move=$false;down=@($false);refresh=$false}
foreach($variant in @('black','effect')){$fs=Clone $v02;if($variant -eq 'black'){$fs[2].clear=@(@{x=@(238,266);y=@(300,505)})}else{$fs[2].effects=@(@{polygon=@(@(238,300),@(265,300),@(265,504),@(238,504));rgba=@(255,225,80,255)})};Add 'V06' $variant $fs @{body=@($false);move=$false;down=@($false);contact=@(if($variant -eq 'black'){'absent'}else{'occluded'})}}
Add 'V07' 'merge' (Clone $v07) @{body=@($true);relation=@('ambiguous');move=$false;down=@($false);alternatives=2}
foreach($order in @('forward','reverse')){$fs=Clone $v02;foreach($f in $fs){$f.lines=@((Line 0 320 500 640 7),(Line 0.04 320 500 640 8));if($order -eq 'reverse'){[array]::Reverse($f.lines)}};Add 'V08' $order $fs @{body=@($true);association='ambiguous';move=$false;down=@($false)}}
$fs=Clone $v02;for($i=0;$i -lt 3;$i++){$s=Shape 520 (@(460,480,500)[$i]) 8 0 80 $false 'tap';$fs[$i].shapes+=,$s;$fs[$i].queries+=,@{front=$s.front;depth=8;width=80;angle=0;kind='tap'}}
Add 'V09' 'tap' $fs @{body=@($true,$false);contact=@('supported','supported');move=$true;down=@($false,$true)}
$fs=@(for($i=0;$i -lt 3;$i++){Frame $times[$i] @((Shape (300+8*$i) 505 200 (@(0,0.1,0.25)[$i]))) @((Line (@(0,0.2,0.4)[$i])))})
Add 'V10' 'rotation' $fs @{body=@($true);contact=@('supported');move=$true;down=@($false);hit_line_error_max=0.001;hit_body_required=$true}
$fs=@(for($i=0;$i -lt 3;$i++){Frame $times[$i] @((Shape (280+20*$i) (@(400,450,500)[$i]) 120 (@(0.5,0.25,0)[$i]))) @((Line),(Line 0.5 600 500 80 9))})
Add 'V11' 'late-align' $fs @{body=@($true);contact=@('supported');association='unique';down=@($true);move=$false;line_id=7} @{execution='never_executed';prefix=0;attachment_query=-1}
Add 'V12' 'unknown' (Clone $v03) @{body=@($true,$true);move=$false;down=@($false,$false);release=$true} @{execution='unknown_down';prefix=1;receipt_unknown=$true}
Add 'V12' 'conflicting-enum-prefix' (Clone $v03) @{lifecycle_invalid=$true;move=$false;down=@($false,$false);release=$true} @{execution='unknown_down';prefix=0;receipt_unknown=$true}
Add 'V13' 'completed' (Clone $v03) @{move=$false;down=@($false,$false);completed_persists=$true} @{execution='completed_up';prefix=2}
foreach($state in @('known_down','unknown_down','never_executed')){$fs=@(for($i=0;$i -lt 3;$i++){Frame $times[$i] @((Shape 320 (@(490,510,495)[$i])))});Add 'V14' $state $fs @{completed_from_geometry=$false;down=@($false)} @{execution=$state;prefix=if($state -eq 'never_executed'){0}else{1}}}
$fs=@(for($i=0;$i -lt 3;$i++){Frame $times[$i] @((Shape 320 (@(600,620,640)[$i]) (@(108,112,128)[$i])) )})
Add 'V15' 'rear-crossing' $fs @{rear=@(492,508,512);past_line_count=2;up_at_second=$false;up_at_third=$true;down=@($false);geometry_only_completion=$false}
$mutations=@(
 @{n='source';field='source_valid';value=$false},@{n='epoch';context='epoch';value=2},@{n='generation';context='generation';value=2},@{n='geometry';context='geometry';value=2},@{n='rotation';context='rotation';value=1},
 @{n='nonincreasing';field='time_ns';value=30000000},@{n='future';field='time_ns';value=52000000},@{n='regions-overflow';bound='regions';value=129},@{n='lines-overflow';bound='lines';value=17},@{n='metadata-overflow';bound='metadata';value=1048577},@{n='probes-overflow';bound='probes';value=6291457},@{n='short-rgb';field='rgb_byte_delta';value=-1},@{n='nan';geometry='nan'},@{n='plan-expiry';guard='plan_deadline_ns';value=51000000},@{n='gate-expiry';guard='gate_deadline_ns';value=51000000},@{n='infinity';geometry='inf'})
foreach($m in $mutations){$fs=Clone $v02;$g=@{};if($m.context){$fs[2].context[$m.context]=$m.value};if($m.field){$fs[2][$m.field]=$m.value};if($m.bound){$fs[2].declared_usage=@{$m.bound=$m.value}};if($m.geometry){$fs[2].queries[0].width=$m.geometry};if($m.guard){$g[$m.guard]=$m.value};Add 'V16' $m.n $fs @{qualification_invalid=$true;move=$false;down=@($false);refresh=$false;current_body_preserved=($m.context -or $m.guard -or $m.field -eq 'time_ns')} $g}
foreach($m in @(@{n='regions-limit';k='regions';v=128},@{n='lines-limit';k='lines';v=16},@{n='metadata-limit';k='metadata';v=1048576},@{n='probes-limit';k='probes';v=6291456})){$fs=Clone $v02;$fs[2].declared_usage=@{$m.k=$m.v};Add 'V16' $m.n $fs @{qualification_invalid=$false;body=@($true);move=$true;down=@($false)}}
$v17=@(
 @{n='age90';t=@(-40000000,-22000000,-4000000,14000000,32000000,50000000);witness=-40000000;retained=$true},
 @{n='age90-plus1';t=@(-40000001,-22000000,-4000000,14000000,32000000,50000000);witness=-40000001;retained=$false},
 @{n='gap40';t=@(0,40000000,60000000);usable=$true},@{n='gap40-plus1';t=@(0,40000001,60000001);usable=$false},
 @{n='samples6';t=@(0,10000000,20000000,30000000,40000000,50000000);witness=0;retained=$true},
 @{n='samples7';t=@(0,10000000,20000000,30000000,40000000,50000000,60000000);witness=0;retained=$false},
 @{n='duplicate-rgb';t=$times;duplicate=$true;independent=1},@{n='same-descriptor-new-background';t=$times;same_descriptor=$true;independent=1})
foreach($m in $v17){$fs=@(for($i=0;$i -lt $m.t.Count;$i++){$s=Shape (300+2*$i) 504;$f=Frame $m.t[$i] @($s);if($m.duplicate -or $m.same_descriptor){$f=Frame $m.t[$i] @((Shape 308 504));if($m.same_descriptor){$f.effects=@(@{polygon=@(@(20,20),@(30,20),@(30,30),@(20,30));rgba=@((180+$i),200,0,255)})}};$f});$e=@{max_samples=6;max_window_ns=90000000};if($m.ContainsKey('retained')){$e.witness_retained=$m.retained};if($m.ContainsKey('usable')){$e.usable=$m.usable};if($m.independent){$e.independent=$m.independent};Add 'V17' $m.n $fs $e @{now_ns=[long]$m.t[-1]+1000000;witness_ns=$m.witness}}
foreach($base in @('V02','V03','V08')){foreach($change in @('runtime-ids','anchor-ids','region-order','line-order','contact-id')){
 $origin=@($cases|Where-Object{$_.id -eq $base})[0];$o=@($oracles|Where-Object{$_.name -eq $origin.name})[0];$fs=Clone $origin.frames;$g=Clone $origin.guard
 if($change -eq 'region-order'){foreach($f in $fs){[array]::Reverse($f.queries)}};if($change -eq 'line-order'){foreach($f in $fs){[array]::Reverse($f.lines)}};if($change -eq 'contact-id'){$g.contact_id=4};if($change -eq 'runtime-ids' -or $change -eq 'anchor-ids'){foreach($f in $fs){for($i=0;$i -lt $f.queries.Count;$i++){$f.queries[$i].diagnostic_id=9999-9996*$i}}}
 Add 'V18' "$base-$change" $fs @{equivalent_to=$origin.name;permutation=$change} $g
}}
foreach($world in @('two-physical','decorative-countermodel')){Add 'V19' $world (Clone $v03) @{equal_to=if($world -eq 'decorative-countermodel'){'V19-two-physical'}else{'V03-incoming'};physical='unknown';body=@($true,$true)}}
$designPath=Join-Path $repoPath 'docs/HOLD_OWNERSHIP_X10D_O_OBSERVABILITY_DESIGN_20261004.json'
$vectors=@{schema='bvi.normalized.v1';family='BVI-1';source_sha256=(Get-FileHash -LiteralPath $designPath).Hash.ToLowerInvariant();top_level_vectors=20;expanded_cases=$cases.Count;cases=@($cases.ToArray());runtime_input_excludes=@('shapes','effects','clear','guard','expected','case name','private physical labels')}
$oracle=@{schema='bvi.oracle.v1';independent_from_candidate=$true;top_level_vectors=20;expanded_cases=$oracles.Count;cases=@($oracles.ToArray());layer_denominator_rule='RGB/typed/lifecycle independently enumerate expanded cases, unsupported claims remain unverified';physical_gold=0;private_worlds=@{V00='whole vs seamless union';V01='identical bounded history';V19='two objects vs visual decoration'}}
foreach($pair in @(@('normalized.json',$vectors),@('oracle.json',$oracle))){$p=Join-Path $batchPath $pair[0];if(Test-Path -LiteralPath $p){throw 'contract exists'};[IO.File]::WriteAllText($p,($pair[1]|ConvertTo-Json -Depth 60)+"`n",[Text.UTF8Encoding]::new($false))}
$binding=@{phase='before candidate source';normalized_sha256=(Get-FileHash "$batchPath/normalized.json").Hash.ToLowerInvariant();oracle_sha256=(Get-FileHash "$batchPath/oracle.json").Hash.ToLowerInvariant();contract_sha256=(Get-FileHash "$PSScriptRoot/CONTRACT.md").Hash.ToLowerInvariant();protocol_sha256=(Get-FileHash "$PSScriptRoot/PROTOCOL.md").Hash.ToLowerInvariant();expanded_cases=$cases.Count;mapping=@($cases|ForEach-Object{@{top=$_.id;case=$_.name;frames=$_.frames.Count}});differences=@('V12 execution enums remove contradictory inherited unknown_down:false','V16 current context mutates while history unchanged','V17 complete90ms/40ms/6sample sequences and+1','line ambiguity preserves visible body','source freshness and boundary duplicate confirmations separate','V15 observed rear geometry only; normal owner Up unverified','2887 contact2 gone;684/197 identity retained,cursor null','RGBA alpha89, integer sampling, draw order and contact flanks explicit')}
[IO.File]::WriteAllText("$batchPath/specification-binding.json",($binding|ConvertTo-Json -Depth 20)+"`n",[Text.UTF8Encoding]::new($false))
Write-Output "normalized20 top-level vectors, $($cases.Count) expanded cases; oracle saved before candidate"
