. "$PSScriptRoot/r3-common.ps1"
if(Test-Path "$batchPath/source-binding-before-cost.json"){throw 'frozen'}
function ReplaceOnce([string]$s,[string]$a,[string]$b) {if(!$s.Contains($a)){throw "source anchor missing $a"};$s.Replace($a,$b)}
$app="$repoPath/apps/runtime_x11_p_r3"
$s=[IO.File]::ReadAllText("$repoPath/apps/runtime_x11_p_r1/main.cpp").Replace("`r`n","`n")
$s=ReplaceOnce $s '#include "meter_touch.hpp"' '#include "meter_touch.hpp"'
$s=ReplaceOnce $s '#include "archive_verify.hpp"' '#include "archive_verify.hpp"
#include "contract.hpp"'
$s=ReplaceOnce $s 'using r1::MeterTouch;' 'using r3::MeterTouch;'
$s=ReplaceOnce $s 'frames=stress?64:workload=="rgb"?256:128' 'frames=stress?64:2560'
$s=ReplaceOnce $s 'offline-X11-P-R1' 'offline-X11-P-R3'
$s=ReplaceOnce $s 'enqueue_cost.reserve(16384);' 'enqueue_cost.reserve(r3::events);std::vector<std::size_t> event_attempts,release_attempts;event_attempts.reserve(r3::events);release_attempts.reserve(r3::releases);std::size_t current_attempt=1;'
$s=ReplaceOnce $s 'const auto bytes=j.dump().size()+96;' 'auto envelope=j;envelope["schema_version"]=2;envelope["clock_domain"]="host_qpc_ns";envelope["round_id"]=1;const auto bytes=envelope.dump().size()+1;'
$s=ReplaceOnce $s 'journal_bytes+bytes>static_cast<std::uint64_t>(stress?6:3)*1024*1024||enqueue_cost.size()>=16384' 'journal_bytes+bytes>r3::journal_bytes||enqueue_cost.size()>=r3::events'
$s=ReplaceOnce $s '++journal_attempts;journal_bytes+=bytes;' '++journal_attempts;journal_bytes+=bytes;event_attempts.push_back(current_attempt);'
$s=ReplaceOnce $s 'j["call_index"]=releases_seen;enqueue' 'j["call_index"]=releases_seen;j["offline_attempt"]=current_attempt;release_attempts.push_back(current_attempt);enqueue'
$s=ReplaceOnce $s 'if(s){seen=s->sequence;auto& x' 'if(s){seen=s->sequence;current_attempt=seen;auto& x'
$s=ReplaceOnce $s 'std::ofstream raw(out/"frames.jsonl");' 'r3::Windows windows;std::ofstream raw(out/"frames.jsonl",std::ios::binary);'
$s=ReplaceOnce $s 'auto row=json{{"attempt",i}' 'auto frame_row=json{{"attempt",i}'
$s=ReplaceOnce $s '{"targets",x.targets},{"lines",x.lines}}.dump();' '{"targets",x.targets},{"lines",x.lines}};windows.frame(frame_row);auto row=frame_row.dump();'
$s=ReplaceOnce $s 'std::ofstream receipts(out/"receipts.jsonl");' 'std::ofstream receipts(out/"receipts.jsonl",std::ios::binary);'
$s=ReplaceOnce $s 'auto row=receipt_json(r,false).dump();' 'auto receipt_row=receipt_json(r,false);windows.receipt(receipt_row,samples.at(seq).capture);auto row=receipt_row.dump();'
$s=ReplaceOnce $s 'std::ofstream releases(out/"releases.jsonl");' 'std::ofstream releases(out/"releases.jsonl",std::ios::binary);'
$s=ReplaceOnce $s 'j["call_index"]=i;auto row=j.dump();' 'j["call_index"]=i;j["offline_attempt"]=release_attempts.at(i);windows.release(j);auto row=j.dump();'
$s=ReplaceOnce $s 'const bool complete=raw_ok&&r1::verify_archive' 'std::ofstream timings(out/"event-timings.jsonl",std::ios::binary);
 for(std::size_t i=0;i<enqueue_cost.size();++i){json row={{"event_index",i},{"attempt",event_attempts.at(i)},{"event_enqueue",enqueue_cost.at(i)},{"writer_serialize",i<stats.serialize_ms.size()?json(stats.serialize_ms[i]):json(nullptr)},{"writer_write",i<stats.write_ms.size()?json(stats.write_ms[i]):json(nullptr)}};auto text=row.dump();if(text.size()>254)throw std::runtime_error("timing row capacity");timings<<text<<\x27\n\x27;if(i<stats.serialize_ms.size())windows.timing(row);}
 timings.flush();raw_ok=raw_ok&&bool(timings);timings.close();
 const bool complete=stats.serialize_ms.size()==enqueue_cost.size()&&stats.write_ms.size()==enqueue_cost.size()&&stats.admitted==journal_attempts+1&&raw_ok&&r3::verify_archive'
