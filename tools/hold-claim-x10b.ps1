param([ValidateSet('Prepare','Build','Tests','Baseline','Variant')][string]$Mode,[string]$Tag='1')
$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$campaignPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue'
$batchPath=Join-Path $campaignPath 'hold-claim-x10b'
$exportPath=Join-Path $repoPath 'out/x10b/c36h-interior'
$buildPath=Join-Path $repoPath 'out/x10b/build'
if($Tag -notmatch '^[a-zA-Z0-9-]{1,24}$'){throw 'invalid tag'}
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Bytes($p){if(!(Test-Path -LiteralPath $p)){return 0};[long](Get-ChildItem -LiteralPath $p -File -Recurse|Measure-Object Length -Sum).Sum}
function Save($name,$value){$data=[Text.UTF8Encoding]::new($false).GetBytes(($value|ConvertTo-Json -Depth 40)+"`n");$s=[IO.File]::Open("$batchPath/$name",[IO.FileMode]::CreateNew,[IO.FileAccess]::Write);try{$s.Write($data,0,$data.Length)}finally{$s.Dispose()}}
function Run($name,$exe,[string[]]$a){if(Test-Path "$batchPath/$name-command.json"){throw 'new attempt required'};Save "$name-command.json" @{exe=$exe;arguments=$a;PAS_RGB_CLIP_ROOT=$env:PAS_RGB_CLIP_ROOT};& $exe @a *> "$batchPath/$name.log";$code=$LASTEXITCODE;Save "$name-exit.json" @{exit=$code;log_sha256=(Hash "$batchPath/$name.log")};Write-Output "$name exit=$code";return $code}
if($Mode -eq 'Prepare'){
  if((Test-Path $batchPath) -or (Test-Path "$repoPath/out/x10b")){throw 'new batch/export required'}
  if((Bytes $campaignPath)+(Bytes "$repoPath/measurements/research-next-20261001")+134217728 -gt 8589934592){throw 'batch reserve'}
  New-Item -ItemType Directory $batchPath,$exportPath|Out-Null
  Save 'protocol-before.json' @{
    hypothesis='A history-derived approaching front is not a new edge when it lies strictly inside a same-direction current held body and both current blue side bands cross it.'
    difference_from_rejected_donor='new current-pixel continuity witness and body extent/type contract, not tuning width/distance thresholds; current raw/direct candidates and all owner/identity rules retained'
    intervention='only before fallback emits a recovered approaching candidate; no inferred pixels/alias/lease extension/Down retry'
    controls=@('disjoint normal body intervals','claimed tangent differs from incoming/line','missing current RGB','one missing side','true leading edge','direct front','active held observation','stale line','rotated body','A/B/D/E fixed windows')
    stop='any unsolved synthetic/owner guard failure or current-witness ambiguity blocks adoption; no threshold search. Lack of counterexample is not real physical-role gold.'
    trace_window='H2440-2495 expanded to include X10 first full-prefix action difference2447'
    limits=@{batch_bytes=134217728;campaign_plus_prior_bytes=8589934592;build_export_bytes=536870912;initial_replays=2;extra_trace_off_repeat_max=1;live=0}
  }
  $original="$repoPath/out/x1/c36h-v3"
  $prov=Get-Content "$campaignPath/contact-replay-x1/source-v2/c36h-source-provenance.json" -Raw|ConvertFrom-Json
  foreach($p in $prov.instrumented_source_sha256.PSObject.Properties){if((Hash "$original/$($p.Name)") -ne $p.Value){throw 'frozen export changed'}}
  Copy-Item -LiteralPath "$original/src","$original/include","$original/tests" -Destination $exportPath -Recurse
  $game="$exportPath/src/game.cpp";$before=Hash $game;$text=[IO.File]::ReadAllText($game)
  $anchor='note.recent_identity=track.id;'+"`n"+'        std::erase_if(notes,[&](const NoteCandidate& incoming) {'
  if(($text.Split(@($anchor),[StringSplitOptions]::None)).Count -ne 2){throw 'unique insertion'}
  $text="#include `"x10b_claim.hpp`"`n"+$text.Replace($anchor,'note.recent_identity=track.id;'+"`n"+'        if(x10b::current_interior_claim(f,note,*line,claimed_outlines)) continue;'+"`n"+'        std::erase_if(notes,[&](const NoteCandidate& incoming) {')
  [IO.File]::WriteAllText($game,$text,[Text.UTF8Encoding]::new($false))
  $prov.instrumented_source_sha256.'src/game.cpp'=Hash $game
  $prov|Add-Member -NotePropertyName experiment_variant -NotePropertyValue 'X10b-C36h-current-interior-front-witness'
  Save 'c36h-source-provenance.json' $prov
  Save 'patch-binding.json' @{before_game_sha256=$before;after_game_sha256=(Hash $game);helper_sha256=(Hash "$repoPath/apps/frame_review/x10b_claim.hpp");parent_sha256=(Hash "$campaignPath/contact-replay-x1/source-v2/c36h-source-provenance.json")}
  $m=Get-Content "$campaignPath/hold-claim-x10/input-manifest.json" -Raw|ConvertFrom-Json;$m.batch_root=$batchPath
  ($m.windows|Where-Object id -eq 'H2479').first=2440
  Save 'input-manifest.json' $m
}elseif($Mode -eq 'Build'){
  $c=Run "configure-$Tag" 'cmake' @('-S',"$repoPath/apps/frame_review/x10b_offline",'-B',$buildPath,'-G','Visual Studio 18 2026','-A','x64','-T','v145','-DCMAKE_GENERATOR_INSTANCE=C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools,version=18.9.12105.275',"-DX1_SOURCE=$exportPath","-DX1_REPO=$repoPath","-DCMAKE_PREFIX_PATH=$repoPath/out/vcpkg_installed/x64-windows")
  if($c[-1] -ne 0){throw 'configure failed'}
  $c=Run "build-$Tag" 'cmake' @('--build',$buildPath,'--config','Release','--target','pas_frame_review','x1_tests','x10b_claim_tests','--parallel','2')
  if($c[-1] -ne 0){throw 'build failed'}
  foreach($dest in @("$buildPath/Release","$buildPath/reused/Release")){Copy-Item -LiteralPath "$repoPath/out/vcpkg_installed/x64-windows/bin/z.dll","$repoPath/out/vcpkg_installed/x64-windows/bin/gtest.dll","$repoPath/out/vcpkg_installed/x64-windows/bin/gtest_main.dll" -Destination $dest}
  if((Bytes "$repoPath/out/x10b") -gt 536870912){throw 'build cap'}
}elseif($Mode -eq 'Tests'){
  $env:PAS_RGB_CLIP_ROOT="$campaignPath/acceptance36h-01/sessions/manual-session-16174738262800/pixel-clips"
  $c=Run "original-tests-$Tag" "$buildPath/reused/Release/x1_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$batchPath/original-tests-$Tag.xml")
  $d=Run "witness-tests-$Tag" "$buildPath/Release/x10b_claim_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$batchPath/witness-tests-$Tag.xml")
  if($c[-1] -ne 0 -or $d[-1] -ne 0){throw 'tests failed; no adoption'}
}else{
  $baseline=$Mode -eq 'Baseline';$exe=if($baseline){"$repoPath/out/x1/repair-tools36/pas_frame_review.exe"}else{"$buildPath/reused/Release/pas_frame_review.exe"}
  $prov=if($baseline){"$campaignPath/contact-replay-x1/source-v2/c36h-source-provenance.json"}else{"$batchPath/c36h-source-provenance.json"}
  if($baseline -and (Hash $exe) -ne '5919a4d1316b4e7a8a6bc4d44a416f606e5108a1e97a95f7255dfb1fff5902b0'){throw 'baseline binary changed'}
  $name="$($Mode.ToLowerInvariant())-$Tag"
  $c=Run $name $exe @('contact',"$batchPath/input-manifest.json",$prov,"$batchPath/$name",'on','owner','frame-first')
  if($c[-1] -ne 0){throw 'replay failed; preserved'}
}
