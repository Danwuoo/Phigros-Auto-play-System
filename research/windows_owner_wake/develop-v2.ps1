param([Parameter(Mandatory)][string]$ReceiptRepo,[ValidatePattern('^[a-z0-9-]{1,15}$')][string]$Attempt='wake02')
$ErrorActionPreference='Stop'
$base=Join-Path $PSScriptRoot 'develop.ps1';$text=[IO.File]::ReadAllText($base)
$body=$text.Substring($text.IndexOf("`$ErrorActionPreference='Stop'"))
# Windows checkout CRLF is explicit: retain both original byte bindings and
# prove strictly decoded UTF8 text differs ONLY by CRLF/LF, never trim/BOM.
function CanonicalSource([string]$path){
 $raw=[IO.File]::ReadAllBytes($path);if($raw.Length -ge 1048576){throw 'source-byte-cap'}
 $decoded=[Text.UTF8Encoding]::new($false,$true).GetString($raw)
 if($decoded.StartsWith([string][char]0xfeff,[StringComparison]::Ordinal)){throw 'source-bom'}
 $lf=$decoded.Replace("`r`n","`n");if($lf.Contains("`r")){throw 'source-isolated-cr'}
 [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($lf))).ToLowerInvariant()
}
$oldJoin="if(`$prior.Count -ne 1 -or `$b.sha256 -cne `$prior[0].sha256 -or `$b.bytes -ne `$prior[0].bytes){throw 'unchanged-core-source-join'}"
$newJoin="if(`$prior.Count -ne 1){throw 'core-source-join-missing'};CheckEntry `$prior[0];`$a=CanonicalSource `$b.path;`$z=CanonicalSource `$prior[0].path;if(`$a -cne `$z){throw 'canonical-core-source-changed'};`$encodingJoins+=@(@{configuration=`$config;relative_path=`$rel;current=`$b;original=`$prior[0];canonical_lf_sha256=`$a;only_utf8_CRLF_LF_difference=`$true})"
if(-not $body.Contains($oldJoin)){throw 'exact-adapter-patch-required'}
$body=$body.Replace($oldJoin,$newJoin)
$body=$body.Replace('$rows=@();$builds=@();$candidateFiles=@()','$rows=@();$builds=@();$candidateFiles=@();$encodingJoins=@()')
$body=$body.Replace('source_manifest=(Binding $sourcePath);builds=$builds;tests=$rows;','source_manifest=(Binding $sourcePath);builds=$builds;tests=$rows;source_encoding_joins=$encodingJoins;')
$body=$body.Replace('core/observer/hook/owner algorithms byte unchanged','core/observer/hook/owner algorithms unchanged; core LF canonical bytes verified')
$body=$body.Replace('$baseInputs=@($PSCommandPath,$recipe,$sourcePath,$retained,$inherited)','$baseInputs=@($PSCommandPath,$base,$recipe,$sourcePath,$retained,$inherited)')
. ([scriptblock]::Create($body))
