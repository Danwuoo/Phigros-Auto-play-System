. "$PSScriptRoot/common.ps1"
. "$Source/transaction.ps1"
if(Test-Path "$Evidence/final-receipt.json"){throw 'seal-once'}
$s=Json "$Evidence/state.json"
if($s.status -cne 'STOP' -or $s.native_stages_consumed -ne 0 -or $s.native_launches_known -ne 0 -or $s.diagnostic_slots_consumed -ne 0){throw 'seal-stop-facts'}
if(Test-Path $Out){throw 'unexpected-out'}
$null=Capacity 2097152 0
foreach($leaf in @('bvi.cpp','bvi.hpp','owned.cs','identity.ps1','transaction.ps1','driver.inc')){if((Sha "$Source/$leaf") -cne (Sha "$Repo/research/x10d_o_bvi_r2f_control/$leaf")){throw "unchanged-core:$leaf"}}
foreach($path in @("$Evidence/transaction-round-1.json","$Evidence/control-round-1.json","$Evidence/diagnostic-pretest.json")){foreach($e in (Json $path).shared_source){CheckEntry $e}}
$p=Protection
$scope=Entry "$Evidence/scope.md"
$script:fullProtectionCache[$scope.path]=$scope
$p.old_unique=$p.unique_files;$p.old_references=$p.references;$p.unique_files=$script:fullProtectionCache.Count;$p.references++;$p.current_scope=$scope
if($p.old_unique -ne 3609 -or $p.old_references -ne 4010 -or $p.unique_files -ne 3610 -or $p.references -ne 4011){throw 'precise-protection-denominator'}
$protected=@($script:fullProtectionCache.Values|Sort-Object path)
$pshards=@();for($i=0;$i -lt $protected.Count;$i+=64){$end=[Math]::Min($i+63,$protected.Count-1);$path="$Evidence/protected-$($pshards.Count).json";NewJson $path @{files=@($protected[$i..$end])};$pshards+=Entry $path}
NewJson "$Evidence/protected-index.json" @{unique_files=$protected.Count;references=$p.references;shards=$pshards;old_unique=3609;old_references=4010;mismatches=0;old_set_and_current_scope=$true;dedup='absolute path OrdinalIgnoreCase; checked bytes and SHA before seal'}
NewJson "$Evidence/protection-final.json" $p
$files=@(Get-ChildItem -LiteralPath $Source -Recurse -File|Sort-Object FullName|ForEach-Object{Entry $_.FullName})
NewJson "$Evidence/source-manifest.json" @{files=$files;runner_frozen=$false;product_frozen=$false;formal_contract_created=$false;immutable_copies=@('bvi.cpp','bvi.hpp','owned.cs','identity.ps1','transaction.ps1','driver.inc');harness_change='main.cpp reservation attempt only';diagnostic_pretest=(Entry "$Evidence/diagnostic-pretest.json");draft_source_not_accepted=$true;old_source_writes=0;pretest_cases_rerun=0}
$old=Join-Path $Campaign 'hold-ownership-x10d-o-bvi-r2f-control';$oldContract=Json "$old/contract.json"
$inputs=@($oldContract.inputs);foreach($e in $inputs){CheckEntry $e}
NewJson "$Evidence/input-manifest.json" @{scope=$scope;historical_contract=(Entry "$old/contract.json");historical_inputs=$inputs;fixture_oracle_modified=$false;expected_layer_cases=356;expected_supplemental=22;expected_schema_controls=4;expected_coverage_rows=1128;expected_coverage_references=2392;actual_candidate_execution=0;geometry=(Entry "$Campaign/hold-ownership-x10d-o-bvi-r2f/geometry-interface-review.json");PNG_used=0}
foreach($e in $oldContract.dependencies){CheckEntry $e}
NewJson "$Evidence/dependency-manifest.json" @{historical_pin_reference=(Entry "$old/dependency-manifest.json");current_hash_checked_dependencies=$oldContract.dependencies;actual_maintenance_host=(Entry (Join-Path $PSHOME 'pwsh.exe'));managed_interop_source=(Entry "$Source/owned.cs");Add_Type_pretest_only=$true;CXX_compilation=0;actual_compiled_CXX_closure=@();actual_loaded_CXX_closure=@();loaded_managed_Roslyn_system_closure_verified=$false;Debug_ASan_availability_this_attempt_verified=$false;limitation='installed hash/existence is not actual compiled/loaded closure or formal availability gate'}
NewJson "$Evidence/binary-manifest.json" @{candidate_binaries=@();candidate_builds=0;new_out_exists=$false;sizeof=$null;metadata=$null;probes=$null;interop_Add_Type_is_not_BVI_build=$true;actual_CreateProcess=0}
$stages=@();foreach($name in @('natural','nonzero','owned-child','probe-1','probe-2','probe-3','probe-4','configure-release','build-release','wrong-contact-release','suite-release','configure-debug','build-debug','suite-debug','configure-asan','build-asan','suite-asan')){$stages+=@{name=$name;native_exit=$null;runner_exit=$null;verification_exit=$null;launched=$false;status='UNREACHED_PREFORMAL_STOP';denominator_in_execution=0}}
NewJson "$Evidence/stage-summary.json" @{attempt=$Attempt;state=(Entry "$Evidence/state.json");stages=$stages;true_controls=0;true_diagnostics=0;product_commands=0;unexecuted_controls=3;unexecuted_probe_slots=4;unexecuted_products=10;wrapper_repairs=0;runner_freeze=$null;product_freeze=$null;pretests=@{helper=@{cases=25;pass=25;failed=0};transaction=@{cases=32;pass=32;failed=0};control=@{cases=39;pass=39;failed=0};diagnostic=@{cases=20;pass=18;failed=2}};cross_shell_original_rejections=5;STOP_cross_shell_rejections=17;candidate_layer_case_assertions=0;product_adopted=$false}
$docs=@(Entry "$Repo/docs/HOLD_OWNERSHIP_X10D_O_BVI_BUILD_RESULT_20261005.md";Entry "$Repo/docs/HOLD_OWNERSHIP_X10D_O_BVI_BUILD_HANDOFF_20261005.md")
$all=@(Get-ChildItem -LiteralPath $Evidence -Recurse -File|Sort-Object FullName|ForEach-Object{Entry $_.FullName})+$files+$docs
$shards=@();for($i=0;$i -lt $all.Count;$i+=64){$end=[Math]::Min($i+63,$all.Count-1);$path="$Evidence/artifact-ledger-$($shards.Count).json";NewJson $path @{files=@($all[$i..$end])};$shards+=Entry $path}
$logs=[long]0;foreach($f in Get-ChildItem -LiteralPath $Evidence -Recurse -File -Filter '*.log'){$logs+=$f.Length}
if($logs -gt 2097152){throw 'maintenance-logs-cap'}
NewJson "$Evidence/artifact-ledger.json" @{shards=$shards;entries=$all.Count;out_entries=@();self_excluded=@('artifact-ledger.json','final-receipt.json');index_SHA_anchored_by_final=$true;self_bytes_included=$true;all_log_bytes=$logs;all_protection_shards_counted=$true}
foreach($f in Get-ChildItem -LiteralPath $Evidence -Recurse -File){if($f.Length -gt 65536){throw 'maintenance-report-cap'}}
$baseBatch=Bytes $Evidence;$external=Bytes $Source;foreach($doc in $docs){$external+=$doc.bytes}
$receipt=[ordered]@{schema=1;attempt=$Attempt;date='2026-10-05';timezone='Asia/Taipei';status='STOP';classification='preformal diagnostic test adapter fails 2/20; wrapper/build not reached';head=$p.head;index_sha256=$p.index_sha256;git_paths=594;git_paths_sha256=$p.git_paths_sha256;dirty_sha256=$p.dirty_sha256;state=(Entry "$Evidence/state.json");scope=$scope;failure=(Entry "$Evidence/failure-classification.json");STOP_readback=(Entry "$Evidence/STOP-readback.json");artifact_ledger=(Entry "$Evidence/artifact-ledger.json");protected_index=(Entry "$Evidence/protected-index.json");protection=(Entry "$Evidence/protection-final.json");source=(Entry "$Evidence/source-manifest.json");inputs=(Entry "$Evidence/input-manifest.json");dependencies=(Entry "$Evidence/dependency-manifest.json");binary=(Entry "$Evidence/binary-manifest.json");stages=(Entry "$Evidence/stage-summary.json");diagnostic_oracle=(Entry "$Evidence/diagnostic-oracle.json");diagnostic_pretest=(Entry "$Evidence/diagnostic-pretest.json");control_carry=(Entry "$old/final-receipt.json");self_bytes=0;self_hash_excluded=$true;protected_unique=$p.unique_files;protected_references=$p.references;mismatches=0;native_controls=0;native_wrapper_probes=0;native_product_commands=0;wrapper_author_repairs=0;product_adopted=$false;candidate_layer_cases=0;PNG_used=0;PNG_remaining=2;pretest_rounds=@{helper=1;transaction=1;control=1;diagnostic=1};capacity=@{aggregate_carry=8285901212;development_carry=8988255;controller_carry=1299900;controller_reserved=7088708;old_out=374705;batch=0;external=$external;development_new=0;development_total=0;development_cap=58720256;development_subcap=40249475;development_subcap_remaining=0;out_new=0;out_total=374705;out_subcap=134217728;shared_out_cap=268435456;aggregate=0;aggregate_cap=8589934592;aggregate_remaining=0;free=[long](Get-PSDrive C).Free;free_minimum=5704253440;maintenance_log_bytes=$logs;estimate_corrections=0;original_data_deleted=0;PNG_copies=0}}
for($i=0;$i -lt 12;$i++){
 $receipt.capacity.batch=$baseBatch+$receipt.self_bytes;$receipt.capacity.development_new=$receipt.capacity.batch+$external;$receipt.capacity.development_total=8988255+$receipt.capacity.development_new;$receipt.capacity.development_subcap_remaining=40249475-$receipt.capacity.development_new;$receipt.capacity.aggregate=8285901212+$receipt.capacity.development_new;$receipt.capacity.aggregate_remaining=8589934592-$receipt.capacity.aggregate
 $data=[Text.Encoding]::UTF8.GetBytes(($receipt|ConvertTo-Json -Depth 100 -Compress)+"`n");if($data.Length -eq $receipt.self_bytes){break};$receipt.self_bytes=$data.Length
}
if($data.Length -ne $receipt.self_bytes){throw 'self-byte-fixed-point'}
CapacityMath $receipt.capacity.development_new 0 0 0 $receipt.capacity.free
$h=NewHandle "$Evidence/final-receipt.json";try{$h.Write($data);$h.Flush($true)}finally{$h.Dispose()}
$got=Json "$Evidence/final-receipt.json"
if((Bytes $Evidence) -ne $got.capacity.batch -or (Get-Item "$Evidence/final-receipt.json").Length -ne $got.self_bytes -or 8285901212+(Bytes $Evidence)+$external -ne $got.capacity.aggregate){throw 'settled-bytes'}
foreach($sh in $shards){CheckEntry $sh;foreach($e in (Json $sh.path).files){CheckEntry $e}}
foreach($sh in $pshards){CheckEntry $sh}
@{status=$got.status;final=(Entry "$Evidence/final-receipt.json");state=$got.state;capacity=$got.capacity;entries=$all.Count;protected=$p.unique_files;refs=$p.references;old_protected=$p.old_unique;native_launches=0;pretest_failures=2}|ConvertTo-Json -Depth 6 -Compress
