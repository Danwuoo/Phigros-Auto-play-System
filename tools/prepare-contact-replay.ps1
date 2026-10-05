param([ValidateSet('c36h','main50')][string]$Lineage,[string]$ExportRoot,[string]$EvidenceRoot)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path "$PSScriptRoot/..").Path
if(!(Split-Path $ExportRoot -IsAbsolute)){ $ExportRoot=Join-Path $repo $ExportRoot }
if(!(Split-Path $EvidenceRoot -IsAbsolute)){ $EvidenceRoot=Join-Path $repo $EvidenceRoot }
if(Test-Path -LiteralPath $ExportRoot){throw 'Export must be new; preserve previous attempts'}
if(Test-Path -LiteralPath (Join-Path $EvidenceRoot "$Lineage-source-provenance.json")){throw 'Provenance output must be new; preserve previous attempts'}
$commit=if($Lineage -eq 'c36h'){'98169a575bd7c3b7503849ceb4b91a934d50ce0f'}else{'f83c7ea0bd4e92ab6fb50a14fc9db2c4e333be2c'}
$freezeRoot=Join-Path $repo 'measurements/game-assist/2026-09-30-m0-manual-continue/acceptance36h-01'
$freeze=Get-Content (Join-Path $freezeRoot 'candidate36h-tint1-freeze.json') -Raw | ConvertFrom-Json
New-Item -ItemType Directory $ExportRoot,$EvidenceRoot -Force | Out-Null
$archive=Join-Path $ExportRoot 'source.tar'
git -C $repo archive --format=tar "--output=$archive" $commit
if($LASTEXITCODE){throw 'git archive failed'}
tar -xf $archive -C $ExportRoot
if($LASTEXITCODE){throw 'tar failed'}
$checks=@()
if($Lineage -eq 'c36h'){
  foreach($entry in $freeze.source_sha256.PSObject.Properties){
    $snapshot=Join-Path (Join-Path $freezeRoot 'candidate36h-tint1-source') $entry.Name
    if(!(Test-Path -LiteralPath $snapshot)){throw "Missing frozen source: $snapshot"}
    $hash=(Get-FileHash -LiteralPath $snapshot -Algorithm SHA256).Hash.ToLower()
    if($hash -ne $entry.Value){throw "Frozen source SHA mismatch: $snapshot"}
    $dest=Join-Path $ExportRoot $entry.Name
    if($entry.Name -notlike 'out/*'){
      $gitText=[IO.File]::ReadAllText($dest).Replace("`r`n","`n")
      $frozenText=[IO.File]::ReadAllText($snapshot).Replace("`r`n","`n")
      $equal=$gitText -ceq $frozenText
      if(!$equal -and ($entry.Name -like 'src/*' -or $entry.Name -like 'include/*' -or $entry.Name -like 'apps/*' -or $entry.Name -like 'tests/*')){throw "Saved commit differs beyond line endings: $($entry.Name)"}
      Copy-Item -LiteralPath $snapshot -Destination $dest
    }
    $checks+=@{path=$entry.Name;frozen_sha256=$hash;git_normalized_content_equal=if($entry.Name -like 'out/*'){$null}else{$equal}}
  }
}
$before=@{}; foreach($p in Get-ChildItem "$ExportRoot/src","$ExportRoot/include" -File -Recurse){$before[$p.FullName.Substring($ExportRoot.Length+1).Replace('\','/')]=(Get-FileHash $p.FullName).Hash.ToLower()}
$script:patches=@()
function Edit-Source([string]$path,[scriptblock]$edit){
  $absolute=Join-Path $ExportRoot $path
  $script:content=[IO.File]::ReadAllText($absolute).Replace("`r`n","`n")
  & $edit
  [IO.File]::WriteAllText($absolute,$script:content,[Text.UTF8Encoding]::new($false))
  $script:patches+=@{path=$path;before_sha256=$before[$path];after_sha256=(Get-FileHash $absolute).Hash.ToLower()}
}
function Replace-Exact([string]$old,[string]$new){
  if(!$script:content.Contains($old)){throw "Missing diagnostic insertion anchor: $old"}
  $script:content=$script:content.Replace($old,$new)
}
Edit-Source 'include/pas/game_session.hpp' {
  Replace-Exact 'SessionObservation process(const Frame&);' 'SessionObservation process(const Frame&); const CandidateBatch& replay_candidates() const { return observer_.candidate_batch(); }'
}
Edit-Source 'include/pas/game.hpp' {
  Replace-Exact 'std::vector<nlohmann::json> take_plan_cancellations();' @'
std::vector<nlohmann::json> take_plan_cancellations();
    nlohmann::json replay_state() const {
        nlohmann::json ids=nlohmann::json::array(),aliases=nlohmann::json::array();
        for(const auto& [note,id]:identities_) {
            const auto cursor=scheduler_.executed_steps(id.intent);
            ids.push_back({{"note_id",note},{"intent_id",id.intent},{"submitted",id.submitted},
                {"kind",name(id.kind)},{"revision",id.revision},{"expires_ns",id.expires},
                {"cursor",cursor?nlohmann::json(*cursor):nlohmann::json(nullptr)},
                {"prefix_offset",id.plan.prefix_offset},{"evidence_ns",id.plan.evidence_ns},
                {"valid_until_ns",id.plan.valid_until_ns},{"line_id",id.line_id},
                {"x",id.last_note.center.x},{"y",id.last_note.center.y},
                {"tail_release_ns",id.hold_tail_release_ns?nlohmann::json(*id.hold_tail_release_ns):nlohmann::json(nullptr)}});
        }
        for(const auto& [note,alias]:contact_aliases_)aliases.push_back({{"candidate_note_id",note},{"owner_note_id",alias.owner},{"last_seen_ns",alias.last_seen_ns}});
        return {{"identities",ids},{"aliases",aliases},{"last_rejection",last_rejection_}};
    }
'@
}
Edit-Source 'src/game.cpp' {
  $script:content="#include `"replay_trace.hpp`"`n"+$script:content
  Replace-Exact 'void GameObserver::reset() {' 'void GameObserver::reset() { if(x1_trace_enabled)x1_emit({{"event","observer_reset"},{"site","explicit_or_context_reset"}});'
  Replace-Exact 'const auto compatible_body=[](const Identity& id,const GameTarget& t) {' 'const auto compatible_body=[](const Identity& id,const GameTarget& t) {'
  # Observe the original predicates, without changing the expression or branch order.
  $anchor='std::set<std::uint64_t> claimed;'
  Replace-Exact $anchor @'
if(x1_trace_enabled)for(const auto& target:s.targets) {
        nlohmann::json comparisons=nlohmann::json::array();
        for(const auto& [key,id]:identities_)if(id.kind==NoteKind::hold)
            comparisons.push_back({{"owner_note_id",key},{"compatible",compatible_body(id,target)},
                {"intent_id",id.intent}});
        x1_emit({{"event","owner_current_support"},{"note_id",target.note_id},
            {"held_support",held_support(target)},{"body_compatibility",comparisons}});
    }
    std::set<std::uint64_t> claimed;
'@
}
Edit-Source 'src/game_tracking.cpp' {
  $script:content="#include `"replay_trace.hpp`"`n"+$script:content
  Replace-Exact 'const auto prior_note_center=match->last;' @'
if(x1_trace_enabled)x1_emit({{"event","candidate_track"},{"candidate_id",ni+1},{"note_id",match->id},
            {"assigned_prior_index",assigned[ni]},{"identity_ambiguous",ambiguous},
            {"prior_revision",match->revision},{"prior_observed_ns",match->observed},
            {"prior_x",match->last.x},{"prior_y",match->last.y},{"prior_samples",match->points.size()}});
        const auto prior_note_center=match->last;
'@
  Replace-Exact 'std::vector<int> assigned=forced_assignment?*forced_assignment:std::vector<int>(notes.size(),-1);' @'
if(x1_trace_enabled)for(std::size_t candidate=0;candidate<notes.size();++candidate) {
        nlohmann::json alternatives=nlohmann::json::array();std::size_t feasible=0;
        for(const auto& pair:pairs)if(pair.note==candidate) {
            ++feasible;if(alternatives.size()<16)alternatives.push_back({{"prior_note_id",tracks[pair.track].id},{"cost",pair.cost}});
        }
        x1_emit({{"event","identity_alternatives"},{"candidate_id",candidate+1},{"feasible_edges",feasible},
            {"best_sixteen_edges",alternatives},{"edges_beyond_diagnostic_sixteen",feasible>16?feasible-16:0}});
    }
    std::vector<int> assigned=forced_assignment?*forced_assignment:std::vector<int>(notes.size(),-1);
'@
  # Each clear is tagged with its instrumented source site and the prior measured count.
  Replace-Exact 'match->points.clear();' 'if(x1_trace_enabled)x1_emit({{"event","history_reset"},{"note_id",match->id},{"source_site",__LINE__},{"prior_samples",match->points.size()}});match->points.clear();'
  # Braces preserve single-statement else semantics above.
  $script:content=$script:content.Replace('else if(x1_trace_enabled)x1_emit({{"event","history_reset"},{"note_id",match->id},{"source_site",__LINE__},{"prior_samples",match->points.size()}});match->points.clear();','else {if(x1_trace_enabled)x1_emit({{"event","history_reset"},{"note_id",match->id},{"source_site",__LINE__},{"prior_samples",match->points.size()}});match->points.clear();}')
  if($Lineage -eq 'main50'){
    Replace-Exact 'if(!line.association_valid)continue;' 'if(!line.association_valid){if(x1_trace_enabled)x1_emit({{"event","relation_gate"},{"note_id",match->id},{"line_id",line.track_id},{"gate","association_invalid"}});continue;}'
    Replace-Exact 'if(line.length<out.context.width*.24)continue;' 'if(line.length<out.context.width*.24){if(x1_trace_enabled)x1_emit({{"event","relation_gate"},{"note_id",match->id},{"line_id",line.track_id},{"gate","length"}});continue;}'
    Replace-Exact 'if(along>line.length*.5+n.width+24)continue;' 'if(along>line.length*.5+n.width+24){if(x1_trace_enabled)x1_emit({{"event","relation_gate"},{"note_id",match->id},{"line_id",line.track_id},{"gate","extent"},{"along",along}});continue;}'
    Replace-Exact 'if(score<best_score) {' @'
if(x1_trace_enabled)x1_emit({{"event","relation_score"},{"note_id",match->id},{"line_id",line.track_id},
                {"distance_cost",across},{"extent_cost",std::max(0.0,along-line.length*.5)*2},
                {"orientation_cost",(1-alignment)*orientation_weight},{"confidence_cost",(1-line.confidence)*4},
                {"trend_cost",relative_trend},{"recent_cost",same_recent?-120:0},{"score",score}});
            if(score<best_score) {
'@
    Replace-Exact 'const bool relation_ambiguous=!preserve_confirmed&&second_score-best_score<8;' 'if(x1_trace_enabled)x1_emit({{"event","relation_selection"},{"note_id",match->id},{"winner",selected?selected->track_id:0},{"confirmed_line_id",match->confirmed_line_id},{"preserve",preserve_confirmed},{"best_score",best_score},{"second_score",second_score}}); const bool relation_ambiguous=!preserve_confirmed&&second_score-best_score<8;'
  }else{
    Replace-Exact 'if(!line.association_valid||line.length<out.context.width*.32)continue;' 'if(!line.association_valid||line.length<out.context.width*.32){if(x1_trace_enabled)x1_emit({{"event","relation_gate"},{"note_id",match->id},{"line_id",line.track_id},{"gate",!line.association_valid?"association_invalid":"length"}});continue;}'
    Replace-Exact 'selected=&line;preserved=true;break;' 'if(x1_trace_enabled)x1_emit({{"event","relation_preserve"},{"note_id",match->id},{"line_id",line.track_id},{"current_body",current_body},{"first_distance",first},{"last_distance",last},{"current_distance",current_d}});selected=&line;preserved=true;break;'
    Replace-Exact 'if(!normal_approach&&alignment<.95)continue;' 'if(!normal_approach&&alignment<.95){if(x1_trace_enabled)x1_emit({{"event","relation_gate"},{"note_id",match->id},{"line_id",line.track_id},{"gate","role_alignment"},{"alignment",alignment},{"normal_approach",normal_approach}});continue;}'
    Replace-Exact 'if(role_class<best_class)' 'if(x1_trace_enabled)x1_emit({{"event","relation_score"},{"note_id",match->id},{"line_id",line.track_id},{"role_class",role_class},{"normal_approach",normal_approach},{"alignment",alignment},{"distance_cost",std::abs(current_d)},{"confidence_cost",(1-line.confidence)*4},{"score",score}});if(role_class<best_class)'
    Replace-Exact 'if(selected&&!ambiguous) {' 'if(x1_trace_enabled)x1_emit({{"event","relation_selection"},{"note_id",match->id},{"winner",selected?selected->track_id:0},{"preserve",preserved},{"best_score",best_score},{"second_score",second_score}}); if(selected&&!ambiguous) {'
  }
  Replace-Exact 'out.targets.push_back(std::move(target));' 'if(x1_trace_enabled)x1_emit({{"event","relation_final"},{"note_id",target.note_id},{"winner",target.line_id},{"samples",target.samples},{"reason",target.reason},{"history_span_ns",target.history_span_ns}});out.targets.push_back(std::move(target));'
}
Edit-Source 'src/core.cpp' {
  $script:content="#include `"replay_trace.hpp`"`n"+$script:content
  Replace-Exact 'void ContactScheduler::cancel(const std::string&) {' 'void ContactScheduler::cancel(const std::string& reason) {if(x1_trace_enabled)x1_emit({{"event","scheduler_cancel"},{"reason",reason},{"now_ns",clock_.now_ns()},{"pending",pending_.size()},{"active",active_count()}});'
}
$compiled=@{};foreach($p in Get-ChildItem "$ExportRoot/src","$ExportRoot/include" -File -Recurse){$compiled[$p.FullName.Substring($ExportRoot.Length+1).Replace('\','/')]=(Get-FileHash $p.FullName).Hash.ToLower()}
$gameTests=[IO.File]::ReadAllText((Join-Path $ExportRoot 'tests/game_tests.cpp')).Replace("`r`n","`n")
$excludedTests=[regex]::Matches($gameTests,'(?m)^TEST\(GameRuntime, ([^)]+)\)') | ForEach-Object {$_.Groups[1].Value}
$gameTests=$gameTests.Replace('#include "pas/runtime.hpp"','')
$gameTests=[regex]::Replace($gameTests,'(?ms)^TEST\(GameRuntime,.*?(?=^TEST\(|\z)','')
[IO.File]::WriteAllText((Join-Path $ExportRoot 'tests/game_tests_offline.cpp'),$gameTests,[Text.UTF8Encoding]::new($false))
$provenance=@{lineage=$Lineage;saved_commit=$commit;freeze_commit=if($Lineage -eq 'c36h'){$freeze.commit}else{$commit};freeze_dirty=($Lineage -eq 'c36h');frozen_binary_sha256=if($Lineage -eq 'c36h'){$freeze.binary_sha256}else{$null};snapshot_checks=$checks;base_source_sha256=$before;instrumented_source_sha256=$compiled;diagnostic_patches=$script:patches;export_root=$ExportRoot;production_checkout_modified=$false;offline_tests_excluded_transport_cases=$excludedTests}
$provenance | ConvertTo-Json -Depth 15 | Set-Content -LiteralPath (Join-Path $EvidenceRoot "$Lineage-source-provenance.json") -Encoding utf8
Write-Output "$Lineage verified and instrumented at $ExportRoot"
