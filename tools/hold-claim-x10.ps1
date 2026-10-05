param([ValidateSet('Prepare','Build','Tests','Replay')][string]$Mode,[string]$Tag='1')
$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$campaignPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue'
$batchPath=Join-Path $campaignPath 'hold-claim-x10'
$exportPath=Join-Path $repoPath 'out/x10/c36h-claim'
$buildPath=Join-Path $repoPath 'out/x10/build'
if($Tag -notmatch '^[a-zA-Z0-9-]{1,24}$'){throw 'invalid tag'}
function Hash($path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
function Bytes($path){if(!(Test-Path -LiteralPath $path)){return 0};[long](Get-ChildItem -LiteralPath $path -File -Recurse|Measure-Object Length -Sum).Sum}
function Save($name,$value){$data=[Text.UTF8Encoding]::new($false).GetBytes(($value|ConvertTo-Json -Depth 40)+"`n");$s=[IO.File]::Open("$batchPath/$name",[IO.FileMode]::CreateNew,[IO.FileAccess]::Write);try{$s.Write($data,0,$data.Length)}finally{$s.Dispose()}}
function Run($name,$exe,[string[]]$a){if(Test-Path "$batchPath/$name-command.json"){throw 'new attempt required'};Save "$name-command.json" @{exe=$exe;arguments=$a};& $exe @a *> "$batchPath/$name.log";$code=$LASTEXITCODE;Save "$name-exit.json" @{exit=$code;log_sha256=(Hash "$batchPath/$name.log")};Write-Output "$name exit=$code";return $code}
if($Mode -eq 'Prepare'){
  if((Test-Path $batchPath) -or (Test-Path "$repoPath/out/x10")){throw 'new batch/export required'}
  if((Bytes $campaignPath)+(Bytes "$repoPath/measurements/research-next-20261001")+134217728 -gt 8589934592){throw 'full batch reserve'}
  New-Item -ItemType Directory $batchPath,$exportPath|Out-Null
  Save 'protocol-before.json' @{
    question='Does the main50 fallback current-rail claim alone prevent C36h2478 additional Down?'
    intervention='exact main50 fallback width/tangent-position claim only; no recovery ordering, association, owner, grace or pixel thresholds changed'
    known_counterexamples='Disjoint normal body intervals and orthogonal independent bodies must survive. Typed geometry contract tests deliberately expose missing donor guards; failures reject adoption, never rewrite expectations.'
    evaluation='Run original C36h synthetic suite, five donor contract tests, then at most one fixed-pixel full replay for causal localization even if donor contract rejected; no live candidate on failure'
    positive='H2478 additional contact disappears while original held contact persists'
    controls='A3498/B4986/E5287 and D6158-6220 traces; C5520 remains old preserved observation control and frozen synthetic Flick regression, not selected new trace'
    cap=@{batch_bytes=134217728;build_and_export_bytes=536870912;new_replays_max=1;live=0}
  }
  $original="$repoPath/out/x1/c36h-v3"
  $prov=Get-Content "$campaignPath/contact-replay-x1/source-v2/c36h-source-provenance.json" -Raw|ConvertFrom-Json
  foreach($p in $prov.instrumented_source_sha256.PSObject.Properties){if((Hash "$original/$($p.Name)") -ne $p.Value){throw 'frozen export SHA'}}
  Copy-Item -LiteralPath "$original/src","$original/include","$original/tests" -Destination $exportPath -Recurse
  $game="$exportPath/src/game.cpp";$before=Hash $game
  $text=[IO.File]::ReadAllText($game)
  $anchor='note.recent_identity=track.id;'+"`n"+'        std::erase_if(notes,[&](const NoteCandidate& incoming) {'
  if(($text.Split(@($anchor),[StringSplitOptions]::None)).Count -ne 2){throw 'unique donor insertion anchor'}
  $text="#include `"x10_claim.hpp`"`n"+$text.Replace($anchor,'note.recent_identity=track.id;'+"`n"+'        if(x10::donor_fallback_claim(note,*line,claimed_outlines)) continue;'+"`n"+'        std::erase_if(notes,[&](const NoteCandidate& incoming) {')
  [IO.File]::WriteAllText($game,$text,[Text.UTF8Encoding]::new($false))
  $prov.instrumented_source_sha256.'src/game.cpp'=Hash $game
  $prov|Add-Member -NotePropertyName experiment_variant -NotePropertyValue 'X10-C36h-main50-fallback-claim-only'
  Save 'c36h-source-provenance.json' $prov
  Save 'patch-binding.json' @{before_game_sha256=$before;after_game_sha256=(Hash $game);donor_helper_sha256=(Hash "$repoPath/apps/frame_review/x10_claim.hpp");parent_provenance_sha256=(Hash "$campaignPath/contact-replay-x1/source-v2/c36h-source-provenance.json")}
  $m=Get-Content "$campaignPath/hold-causal-x9/input-manifest.json" -Raw|ConvertFrom-Json
  $m.batch_root=$batchPath
  $m.windows=@($m.windows|Where-Object id -ne 'C5520')+@([ordered]@{id='D6214';first=6158;last=6220;anchor=6214})
  Save 'input-manifest.json' $m
  Write-Output 'X10 exact one-mechanism export prepared; production unchanged'
}elseif($Mode -eq 'Build'){
  $c=Run "configure-$Tag" 'cmake' @('-S',"$repoPath/apps/frame_review/x10_offline",'-B',$buildPath,'-G','Visual Studio 18 2026','-A','x64','-T','v145','-DCMAKE_GENERATOR_INSTANCE=C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools,version=18.9.12105.275',"-DX1_SOURCE=$exportPath","-DX1_REPO=$repoPath","-DCMAKE_PREFIX_PATH=$repoPath/out/vcpkg_installed/x64-windows")
  if($c[-1] -ne 0){throw 'configure failed'}
  $c=Run "build-$Tag" 'cmake' @('--build',$buildPath,'--config','Release','--target','pas_frame_review','x1_tests','x10_claim_tests','--parallel','2')
  if($c[-1] -ne 0){throw 'build failed'}
  foreach($dest in @("$buildPath/Release","$buildPath/reused/Release")){Copy-Item -LiteralPath "$repoPath/out/vcpkg_installed/x64-windows/bin/z.dll","$repoPath/out/vcpkg_installed/x64-windows/bin/gtest.dll","$repoPath/out/vcpkg_installed/x64-windows/bin/gtest_main.dll" -Destination $dest}
  if((Bytes "$repoPath/out/x10") -gt 536870912){throw 'build capacity'}
}elseif($Mode -eq 'Tests'){
  $c=Run 'original-tests' "$buildPath/reused/Release/x1_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$batchPath/original-tests.xml")
  $d=Run 'donor-tests' "$buildPath/Release/x10_claim_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$batchPath/donor-tests.xml")
  Write-Output "Original exit=$($c[-1]);donor contract exit=$($d[-1]);any failure blocks production adoption"
}else{
  $c=Run 'variant-replay' "$buildPath/reused/Release/pas_frame_review.exe" @('contact',"$batchPath/input-manifest.json","$batchPath/c36h-source-provenance.json","$batchPath/c36h-claim-on-1",'on','owner','frame-first')
  if($c[-1] -ne 0){throw 'replay failed; preserve'}
}
