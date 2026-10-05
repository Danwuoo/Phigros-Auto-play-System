$ErrorActionPreference='Stop'
$script:Repo='C:\Users\wurre\Desktop\Phigros-Auto-play-System'
$script:Attempt='bvi-r2f-20261005-02'
$script:Source=Join-Path $Repo 'research/x10d_o_bvi_r2f_resume'
$script:Campaign=Join-Path $Repo 'measurements/game-assist/2026-09-30-m0-manual-continue'
$script:Evidence=Join-Path $Campaign 'hold-ownership-x10d-o-bvi-r2f-resume'
$script:Out=Join-Path $Repo 'out/x10d-o-bvi-r2f-resume'
function ReadStream($p){[IO.FileStream]::new($p,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)}
function Json($p){$h=ReadStream $p;try{if($h.Length -gt 67108864){throw 'json-read-cap'};$r=[IO.StreamReader]::new($h,[Text.Encoding]::UTF8,$true,4096,$true);try{$r.ReadToEnd()|ConvertFrom-Json -Depth 100}finally{$r.Dispose()}}finally{$h.Dispose()}}
function Sha($p){$h=ReadStream $p;try{$sha=[Security.Cryptography.SHA256]::Create();try{[Convert]::ToHexString($sha.ComputeHash($h)).ToLowerInvariant()}finally{$sha.Dispose()}}finally{$h.Dispose()}}
function Entry($p){if(-not (Test-Path -LiteralPath $p -PathType Leaf)){throw "Could not find input: $p"};$f=Get-Item -LiteralPath $p;@{path=$f.FullName;bytes=[long]$f.Length;sha256=(Sha $p)}}
function Bytes($p){$n=[long]0;if(Test-Path -LiteralPath $p){foreach($f in Get-ChildItem -LiteralPath $p -Recurse -File){$n+=$f.Length}};$n}
function TextSha($a){[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes(($a -join "`n")+"`n"))).ToLowerInvariant()}
function NoAlias($p){$a=[IO.Path]::GetFullPath($p);while($a){if(Test-Path -LiteralPath $a){if((Get-Item -LiteralPath $a).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'reparse'}};$parent=[IO.Path]::GetDirectoryName($a);if($parent -eq $a){break};$a=$parent}}
function ExactPath($raw,$allowed,[scriptblock]$alias={param($p)NoAlias $p}){
 if($raw -match '(^|[\\/])\.\.([\\/]|$)' -or $raw -match '(^|[\\/])[^\\/]*[ .]([\\/]|$)' -or $raw -match '^\\\\' -or $raw -notmatch '^[a-zA-Z]:[\\/]' -or $raw.Substring(2).Contains(':')){throw 'raw-path'}
 $p=[IO.Path]::GetFullPath($raw);$expected=[IO.Path]::GetFullPath($allowed)
 if(-not $p.StartsWith($Repo+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or -not [string]::Equals($p,$expected,[StringComparison]::OrdinalIgnoreCase)){throw 'exact-allowlist'}
 & $alias $p;return $p
}
function Binding(){[pscustomobject]@{repo=$Repo;source=(ExactPath $Source $Source);evidence=(ExactPath $Evidence $Evidence);out=(ExactPath $Out $Out);attempt=$Attempt}}
function InitializeRoots([scriptblock]$exists={param($p)Test-Path -LiteralPath $p}){if((& $exists $Evidence) -or (& $exists $Out)){throw 'initialize-existing-root'}}
function CheckEntry($e){$a=Entry $e.path;if($a.sha256 -cne $e.sha256 -or $a.bytes -ne $e.bytes){throw 'source-input-sha'}}
function ExpectedExit($r,$native,$runner){if($null -eq $r.exit_code -or $r.exit_code -ne $native -or $r.runner_exit -ne $runner){throw 'exit-predicate'}}
function PutHandle([IO.FileStream]$h,$v,[int]$cap=65536){$data=[Text.Encoding]::UTF8.GetBytes(($v|ConvertTo-Json -Depth 100 -Compress)+"`n");if($data.Length -gt $cap){throw 'json-cap'};$h.Position=0;$h.SetLength(0);$h.Write($data);$h.Flush($true);$h.Position=0;$got=[byte[]]::new($data.Length);$n=$h.Read($got);if($n -ne $data.Length -or [Convert]::ToHexString($data) -cne [Convert]::ToHexString($got)){throw 'readback'};$h.Position=$h.Length}
function NewHandle($p){NoAlias $p;[IO.FileStream]::new($p,[IO.FileMode]::CreateNew,[IO.FileAccess]::ReadWrite,[IO.FileShare]::Read)}
function NewJson($p,$v){$h=NewHandle $p;try{PutHandle $h $v}finally{$h.Dispose()}}
function CapacityMath([long]$dev,[long]$out,[long]$reserveDevelopment,[long]$reserveOut,[long]$free){
 if($dev -lt 0 -or $out -lt 0 -or $reserveDevelopment -lt 0 -or $reserveOut -lt 0){throw 'capacity-enumeration'}
 if($dev -gt 41786831 -or 7450899+$dev -gt 58720256 -or $out -gt 134217728 -or 374705+$out -gt 268435456 -or 8284363856+$dev+$out+7088708 -gt 8589934592 -or $free -lt 5704253440){throw 'capacity-actual-hard-stop'}
 if($dev+$reserveDevelopment -gt 41786831 -or 7450899+$dev+$reserveDevelopment -gt 58720256 -or $out+$reserveOut -gt 134217728 -or 374705+$out+$reserveOut -gt 268435456 -or 8284363856+$dev+$out+$reserveDevelopment+$reserveOut+7088708 -gt 8589934592){throw 'PREFLIGHT_REJECTED-estimate'}
}
function Capacity([long]$reserveDevelopment=0,[long]$reserveOut=0){
 $batch=Bytes $Evidence;$external=Bytes $Source
 foreach($leaf in @('HOLD_OWNERSHIP_X10D_O_BVI_R2F_RESUME_RESULT_20261005.md','HOLD_OWNERSHIP_X10D_O_BVI_R2F_RESUME_HANDOFF_20261005.md')){$p=Join-Path "$Repo/docs" $leaf;if(Test-Path $p){$external+=(Get-Item $p).Length}}
 $newOut=Bytes $Out;$development=$batch+$external;$aggregate=8284363856+$development+$newOut;$free=(Get-PSDrive C).Free
 CapacityMath $development $newOut $reserveDevelopment $reserveOut $free
 @{batch=$batch;external=$external;development_new=$development;development_total=7450899+$development;out_new=$newOut;out_total=374705+$newOut;aggregate=$aggregate;aggregate_remaining=8589934592-$aggregate;controller_reserved=7088708;free=$free;reserve_development=$reserveDevelopment;reserve_out=$reserveOut}
}
function Protection(){
 $cache=@{};$checks=0
 function Check($e){if(-not $e.path){throw 'empty-anchor'};$p=[IO.Path]::GetFullPath($e.path);$script:protectionChecks++;if(-not $cache.ContainsKey($p)){$cache[$p]=Entry $p};$a=$cache[$p];if($a.bytes -ne $e.bytes -or $a.sha256 -cne $e.sha256){throw "protection: $p"}}
 $script:protectionChecks=0;$r2d=Join-Path $Campaign 'hold-ownership-x10d-o-bvi-r2d';$i=Json "$r2d/controller-review/integrity.json"
 foreach($e in $i.source_anchors){Check $e}
 $b=Json "$r2d/protection-before.json";$r=Json "$r2d/final-receipt-corrected.json";$l=Json "$r2d/artifact-ledger-corrected.json"
 foreach($e in @($b.full_manifest,$b.controller_ledger,$b.controller_receipt,$b.controller_self_receipt_anchor)+@($b.review_added)){Check $e}
 foreach($e in (Json $b.full_manifest.path).files){Check $e}
 $pl=Json $b.controller_ledger.path;$pr=Json $b.controller_receipt.path
 foreach($e in @($pl.files)+@($pl.external)+@($pr.development_receipt,$pr.artifact_ledger)){Check $e}
 foreach($e in @($l.files)+@($l.external)+@($r.design,$r.protection_before,$r.protection_after,$r.controller_carry,$r.artifact_ledger,$r.superseded_failed_receipt,$r.maintenance_failure)){Check $e}
 $reconstructed=$cache.Count;if($reconstructed -ne 2134){throw "anchor count $reconstructed"}
 Check $i.delivery_receipt
 $cl=Json "$r2d/controller-review/artifact-ledger.json";foreach($e in @($cl.files)+@($cl.external)){Check $e}
 $cr=Json "$r2d/controller-review/controller-receipt.json";Check $cr.artifact_ledger
 # The controller self receipt is anchored by the subsequent dispatch receipt.
 $dispatch=Json "$Campaign/hold-ownership-x10d-o-bvi-r2e-dispatch/dispatch-receipt.json"
 foreach($e in @($dispatch.prior_receipt,$dispatch.dispatch_document,$dispatch.artifact_ledger)){Check $e}
 $dl=Json $dispatch.artifact_ledger.path;foreach($e in @($dl.files)+@($dl.external)){Check $e}
 $postAnchors=@();foreach($p in @("$r2d/controller-review/controller-receipt.json","$Campaign/hold-ownership-x10d-o-bvi-r2e-dispatch/dispatch-receipt.json")){$e=Entry $p;$postAnchors+=$e;Check $e}
 $r2e=Join-Path $Campaign 'hold-ownership-x10d-o-bvi-r2e'
 foreach($e in (Json "$r2e/protection-before-corrected.json").post_anchors){Check $e}
 if($cache.Count -ne 2145){throw 'R2E original protection count'}
 $ledger=Json "$r2e/artifact-ledger.json";$receipt=Json "$r2e/final-receipt.json"
 Check $receipt.artifact_ledger
 foreach($e in @($ledger.files)+@($ledger.external)+@($ledger.out_entries)){Check $e}
 foreach($prop in $receipt.PSObject.Properties){if($null -ne $prop.Value -and $prop.Value.path -and $prop.Value.sha256){Check $prop.Value}}
 $freeze=Json "$r2e/freeze.json";$contract=Json "$r2e/contract.json"
 foreach($e in @($freeze.files)+@($contract.inputs)+@($contract.dependencies)){Check $e}
 $ecl=Json "$r2e/controller-review/artifact-ledger.json";$ecr=Json "$r2e/controller-review/controller-receipt.json"
 Check $ecr.artifact_ledger
 foreach($e in @($ecl.files)+@($ecl.external)){Check $e}
 foreach($prop in $ecr.PSObject.Properties){if($null -ne $prop.Value -and $prop.Value.path -and $prop.Value.sha256){Check $prop.Value}}
 Check (Entry "$r2e/controller-review/controller-receipt.json")
 $r2f=Join-Path $Campaign 'hold-ownership-x10d-o-bvi-r2f';$fr=Json "$r2f/final-receipt.json";$fl=Json "$r2f/artifact-ledger.json"
 if((Sha "$r2f/final-receipt.json") -cne '11f5f0255b0eb16bdd84301cefb8cb77c0142fe6df7e099ee865bf8d7697dfb9' -or (Sha "$r2f/state.json") -cne '35fa5fe81fdc2d0e775bcd68c5da3376f9a58d296ff0b5a4ae6dc7e7d7da7513'){throw 'R2F-anchor'}
 Check (Entry "$r2f/final-receipt.json");Check $fr.artifact_ledger
 foreach($e in @($fl.files)+@($fl.external)+@($fl.out_entries)){Check $e}
 foreach($prop in $fr.PSObject.Properties){if($null -ne $prop.Value -and $prop.Value.path -and $prop.Value.sha256){Check $prop.Value}}
 $head=(& git rev-parse HEAD).Trim();if($LASTEXITCODE -ne 0){throw 'git-head'};$index=Sha "$Repo/.git/index"
 $allPaths=@(& git -c core.quotepath=false ls-files --cached --others --exclude-standard);if($LASTEXITCODE -ne 0){throw 'git-paths'}
 $allDirty=@(& git -c core.quotepath=false status --porcelain=v1 --untracked-files=all);if($LASTEXITCODE -ne 0){throw 'git-dirty'}
 $basePaths=@($allPaths|Where-Object{$_ -notlike 'research/x10d_o_bvi_r2f_resume/*' -and $_ -notlike 'docs/HOLD_OWNERSHIP_X10D_O_BVI_R2F_RESUME_*'})
 $baseDirty=@($allDirty|Where-Object{$p=$_.Substring(3);$p -notlike 'research/x10d_o_bvi_r2f_resume/*' -and $p -notlike 'docs/HOLD_OWNERSHIP_X10D_O_BVI_R2F_RESUME_*'})
 if($basePaths.Count -ne 545){throw 'original-git545'}
 $paths=@($basePaths|Where-Object{$_ -notlike 'research/x10d_o_bvi_r2f/*' -and $_ -notlike 'docs/HOLD_OWNERSHIP_X10D_O_BVI_R2F_RESULT_*' -and $_ -notlike 'docs/HOLD_OWNERSHIP_X10D_O_BVI_R2F_HANDOFF_*'})
 $dirty=@($baseDirty|Where-Object{$p=$_.Substring(3);$p -notlike 'research/x10d_o_bvi_r2f/*' -and $p -notlike 'docs/HOLD_OWNERSHIP_X10D_O_BVI_R2F_RESULT_*' -and $p -notlike 'docs/HOLD_OWNERSHIP_X10D_O_BVI_R2F_HANDOFF_*'})
 if($head -cne $ecr.head -or $index -cne $ecr.index_sha256 -or $paths.Count -ne 532 -or (TextSha ($paths|Sort-Object)) -cne $ecr.git_paths_sha256 -or (TextSha $dirty) -cne $ecr.dirty_sha256 -or @(& git diff HEAD --name-only -- src include CMakeLists.txt).Count){throw 'git-state'}
 if(Test-Path "$Evidence/protection-before.json"){$before=Json "$Evidence/protection-before.json";if((TextSha ($paths|Sort-Object)) -cne $before.git_paths_sha256 -or (TextSha $dirty) -cne $before.dirty_sha256){throw 'dirty-change'}}
 @{schema=1;reconstructed=$reconstructed;original_R2E_protection=2145;unique_files=$cache.Count;references=$script:protectionChecks;mismatches=0;source_anchors=$i.source_anchors;delivery_receipt=$i.delivery_receipt;post_anchors=$postAnchors;controller_carry=(Entry "$r2e/controller-review/controller-receipt.json");head=$head;index_sha256=$index;git_paths=$paths.Count;git_paths_sha256=(TextSha ($paths|Sort-Object));dirty_sha256=(TextSha $dirty);png_content_audits=0}
}
function CheckState($s,$stage,$contractSha,$freezeSha){
 if(-not $s -or $s.attempt -cne $Attempt -or $s.status -cne 'READY' -or $s.next -cne $stage -or $s.contract_sha -cne $contractSha -or $s.freeze_sha -cne $freezeSha){throw 'state-blocked'}
 foreach($e in $s.receipts){$a=Entry $e.path;if($a.sha256 -cne $e.sha256 -or $a.bytes -ne $e.bytes){throw 'receipt-sha'};$v=Json $e.path;if($v.verification_exit -ne 0 -or $v.attempt -cne $Attempt -or $v.pending){throw 'receipt-unverified'};foreach($ref in $v.entries){$actual=Entry $ref.path;if($actual.sha256 -cne $ref.sha256 -or $actual.bytes -ne $ref.bytes){throw 'referenced-receipt-sha'}}}
}
