$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$campaignPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue'
$batchPath=Join-Path $campaignPath 'hold-cascade-x10c'
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$files=@(Get-ChildItem -LiteralPath "$repoPath/apps/frame_review" -File -Recurse)+@(Get-Item -LiteralPath "$repoPath/tests/contact_replay_tests.cpp","$repoPath/tests/x10c_diagnostics_tests.cpp")+@(Get-ChildItem -LiteralPath "$repoPath/tools" -File |Where-Object {$_.Name -like '*hold-cascade-x10c*'})
$sources=@()
foreach($f in $files){$rel=$f.FullName.Substring($repoPath.Length+1).Replace('\','/');$dest=Join-Path "$batchPath/source-before-replay" $rel;[IO.Directory]::CreateDirectory((Split-Path $dest))|Out-Null;[IO.File]::Copy($f.FullName,$dest,$false);$sources+=@{path=$rel;sha256=(Hash $dest)}}
$exports=@();foreach($role in @('baseline','variant')){foreach($f in Get-ChildItem -LiteralPath "$repoPath/out/x10c/$role" -File -Recurse){$exports+=@{role=$role;path=$f.FullName.Substring(("$repoPath/out/x10c/$role").Length+1).Replace('\','/');sha256=(Hash $f.FullName)}}}
$binaries=@();foreach($f in Get-ChildItem -LiteralPath "$repoPath/out/x10c/build/Release","$repoPath/out/x10c/asan/Debug" -File|Where-Object {$_.Extension -in @('.exe','.dll')}){$binaries+=@{path=$f.FullName.Substring($repoPath.Length+1).Replace('\','/');sha256=(Hash $f.FullName)}}
foreach($role in @('baseline','variant')){$p=Get-Content -LiteralPath "$batchPath/$role-provenance.json" -Raw|ConvertFrom-Json;foreach($e in $p.instrumented_source_sha256.PSObject.Properties){if((Hash "$($p.export_root)/$($e.Name)") -ne $e.Value){throw 'export/provenance mismatch'}}}
$value=@{schema=1;head=(git rev-parse HEAD);source_files=$sources;export_files=$exports;binaries=$binaries;input_manifest_sha256=(Hash "$batchPath/input-manifest.json");protocol_sha256=(Hash "$batchPath/protocol-before.md");baseline_provenance_sha256=(Hash "$batchPath/baseline-provenance.json");variant_provenance_sha256=(Hash "$batchPath/variant-provenance.json");note='Final source/binary binding before first replay; no build after binding. Earlier patch-bindings.json records initial generated reader, engineering-refinement file and build/test failure logs retained.'}
$data=[Text.UTF8Encoding]::new($false).GetBytes(($value|ConvertTo-Json -Depth 20)+"`n");$s=[IO.File]::Open("$batchPath/source-binding-before-replay.json",[IO.FileMode]::CreateNew,[IO.FileAccess]::Write);try{$s.Write($data,0,$data.Length)}finally{$s.Dispose()}
Write-Output 'Final replay sources and binaries bound'
