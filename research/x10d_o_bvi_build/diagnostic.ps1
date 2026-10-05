function DiagnosticSlot($s,$spec){
 if($spec.name -notmatch '^probe-([1-4])$' -or $s.diagnostic_slots_consumed -ne [int]$Matches[1]-1 -or $s.diagnostic_slots_consumed -ge 4 -or $s.product_frozen){throw 'diagnostic-slot'}
 if($spec.wrapper_revision -ne $s.wrapper_revision -or $s.wrapper_repairs -gt 2){throw 'diagnostic-revision'}
 if($s.receipts.Count -ne 3){throw 'diagnostic-controls'}
 if($spec.exe -cne 'C:\Windows\System32\cmd.exe' -or $spec.argv.Count -ne 4 -or ($spec.argv[0..2] -join '|') -cne '/d|/s|/c' -or $spec.argv[3] -cne "$Source\wrappers-v$($s.wrapper_revision)\probe.cmd"){throw 'diagnostic-command-allowlist'}
}
function DiagnosticOutcome($facts,$spec,$text){
 $copy=[pscustomobject]@{name=$spec.name;native_exit=$facts.exit_code;runner_exit=$facts.exit_code;total_s=30}
 VerifyProcess $facts $copy
 if($facts.exit_code -gt 255 -or $facts.runner_exit -ne $facts.exit_code){throw 'diagnostic-exit-untrusted'}
 if($facts.exit_code -eq 0 -and $text.Contains("TRACE DIAGNOSTIC_POSITIVE wrapper-v$($spec.wrapper_revision)") -and $text.Contains('TRACE L07 vcvars END exit=0') -and $text.Contains('TRACE L12 argv-configure END exit=0') -and $text.Contains('TRACE L17 argv-build END exit=7')){'DIAGNOSTIC_POSITIVE'}else{'DIAGNOSTIC_REJECTED'}
}
function DiagnosticGate($s){
 if(-not $s.diagnostic_positive -or $s.diagnostic_receipts.Count -lt 1 -or $s.diagnostic_slots_consumed -gt 4 -or $s.wrapper_repairs -gt 2){throw 'diagnostic-positive-required'}
 foreach($e in $s.diagnostic_receipts){CheckEntry $e;$v=Json $e.path;if($v.pending -or -not $v.integrity_verified -or $v.attempt -cne $Attempt){throw 'diagnostic-receipt-untrusted'};foreach($r in $v.entries){CheckEntry $r}}
 $last=Json $s.diagnostic_receipts[-1].path
 if($last.classification -cne 'DIAGNOSTIC_POSITIVE' -or $last.verification_exit -ne 0 -or $last.wrapper_revision -ne $s.wrapper_revision){throw 'diagnostic-positive-version'}
}
function FinishDiagnostic($tx,$facts,$capacity){
 $tx.facts=$facts;if($facts.created -eq $true){$tx.state.native_launches_known++}
 WriteTransaction $tx "$($tx.stage)-result.json" @{attempt=$Attempt;stage=$tx.stage;facts=$facts;pending=$false}
 foreach($leaf in @("$($tx.stage).stdout.log","$($tx.stage).stderr.log")){$tx.handles[$leaf].Flush($true)}
 $text=[IO.File]::ReadAllText((Join-Path $tx.dir "$($tx.stage).stdout.log"))
 $outcome=DiagnosticOutcome $facts $tx.spec $text
 if($capacity.out_new -ne 0){throw 'diagnostic-product-side-effect'}
 $entries=@();foreach($leaf in @($tx.handles.Keys|Sort-Object)){if($leaf -cne "$($tx.stage)-verification.json"){$entries+=Entry (Join-Path $tx.dir $leaf)}}
 $entries+=Entry "$Evidence/wrapper-v$($tx.spec.wrapper_revision).json"
 foreach($v in (Json "$Evidence/wrapper-v$($tx.spec.wrapper_revision).json").files){$entries+=$v.entry}
 $v=@{attempt=$Attempt;stage=$tx.stage;pending=$false;classification=$outcome;integrity_verified=$true;verification_exit=if($outcome -eq 'DIAGNOSTIC_POSITIVE'){0}else{1};native_exit=$facts.exit_code;runner_exit=$facts.runner_exit;wrapper_revision=$tx.spec.wrapper_revision;entries=$entries;capacity_after=$capacity;product_executed=$false;producer_complete=$true;diagnostic_rejection_is_not_pass=$true}
 WriteTransaction $tx "$($tx.stage)-verification.json" $v
 $tx.sealed=$true;$a=Entry "$($tx.dir)/$($tx.stage)-verification.json";$got=Json $a.path
 foreach($e in $got.entries){CheckEntry $e}
 $tx.state.diagnostic_receipts+=$a;$tx.state.running=$null;$tx.state.revision++
 if($outcome -eq 'DIAGNOSTIC_POSITIVE'){$tx.state.diagnostic_positive=$true;$tx.state.status='READY';$tx.state.next='product-freeze'}
 elseif($tx.state.diagnostic_slots_consumed -ge 4){$tx.state.status='STOP';$tx.state.reason='diagnostic-slots-exhausted';$tx.state.next=$null}
 else{$tx.state.status='READY';$tx.state.next=$tx.spec.next}
 DurableState $tx.dir $tx.state
 if($tx.state.status -eq 'READY'){CheckState (Json "$Evidence/state.json") $tx.state.next $tx.state.contract_sha $tx.state.freeze_sha}
 $v
}
