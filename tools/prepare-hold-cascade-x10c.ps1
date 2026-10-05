$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$campaignPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue'
$batchPath=Join-Path $campaignPath 'hold-cascade-x10c'
$outPath=Join-Path $repoPath 'out/x10c'
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Save($name,$value){$data=[Text.UTF8Encoding]::new($false).GetBytes(($value|ConvertTo-Json -Depth 60)+"`n");$s=[IO.File]::Open("$batchPath/$name",[IO.FileMode]::CreateNew,[IO.FileAccess]::Write);try{$s.Write($data,0,$data.Length)}finally{$s.Dispose()}}
function ReplaceExact([string]$old,[string]$new){if(($script:content.Split(@($old),[StringSplitOptions]::None)).Count -ne 2){throw "unique replacement missing: $old"};$script:content=$script:content.Replace($old,$new)}
if(Test-Path -LiteralPath $outPath){throw 'new export required'}
$bindings=@()
foreach($role in @('baseline','variant')){
  $parent=if($role -eq 'baseline'){"$repoPath/out/x1/c36h-v3"}else{"$repoPath/out/x10b/c36h-interior"}
  $p=if($role -eq 'baseline'){"$campaignPath/contact-replay-x1/source-v2/c36h-source-provenance.json"}else{"$campaignPath/hold-claim-x10b/c36h-source-provenance.json"}
  $prov=Get-Content -LiteralPath $p -Raw|ConvertFrom-Json
  foreach($e in $prov.instrumented_source_sha256.PSObject.Properties){if((Hash "$parent/$($e.Name)") -ne $e.Value){throw 'frozen parent mismatch'}}
  $export=Join-Path $outPath $role
  New-Item -ItemType Directory $export|Out-Null
  Copy-Item -LiteralPath "$parent/src","$parent/include","$parent/tests" -Destination $export -Recurse
  $script:content=[IO.File]::ReadAllText("$export/src/game.cpp")
  $before=Hash "$export/src/game.cpp"
  $script:content="#include `"x10c_diagnostics.hpp`"`n"+$script:content
  $anchor='note.recent_identity=track.id;'+"`n"
  if($role -eq 'variant'){$anchor+='        if(x10b::current_interior_claim(f,note,*line,claimed_outlines)) continue;'+"`n"}
  $anchor+='        std::erase_if(notes,[&](const NoteCandidate& incoming) {'
  $evaluation=if($role -eq 'variant'){'x10b::current_interior_claim(f,note,*line,claimed_outlines)'}else{'x10c::enabled&&x10b::current_interior_claim(f,note,*line,claimed_outlines)'}
  $skip=if($role -eq 'variant'){'        if(x10c_claim) continue;'+"`n"}else{''}
  ReplaceExact $anchor ('note.recent_identity=track.id;'+"`n"+'        const bool x10c_claim='+$evaluation+';'+"`n"+'        x10c::fallback(f,note,*line,claimed_outlines,track,notes,approaching,x10c_claim,'+$(if($role -eq 'variant'){'x10c_claim'}else{'false'})+',x1_trace_enabled);'+"`n"+$skip+'        std::erase_if(notes,[&](const NoteCandidate& incoming) {')
  [IO.File]::WriteAllText("$export/src/game.cpp",$script:content,[Text.UTF8Encoding]::new($false))
  $script:content=[IO.File]::ReadAllText("$export/include/pas/game.hpp")
  ReplaceExact 'const CandidateBatch& candidate_batch() const { return candidate_batch_; }' 'const CandidateBatch& candidate_batch() const { return candidate_batch_; } const std::vector<GameTrackHistory>& replay_x10c_tracks() const { return tracks_; }'
  [IO.File]::WriteAllText("$export/include/pas/game.hpp",$script:content,[Text.UTF8Encoding]::new($false))
  $script:content=[IO.File]::ReadAllText("$export/include/pas/game_session.hpp")
  ReplaceExact 'const CandidateBatch& replay_candidates() const { return observer_.candidate_batch(); }' 'const CandidateBatch& replay_candidates() const { return observer_.candidate_batch(); } const std::vector<GameTrackHistory>& replay_x10c_tracks() const { return observer_.replay_x10c_tracks(); }'
  [IO.File]::WriteAllText("$export/include/pas/game_session.hpp",$script:content,[Text.UTF8Encoding]::new($false))
  foreach($file in @('src/game.cpp','include/pas/game.hpp','include/pas/game_session.hpp')){$prov.instrumented_source_sha256.$file=Hash "$export/$file"}
  $prov.export_root=$export
  $prov|Add-Member -NotePropertyName x10c_role -NotePropertyValue $role
  Save "$role-provenance.json" $prov
  $bindings+=@{role=$role;parent_root=$parent;actual_export_root=$export;parent_provenance_sha256=(Hash $p);before_game_sha256=$before;after_game_sha256=(Hash "$export/src/game.cpp")}
}
$parentSource="$campaignPath/hold-claim-x10b/source/apps/frame_review/contact_replay.cpp"
$destination="$repoPath/apps/frame_review/x10c_contact_replay.cpp"
if(Test-Path -LiteralPath $destination){throw 'new reader source required'}
$script:content=[IO.File]::ReadAllText($parentSource)
$script:content="#include `"x10c_diagnostics.hpp`"`n"+$script:content
ReplaceExact 'if(manifest.at("windows").size()!=5)throw std::runtime_error("five_windows_required");' 'if(manifest.value("experiment","")=="X10c") { x10c::validate_windows(manifest); } else if(manifest.at("windows").size()!=5)throw std::runtime_error("five_windows_required");'
ReplaceExact 'Input input(argv[2]);const auto provenance=load(argv[3]);' 'Input input(argv[2]);const bool x10c_run=std::string(argv[1])=="contact-x10c"; if(x10c_run&&input.manifest.value("experiment","")!="X10c")throw std::runtime_error("x10c_manifest_required"); const auto provenance=load(argv[3]);'
ReplaceExact 'Digest digest;FakeClock clock;' 'std::ofstream x10c_digests,x10c_mechanism; std::size_t x10c_mechanism_count=0; if(x10c_run){x10c_digests.open(output/"state-digests.jsonl");x10c_mechanism.open(output/"fallback-witness.jsonl");} Digest digest;FakeClock clock;'
ReplaceExact 'x1_trace_enabled=trace&&(input.selected(i)||x2_run);' 'x1_trace_enabled=trace&&(input.selected(i)||x2_run); x10c::enabled=x10c_run&&trace;'
ReplaceExact 'auto diagnostics=x1_drain();' @'
auto diagnostics=x1_drain();
            if(x10c_run) {
                const auto bank=consume?candidate_batch_json(perception.replay_candidates()):json(nullptr);
                const auto tracks=x10c::histories(perception.replay_x10c_tracks());
                json hashes=json::object();
                for(const auto& [key,value]:std::map<std::string,json>{{"semantic",semantic},{"scene",scene},{"bank",bank},{"history",tracks},{"owner",owner_state},{"contacts",touch.contacts}}) {
                    Digest d;d.add(value);hashes[key+"_sha256"]=d.finish();
                }
                writer.row(x10c_digests,{{"ordinal",i},{"source_frame",frame},{"png_sha256",e.at("png_sha256")},{"consumed",consume},{"digests",hashes}});
                for(auto row:x10c::drain()) {
                    if(++x10c_mechanism_count>100000)throw std::runtime_error("x10c_mechanism_capacity");
                    row["ordinal"]=i;row["png_sha256"]=e.at("png_sha256");writer.row(x10c_mechanism,row);
                }
            }
