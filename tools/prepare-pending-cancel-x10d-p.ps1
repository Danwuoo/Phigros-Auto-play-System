$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$campaignPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue'
$batchPath=Join-Path $campaignPath 'pending-cancel-x10d-p'
$outPath=Join-Path $repoPath 'out/x10d-p'
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Bytes($p){if(!(Test-Path -LiteralPath $p)){return 0};[long](Get-ChildItem -LiteralPath $p -File -Recurse|Measure-Object Length -Sum).Sum}
function Save($name,$value){[IO.File]::WriteAllText("$batchPath/$name",($value|ConvertTo-Json -Depth 60)+"`n",[Text.UTF8Encoding]::new($false))}
if((Test-Path -LiteralPath $batchPath) -or (Test-Path -LiteralPath $outPath)){throw 'new roots required'}
$existing=(Bytes $campaignPath)+(Bytes "$repoPath/measurements/research-next-20261001")
$free=(Get-PSDrive -Name C).Free
if($existing+134217728 -gt 8589934592 -or $free -lt 805306368+134217728+5368709120){throw 'capacity reserve'}
New-Item -ItemType Directory $batchPath,$outPath|Out-Null
$prior="$campaignPath/hold-cascade-x10c"
$binding=Get-Content -LiteralPath "$prior/source-binding-before-replay.json" -Raw|ConvertFrom-Json
$checked=@()
foreach($f in $binding.export_files){$p="$repoPath/out/x10c/$($f.role)/$($f.path)";if((Hash $p) -ne $f.sha256){throw "frozen export mismatch $p"};$checked+=@{path=$p;sha256=$f.sha256}}
foreach($f in $binding.binaries){$p="$repoPath/$($f.path)";if((Hash $p) -ne $f.sha256){throw "frozen binary mismatch $p"};$checked+=@{path=$p;sha256=$f.sha256}}
$baseOn=Get-Content -LiteralPath "$prior/baseline-on-1/summary.json" -Raw|ConvertFrom-Json
$baseOff=Get-Content -LiteralPath "$prior/baseline-off-1/summary.json" -Raw|ConvertFrom-Json
if($baseOn.semantic_sha256 -ne $baseOff.semantic_sha256 -or (Hash "$prior/baseline-on-1/events.jsonl") -ne (Hash "$prior/baseline-off-1/events.jsonl") -or (Hash "$prior/baseline-on-1/state-digests.jsonl") -ne (Hash "$prior/baseline-off-1/state-digests.jsonl")){throw 'reference ON/OFF bridge'}
$oldStatus=@(git status --short)
$dirtyHashes=@();foreach($line in $oldStatus){$p=$line.Substring(3);if(Test-Path -LiteralPath "$repoPath/$p" -PathType Leaf){$dirtyHashes+=@{path=$p;sha256=(Hash "$repoPath/$p")}}}
Save 'workspace-before.json' @{head=(git rev-parse HEAD);branch=(git branch --show-current);worktrees=@(git worktree list);status=$oldStatus;dirty=$true;strategy_main='observer50/planner27/diagnostics11';strategy_reference='C36h observer37/planner19 tint1; isolated instrumentation binary';existing_dirty_file_hashes=$dirtyHashes;campaign_plus_prior_bytes=$existing;disk_free_bytes=$free;limits=@{batch=134217728;out=805306368;campaign=8589934592};frozen_checked=$checked;old_source_binding_sha256=(Hash "$prior/source-binding-before-replay.json");reference_old_trace_equal=$true}
[IO.File]::Copy("$repoPath/docs/PENDING_CANCEL_X10D_P_PROTOCOL_20261003.md","$batchPath/protocol-before.md",$false)
$hook=@'
        // A newer complete snapshot with no current Note must withdraw a
        // pending Down immediately. Missing grace applies only after a
        // contact has started; it cannot license a new touch from old pixels.
        if(identity.submitted) if(const auto cursor=scheduler_.executed_steps(identity.intent);
           cursor&&*cursor==0) {
            cancel_contact(id,identity,"pending_down_current_object_missing");
            continue;
        }