# Quoting for the generated C++ character literal.
$s=$s.Replace('\x27',"'")
$s=ReplaceOnce $s '{"receipt_capacity",512},{"release_capacity",512},{"archive_sample_capacity",16384}' '{"receipt_capacity",r3::receipts},{"release_capacity",r3::releases},{"archive_sample_capacity",r3::events},{"event_capacity",r3::events},{"journal_admission_limit",r3::journal_bytes},{"run_physical_reserve",r3::run_bytes}'
$s=ReplaceOnce $s '{"adapter_contract","same v1 stimulus; normal owner cap32/stress128; live lead35"}' '{"adapter_contract","unchanged R1 v1 RGB16/owner32 stimulus;256warmup+2304measurement;lead35"},{"windows",windows.report()},{"captured_attempts",std::count_if(samples.begin()+1,samples.end(),[](const auto& x){return x.capture>0;})}'
$s=ReplaceOnce $s 'save(out/"summary.json",result);return result;' 'result["normal_gate"]=stress?json(nullptr):json(r3::normal(result));save(out/"summary.json",result);return result;'
$s=ReplaceOnce $s 'return r.at("hard_gate").get<bool>()?0:2;' 'return (load=="normal"?r.at("normal_gate"):r.at("hard_gate")).get<bool>()?0:2;'
[IO.File]::WriteAllText("$app/main.cpp",$s,[Text.UTF8Encoding]::new($false))
$s=[IO.File]::ReadAllText("$repoPath/apps/runtime_x11_p_r1/meter_touch.hpp").Replace('namespace r1 {','namespace r3 {').Replace('512','8192')
[IO.File]::WriteAllText("$app/meter_touch.hpp",$s,[Text.UTF8Encoding]::new($false))
$s=[IO.File]::ReadAllText("$repoPath/apps/runtime_x11_p_r1/archive_verify.hpp").Replace('namespace r1 {','namespace r3 {').Replace('16384','65536')
[IO.File]::WriteAllText("$app/archive_verify.hpp",$s,[Text.UTF8Encoding]::new($false))
$s=[IO.File]::ReadAllText("$repoPath/out/x11-p-r1/B0/src/session_archive.cpp").Replace('16384','65536')
[IO.File]::WriteAllText("$app/session_archive.cpp",$s,[Text.UTF8Encoding]::new($false))
$s=[IO.File]::ReadAllText("$repoPath/apps/runtime_x11_p_r1/gate.cpp").Replace('#include "gate_contract.hpp"','#include "contract.hpp"').Replace('using r1::normal;','using r3::normal;')
$s=$s.Replace('runs[i]["metrics_ms"]','runs[i]["windows"]["measurement"]["metrics_ms"]').Replace('runs[i+1]["metrics_ms"]','runs[i+1]["windows"]["measurement"]["metrics_ms"]')
foreach($i in @('0','1','2','3')){$s=$s.Replace("r[$i][`"metrics_ms`"]","r[$i][`"windows`"] [`"measurement`"] [`"metrics_ms`"]")}
[IO.File]::WriteAllText("$app/gate.cpp",$s,[Text.UTF8Encoding]::new($false))
Write-Output 'generated isolated R3 sources from R1; originals untouched'