'@
ReplaceExact 'trace_row["diagnostics"]=std::move(diagnostics);' @'
if(x10c_run){trace_row["observer_history"]=x10c::histories(perception.replay_x10c_tracks());trace_row["timing"]={{"original_capture_complete_ns",e.at("capture_complete_ns")},{"original_pixels_ready_ns",e.at("pixels_ready_ns")},{"replay_capture_complete_ns",capture},{"replay_pixels_ready_ns",ready},{"recognition_complete_ns",clock.now_ns()},{"recognition_duration","zero_fake_time"},{"source_render_age",nullptr}};}
                trace_row["diagnostics"]=std::move(diagnostics);
'@
ReplaceExact 'frames.close();events.close();original.close();' 'x10c::enabled=false;x10c::drain();x10c_digests.close();x10c_mechanism.close();frames.close();events.close();original.close();'
ReplaceExact 'save(output/"summary.json",summary);' 'if(x10c_run){summary["experiment"]="X10c";summary["role"]=provenance.at("x10c_role");summary["fallback_witness_rows"]=x10c_mechanism_count;summary["x10c_diagnostic_peak_bytes"]=x10c::peak;summary["state_digest_scope"]="all7722 frame semantic plus scene/bank/note-history/owner/contacts; hidden internal line-tracker state not serialized";} save(output/"summary.json",summary);'
ReplaceExact 'if(std::string(argv[1])=="contact")return replay(argc,argv);' 'if(std::string(argv[1])=="contact"||std::string(argv[1])=="contact-x10c")return replay(argc,argv);'
[IO.File]::WriteAllText($destination,$script:content,[Text.UTF8Encoding]::new($false))
$m=Get-Content -LiteralPath "$campaignPath/hold-claim-x10b/input-manifest.json" -Raw|ConvertFrom-Json
$m.batch_root=$batchPath
$m.windows+=@([pscustomobject]@{id='K2883';first=2865;last=2895;anchor=2883},[pscustomobject]@{id='C5520';first=5516;last=5524;anchor=5520})
$m|Add-Member -NotePropertyName experiment -NotePropertyValue 'X10c'
Save 'input-manifest.json' $m
Save 'patch-bindings.json' @{exports=$bindings;reader_parent_sha256=(Hash $parentSource);reader_sha256=(Hash $destination);predicate_sha256=(Hash "$repoPath/apps/frame_review/x10b_claim.hpp");diagnostics_sha256=(Hash "$repoPath/apps/frame_review/x10c_diagnostics.hpp")}
Write-Output 'X10c exports and reader prepared'
