$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$batchPath="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue/pending-cancel-x10d-p"
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Save($name,$value){$s=[IO.File]::Open("$batchPath/$name",[IO.FileMode]::CreateNew,[IO.FileAccess]::Write);try{$b=[Text.UTF8Encoding]::new($false).GetBytes(($value|ConvertTo-Json -Depth 40)+"`n");$s.Write($b,0,$b.Length)}finally{$s.Dispose()}}
$tests=@()
foreach($name in @('baseline-original-2','baseline-red-2','variant-original-2','variant-green-2','asan-original-3','asan-green-3')){
 [xml]$x=Get-Content -LiteralPath "$batchPath/$name.xml" -Raw
 $t=$x.testsuites;$cases=@($t.testsuite.testcase);$fail=@($cases|Where-Object {$null -ne $_.failure}|ForEach-Object {$_.name})
 if([int]$t.errors -ne 0 -or [int]$t.disabled -ne 0 -or @($cases|Where-Object {$null -ne $_.skipped}).Count -ne 0){throw 'unexpected test skip/error'}
 $n=if($name -like '*original*'){207}else{13}
 if([int]$t.tests -ne $n){throw 'test denominator'}
 if($name -in @('variant-original-2','asan-original-3')){if($fail.Count -ne 1 -or $fail[0] -ne 'SingleMissingDragFrameKeepsFutureDownButGraceExpiryCancelsIt'){throw 'nondeclared original regression'}}
 elseif($name -eq 'baseline-red-2'){if($fail.Count -ne 5){throw 'unexpected red set'}}
 elseif($fail.Count -ne 0){throw 'unexpected test failure'}
 $tests+=@{name=$name;sha256=(Hash "$batchPath/$name.xml");tests=$n;failures=$fail;skips=0;errors=0}
}
$sources=@();$files=@(Get-ChildItem -LiteralPath "$repoPath/apps/frame_review" -Recurse -File)+@(Get-Item -LiteralPath "$repoPath/tests/contact_replay_tests.cpp","$repoPath/tests/x10d_p_owner_tests.cpp","$repoPath/docs/PENDING_CANCEL_X10D_P_PROTOCOL_20261003.md")+@(Get-ChildItem -LiteralPath "$repoPath/tools" -File |Where-Object {$_.Name -like '*pending-cancel-x10d-p*'})
foreach($f in $files){$rel=$f.FullName.Substring($repoPath.Length+1).Replace('\','/');$dest="$batchPath/source-before-replay/$rel";[IO.Directory]::CreateDirectory((Split-Path $dest))|Out-Null;[IO.File]::Copy($f.FullName,$dest,$false);$sources+=@{path=$rel;sha256=(Hash $dest)}}
$exports=@();$filesCompared=@()
foreach($role in @('baseline','variant')){
 $prov=Get-Content -LiteralPath "$batchPath/$role-provenance.json" -Raw|ConvertFrom-Json
 foreach($e in $prov.instrumented_source_sha256.PSObject.Properties){if((Hash "$($prov.export_root)/$($e.Name)") -ne $e.Value){throw 'provenance mismatch'}}
 foreach($f in Get-ChildItem -LiteralPath "$repoPath/out/x10d-p/$role" -File -Recurse){$rel=$f.FullName.Substring(("$repoPath/out/x10d-p/$role").Length+1).Replace('\','/');$h=Hash $f.FullName;$exports+=@{role=$role;path=$rel;sha256=$h}
  $parentHash=Hash "$repoPath/out/x10c/baseline/$rel"
  if($role -eq 'baseline' -and $h -ne $parentHash){throw 'baseline modified'}
  if($role -eq 'variant' -and $rel -ne 'src/game.cpp' -and $h -ne $parentHash){throw 'additional candidate change'}
  $filesCompared+=@{role=$role;path=$rel;parent_sha256=$parentHash;sha256=$h;equal=($h -eq $parentHash)}
 }
}
$binaries=@();foreach($f in Get-ChildItem -LiteralPath "$repoPath/out/x10d-p/build/Release","$repoPath/out/x10d-p/asan/Debug" -File|Where-Object {$_.Extension -in @('.exe','.dll')}){$binaries+=@{path=$f.FullName.Substring($repoPath.Length+1).Replace('\','/');sha256=(Hash $f.FullName)}}
git diff --quiet HEAD -- src include CMakeLists.txt;if($LASTEXITCODE -ne 0){throw 'production changed'}
Save 'source-binding-before-replay.json' @{schema=1;head=(git rev-parse HEAD);sources=$sources;export_files=$exports;parent_comparison=$filesCompared;binaries=$binaries;tests=$tests;input_manifest_sha256=(Hash "$batchPath/input-manifest.json");protocol_sha256=(Hash "$batchPath/protocol-before.md");baseline_provenance_sha256=(Hash "$batchPath/baseline-provenance.json");variant_provenance_sha256=(Hash "$batchPath/variant-provenance.json");note='Only one semantic hook, suppression OFF. Final binding supersedes initial generated reader hash; engineering refinement before replay preserved. No replay/core rebuild after binding.'}
Write-Output 'Source/binary/tests binding completed before replay'