'@
$hook=$hook.Replace("`r`n","`n")+"`n"
foreach($role in @('baseline','variant')){
 $export="$outPath/$role";New-Item -ItemType Directory $export|Out-Null
 Copy-Item -LiteralPath "$repoPath/out/x10c/baseline/src","$repoPath/out/x10c/baseline/include","$repoPath/out/x10c/baseline/tests" -Destination $export -Recurse
 $prov=Get-Content -LiteralPath "$prior/baseline-provenance.json" -Raw|ConvertFrom-Json
 if($role -eq 'variant'){
  $p="$export/src/game.cpp";$s=[IO.File]::ReadAllText($p).Replace("`r`n","`n")
  $anchor='        const auto grace=identity.kind==NoteKind::hold?60''000''000:identity.kind==NoteKind::flick?75''000''000:40''000''000;'
  if(($s.Split(@($anchor),[StringSplitOptions]::None)).Count -ne 2){throw 'hook anchor'}
  $s=$s.Replace($anchor,$hook+$anchor);[IO.File]::WriteAllText($p,$s,[Text.UTF8Encoding]::new($false))
  $original=[IO.File]::ReadAllText("$outPath/baseline/src/game.cpp").Replace("`r`n","`n")
  if($s.Replace($hook,'') -ne $original){throw 'inverse hook'}
  $donor=[IO.File]::ReadAllText("$repoPath/src/game.cpp").Replace("`r`n","`n")
  if(!$donor.Contains($hook.TrimEnd())){throw 'donor exact hook'}
  $prov.instrumented_source_sha256.'src/game.cpp'=Hash $p
 }
 $prov.export_root=$export;$prov.x10c_role=$role
 $prov|Add-Member -NotePropertyName x10d_p -NotePropertyValue @{suppression=$false;hook=($role -eq 'variant');parent='frozen out/x10c/baseline';donor_game_sha256=(Hash "$repoPath/src/game.cpp")}
 Save "$role-provenance.json" $prov
}
$m=Get-Content -LiteralPath "$prior/input-manifest.json" -Raw|ConvertFrom-Json
$m.batch_root=$batchPath
Save 'input-manifest.json' $m
$reader=[IO.File]::ReadAllText("$repoPath/apps/frame_review/x10c_contact_replay.cpp")
$reader=$reader.Replace('std::ofstream x10c_digests,x10c_mechanism;', 'std::ofstream x10d_lifecycle; std::size_t x10d_lifecycle_bytes=0; if(trace)x10d_lifecycle.open(output/"lifecycle.jsonl"); std::ofstream x10c_digests,x10c_mechanism;')
$anchor='            digest.add(semantic);'
if(($reader.Split(@($anchor),[StringSplitOptions]::None)).Count -ne 2){throw 'reader anchor'}
$addition=@'
            // Single-direction audit output, after owner acceptance/poll.
            // Compact full-prefix records support lifecycle joins beyond windows.
            if(trace) {
                json targets=json::array(),ids=json::array();
                if(!scene.is_null())for(const auto& t:scene.at("targets")) {
                    json compact=json::object();
                    for(const auto* k:{"note_id","kind","revision","reason","evidence_ns","expires_ns","crossing_ns","hit_x","hit_y","samples","line_id","rails_geometry","head_on_line","held_body_evidence","held_body_patch"})
                        if(t.contains(k))compact[k]=t.at(k);
                    // Flattened qualification fields; full geometry remains
                    // in selected-window traces and per-frame scene digests.
                    targets.push_back(t);
                }
                if(!owner_state.is_null())for(const auto& id:owner_state.at("identities"))ids.push_back(id);
                json row={{"ordinal",i},{"consumed",consume},{"capture_ns",capture},{"ready_ns",ready},{"acceptance_ns",x10d_accept_ns>=0?json(x10d_accept_ns):json(nullptr)},
                    {"targets",targets},{"identities",ids},{"contacts",touch.contacts}};
                const auto bytes=row.dump().size()+1;
                if(x10d_lifecycle_bytes+bytes>32*1024*1024)throw std::runtime_error("x10d_lifecycle_capacity");
                x10d_lifecycle_bytes+=bytes;writer.row(x10d_lifecycle,row);
            }
'@
# Full target records would exceed the predeclared compact budget. Preserve
# identity/qualification/timing plus contact support, no unused geometry dumps.
$addition=$addition.Replace('targets.push_back(t);','targets.push_back(compact);')
$reader=$reader.Replace($anchor,$anchor+"`n"+$addition)
$reader=$reader.Replace('json scene=nullptr,state=nullptr,candidates=nullptr;','json scene=nullptr,state=nullptr,candidates=nullptr;Nanoseconds x10d_accept_ns=-1;')
$reader=$reader.Replace('else {owner.accept(packet.scene,packet.allow_down);','else {x10d_accept_ns=clock.now_ns();owner.accept(packet.scene,packet.allow_down);')
$reader=$reader.Replace('x10c::enabled=false;x10c::drain();','x10d_lifecycle.close();x10c::enabled=false;x10c::drain();')
$reader=$reader.Replace('summary["experiment"]="X10c";', 'summary["experiment"]="X10c";summary["x10d_p"]=true;summary["suppression"]=false;summary["lifecycle_bytes"]=x10d_lifecycle_bytes;')
[IO.File]::WriteAllText("$repoPath/apps/frame_review/x10d_p_contact_replay.cpp",$reader,[Text.UTF8Encoding]::new($false))
& git diff --no-index -- "$outPath/baseline/src/game.cpp" "$outPath/variant/src/game.cpp" > "$batchPath/isolated-hook.patch"
if($LASTEXITCODE -ne 1){throw 'expected nonempty patch'}
Save 'export-audit.json' @{inverse_hook_restores_parent=$true;suppression=$false;only_candidate_change='src/game.cpp pending missing hook';reader_parent_sha256=(Hash "$repoPath/apps/frame_review/x10c_contact_replay.cpp");reader_sha256=(Hash "$repoPath/apps/frame_review/x10d_p_contact_replay.cpp");parent_provenance_sha256=(Hash "$prior/baseline-provenance.json")}
Write-Output "Prepared X10d-P; campaign=$existing disk=$free"
