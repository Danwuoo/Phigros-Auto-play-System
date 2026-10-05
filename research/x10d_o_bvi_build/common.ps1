. "$PSScriptRoot/base.ps1"
function Protection(){
 $b=LegacyProtection
 $cache=@{};$refs=0
 function C($e){$script:newProtectionRefs++;$p=[IO.Path]::GetFullPath($e.path);if(-not $cache.ContainsKey($p)){$cache[$p]=Entry $p};$a=$cache[$p];if($a.sha256 -cne $e.sha256 -or $a.bytes -ne $e.bytes){throw "control-protection:$p"}}
 $script:newProtectionRefs=0
 $old=Join-Path $Campaign 'hold-ownership-x10d-o-bvi-r2f-control'
 if((Sha "$old/final-receipt.json") -cne '28e653c844d44f0d80ef716fadbb7137a715a01be81de22bed809a953da8c2d8' -or (Sha "$old/state.json") -cne '9089d70d45978a13c83fe62d34b82fa39db08a861403aea2808160998be1f363'){throw 'control-anchor'}
 $r=Json "$old/final-receipt.json";C (Entry "$old/final-receipt.json");C $r.artifact_ledger
 foreach($sh in (Json $r.artifact_ledger.path).shards){C $sh;foreach($e in (Json $sh.path).files){C $e}}
 foreach($v in $r.PSObject.Properties.Value){if($v -and $v.path -and $v.sha256){C $v}}
 foreach($e in @((Json "$old/freeze.json").files)+@((Json "$old/contract.json").inputs)+@((Json "$old/contract.json").dependencies)){C $e}
 # Deduplicate the full original reconstruction and these new references exactly by a read-only entry hook.
 $script:fullProtectionCache=@{};$script:fullProtectionRefs=0
 $originalEntry=(Get-Item Function:Entry).ScriptBlock
 function Entry($p){$e=& $originalEntry $p;$script:fullProtectionRefs++;$script:fullProtectionCache[[IO.Path]::GetFullPath($e.path)]=$e;$e}
 try{$null=LegacyProtection;foreach($e in $cache.Values){$null=Entry $e.path}}finally{Set-Item Function:Entry $originalEntry}
 $allPaths=@(& git -c core.quotepath=false ls-files --cached --others --exclude-standard);if($LASTEXITCODE){throw 'git-paths'}
 $allDirty=@(& git -c core.quotepath=false status --porcelain=v1 --untracked-files=all);if($LASTEXITCODE){throw 'git-dirty'}
 $paths=@($allPaths|Where-Object{$_ -notlike 'research/x10d_o_bvi_build/*' -and $_ -notlike 'docs/HOLD_OWNERSHIP_X10D_O_BVI_BUILD_*'})
 $dirty=@($allDirty|Where-Object{$p=$_.Substring(3);$p -notlike 'research/x10d_o_bvi_build/*' -and $p -notlike 'docs/HOLD_OWNERSHIP_X10D_O_BVI_BUILD_*'})
 if($paths.Count -ne 594){throw 'original-git594'}
 $result=@{schema=1;unique_files=$script:fullProtectionCache.Count;references=$b.references+$script:newProtectionRefs;mismatches=0;head=$b.head;index_sha256=$b.index_sha256;git_paths=594;git_paths_sha256=(TextSha ($paths|Sort-Object));dirty_sha256=(TextSha $dirty);legacy=$b;control_final=(Entry "$old/final-receipt.json");control_references=$script:newProtectionRefs;control_unique=$cache.Count;PNG_used=0}
 if(Test-Path "$Evidence/protection-before.json"){$before=Json "$Evidence/protection-before.json";if($result.git_paths_sha256 -cne $before.git_paths_sha256 -or $result.dirty_sha256 -cne $before.dirty_sha256){throw 'original-dirty-change'}}
 $result
}
function CheckState($s,$stage,$contractSha,$freezeSha){
 BaseCheckState $s $stage $contractSha $freezeSha
 if($s.diagnostic_receipts){foreach($e in $s.diagnostic_receipts){CheckEntry $e;$v=Json $e.path;if($v.pending -or $v.attempt -cne $Attempt -or -not $v.integrity_verified -or $v.classification -notin @('DIAGNOSTIC_REJECTED','DIAGNOSTIC_POSITIVE')){throw 'diagnostic-receipt-untrusted'};foreach($ref in $v.entries){CheckEntry $ref}}}
}
