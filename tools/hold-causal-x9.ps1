param([ValidateSet('Prepare','Replay')][string]$Mode,[ValidateSet('c36h','main50')][string]$Lineage='c36h')
$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$campaignPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue'
$parentPath=Join-Path $campaignPath 'contact-replay-x1'
$batchPath=Join-Path $campaignPath 'hold-causal-x9'
function Hash($path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
function Bytes($path){if(!(Test-Path -LiteralPath $path)){return 0};[long](Get-ChildItem -LiteralPath $path -File -Recurse|Measure-Object Length -Sum).Sum}
function Save($name,$value){$s=($value|ConvertTo-Json -Depth 40)+"`n";$data=[Text.UTF8Encoding]::new($false).GetBytes($s);$f=[IO.File]::Open("$batchPath/$name",[IO.FileMode]::CreateNew,[IO.FileAccess]::Write);try{$f.Write($data,0,$data.Length)}finally{$f.Dispose()}}
$freeze=Get-Content "$parentPath/repair-r1-r2-20261002/tool-freeze.json" -Raw|ConvertFrom-Json
if((Hash "$parentPath/repair-r1-r2-20261002/tool-freeze.json") -ne '74856111618b5018efb5f734220c27c4163a2c2597e054a90441359d00d163b4'){throw 'parent freeze changed'}
foreach($p in $freeze.binaries){if((Hash $p.path) -ne $p.sha256){throw "frozen binary changed $($p.path)"}}
if($Mode -eq 'Prepare'){
    if(Test-Path -LiteralPath $batchPath){throw 'new batch required'}
    if((Bytes $campaignPath)+(Bytes "$repoPath/measurements/research-next-20261001")+134217728 -gt 8589934592){throw 'X9 full batch reserve'}
    New-Item -ItemType Directory $batchPath|Out-Null
    $manifest=Get-Content "$parentPath/input-manifest.json" -Raw|ConvertFrom-Json
    $manifest.batch_root=$batchPath
    $manifest.windows=@([ordered]@{id='H2479';first=2460;last=2495;anchor=2479})+@($manifest.windows|Where-Object {$_.id -ne 'D6214'})
    Save 'input-manifest.json' $manifest
    Save 'protocol-before.json' ([ordered]@{
        question='Does original C36g clip02 Hold identity cancellation around2479 persist in C36h full contact replay, and at which layer first?'
        hypotheses=@('H1 visible body lost in current extraction','H2 same visible Hold split into competing runtime identities','H3 current relation loses support despite correct line','H4 owner rejects supported continuation','H0 C36h already maintains contact: no failure to fix')
        source_policy='fixed original recording; original actions external comparison only; same owner cadence, origin, zero-recognition/RPC success receipts, frame-first; full prefix and32preroll'
        intervention='diagnostic window selection only; frozen strategy binaries reused, no source/binary changes'
        controls=@('A3498 visible-body no-root sustain','B4986 body absence','C5520 Flick observation','E5287 user normal')
        falsifier='If C36h contact stays supported, reject historical cancellation as current C36h failure; no synthetic weakening of guard or invented missing current support'
        first_boundary_policy='read candidate bank, identity assignment, line relation, root reason, owner support/alias and successful Down prefix; ID is local join only'
        limits=@{batch_bytes=134217728;campaign_plus_prior_bytes=8589934592;new_replays_max=2;new_build=0;new_training=0;new_live=0}
        parent_manifest_sha256=(Hash "$parentPath/input-manifest.json");manifest_sha256=(Hash "$batchPath/input-manifest.json");binaries=$freeze.binaries
    })
    Write-Output 'Prepared X9: H2460-2495 plus four retained controls; diagnostic-only new windows'
}else{
    $suffix=if($Lineage -eq 'c36h'){'36'}else{'50'};$exe="$repoPath/out/x1/repair-tools$suffix/pas_frame_review.exe"
    $provenance="$parentPath/source-v2/$Lineage-source-provenance.json"
    $expected=(Get-Content "$parentPath/$Lineage-final-on-1/summary.json" -Raw|ConvertFrom-Json).source_provenance_sha256
    if((Hash $provenance) -ne $expected){throw 'source provenance changed'}
    $protocol=Get-Content "$batchPath/protocol-before.json" -Raw|ConvertFrom-Json
    if((Hash "$batchPath/input-manifest.json") -ne $protocol.manifest_sha256){throw 'manifest changed'}
    if(Test-Path "$batchPath/$Lineage-command.json"){throw 'run already attempted'}
    $arguments=@('contact',"$batchPath/input-manifest.json",$provenance,"$batchPath/$Lineage-on-1",'on','owner','frame-first')
    Save "$Lineage-command.json" @{exe=$exe;binary_sha256=(Hash $exe);arguments=$arguments;source_provenance_sha256=(Hash $provenance);input_manifest_sha256=$protocol.manifest_sha256}
    & $exe @arguments *> "$batchPath/$Lineage-on-1.log"
    $exit=$LASTEXITCODE
    Save "$Lineage-exit.json" @{exit=$exit;log_sha256=(Hash "$batchPath/$Lineage-on-1.log");batch_bytes=(Bytes $batchPath)}
    if($exit -ne 0){throw 'replay failed; preserved'}
    $s=Get-Content "$batchPath/$Lineage-on-1/summary.json" -Raw|ConvertFrom-Json
    $old=Get-Content "$parentPath/$Lineage-final-on-1/summary.json" -Raw|ConvertFrom-Json
    if(!$s.success -or $s.contacts_at_exit -ne 0 -or $s.verified_pngs -ne 7722 -or $s.window_frames -ne 82){throw 'incomplete replay'}
    # Exact full-prefix semantics must be unchanged by output window selection.
    if($s.semantic_sha256 -ne $old.semantic_sha256 -or (Hash "$batchPath/$Lineage-on-1/events.jsonl") -ne (Hash "$parentPath/$Lineage-final-on-1/events.jsonl")){throw 'diagnostic window semantic mismatch'}
    Save "$Lineage-semantic-check.json" @{full_prefix_digest_equal=$true;events_byte_identical=$true;old_summary_sha256=(Hash "$parentPath/$Lineage-final-on-1/summary.json");new_summary_sha256=(Hash "$batchPath/$Lineage-on-1/summary.json");trace_sha256=(Hash "$batchPath/$Lineage-on-1/trace.jsonl");new_build=0;new_replays=1}
    Write-Output "${Lineage}: 7722 PNGs,7715 perception,82 trace frames; full semantics and events equal frozen run"
}
