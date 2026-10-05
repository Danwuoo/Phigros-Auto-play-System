$ErrorActionPreference='Stop'
$script:Repo='C:\Users\wurre\Desktop\Phigros-Auto-play-System'
$script:Attempt='bvi-r2e-20261005-01'
$script:Source=Join-Path $Repo 'research/x10d_o_bvi_r2e'
$script:Campaign=Join-Path $Repo 'measurements/game-assist/2026-09-30-m0-manual-continue'
$script:Evidence=Join-Path $Campaign 'hold-ownership-x10d-o-bvi-r2e'
$script:Out=Join-Path $Repo 'out/x10d-o-bvi-r2e'
function Json($p){[IO.File]::ReadAllText($p)|ConvertFrom-Json -Depth 100}
function Sha($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Entry($p){$f=Get-Item -LiteralPath $p;@{path=$f.FullName;bytes=[long]$f.Length;sha256=(Sha $p)}}
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
function Capacity([long]$reserveDevelopment=0,[long]$reserveOut=0){
 $batch=Bytes $Evidence;$external=Bytes $Source
 foreach($leaf in @('HOLD_OWNERSHIP_X10D_O_BVI_R2E_RESULT_20261005.md','HOLD_OWNERSHIP_X10D_O_BVI_R2E_HANDOFF_20261005.md')){$p=Join-Path "$Repo/docs" $leaf;if(Test-Path $p){$external+=(Get-Item $p).Length}}
 $newOut=Bytes $Out;$development=$batch+$external;$aggregate=8283855884+$development+$newOut;$free=(Get-PSDrive C).Free
 if($development+$reserveDevelopment -gt 41943040 -or 6974627+$development+$reserveDevelopment -gt 58720256 -or $newOut+$reserveOut -gt 134217728 -or 374705+$newOut+$reserveOut -gt 268435456 -or $aggregate+$reserveDevelopment+$reserveOut+7120408 -gt 8589934592 -or $free -lt 5704253440){throw 'capacity'}
 @{batch=$batch;external=$external;development_new=$development;development_total=6974627+$development;out_new=$newOut;out_total=374705+$newOut;aggregate=$aggregate;aggregate_remaining=8589934592-$aggregate;controller_reserved=7120408;free=$free}
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
 $head=(& git rev-parse HEAD).Trim();$index=Sha "$Repo/.git/index"
 $paths=@(& git -c core.quotepath=false ls-files --cached --others --exclude-standard|Where-Object{$_ -notlike 'research/x10d_o_bvi_r2e/*' -and $_ -notlike 'docs/HOLD_OWNERSHIP_X10D_O_BVI_R2E_RESULT_*' -and $_ -notlike 'docs/HOLD_OWNERSHIP_X10D_O_BVI_R2E_HANDOFF_*'})
 $dirty=@(& git -c core.quotepath=false status --porcelain=v1 --untracked-files=all|Where-Object{$p=$_.Substring(3);$p -notlike 'research/x10d_o_bvi_r2e/*' -and $p -notlike 'docs/HOLD_OWNERSHIP_X10D_O_BVI_R2E_RESULT_*' -and $p -notlike 'docs/HOLD_OWNERSHIP_X10D_O_BVI_R2E_HANDOFF_*'})
 if($head -cne $dispatch.head -or $index -cne $dispatch.index_sha256 -or $paths.Count -ne 507 -or @(& git diff HEAD --name-only -- src include CMakeLists.txt).Count){throw 'git-state'}
 if(Test-Path "$Evidence/protection-before.json"){$before=Json "$Evidence/protection-before.json";if((TextSha ($paths|Sort-Object)) -cne $before.git_paths_sha256 -or (TextSha $dirty) -cne $before.dirty_sha256){throw 'dirty-change'}}
 @{schema=1;reconstructed=$reconstructed;unique_files=$cache.Count;references=$script:protectionChecks;mismatches=0;source_anchors=$i.source_anchors;delivery_receipt=$i.delivery_receipt;post_anchors=$postAnchors;head=$head;index_sha256=$index;git_paths=$paths.Count;git_paths_sha256=(TextSha ($paths|Sort-Object));dirty_sha256=(TextSha $dirty);png_content_audits=0}
}
function CheckState($s,$stage,$contractSha,$freezeSha){
 if(-not $s -or $s.attempt -cne $Attempt -or $s.status -cne 'READY' -or $s.next -cne $stage -or $s.contract_sha -cne $contractSha -or $s.freeze_sha -cne $freezeSha){throw 'state-blocked'}
 foreach($e in $s.receipts){$a=Entry $e.path;if($a.sha256 -cne $e.sha256 -or $a.bytes -ne $e.bytes){throw 'receipt-sha'};$v=Json $e.path;if($v.verification_exit -ne 0 -or $v.attempt -cne $Attempt -or $v.pending){throw 'receipt-unverified'};foreach($ref in $v.entries){$actual=Entry $ref.path;if($actual.sha256 -cne $ref.sha256 -or $actual.bytes -ne $ref.bytes){throw 'referenced-receipt-sha'}}}
}
