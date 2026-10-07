param([Parameter(Mandatory)][string]$Report)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $Report){throw 'fresh-cache-controls'}
if(-not ('PasOwnedV3' -as [type])){Add-Type -Path (Join-Path $PSScriptRoot 'owned.cs')}
$cases=@(
 @('same-held-exited',$true),@('no-cache',$false),@('pid-reused',$false),
 @('creation-mismatch',$false),@('different-handle',$false),@('zero-handle',$false),
 @('still-live',$false),@('empty-image',$false),@('unvalidated-image',$false)
)
$rows=@()
foreach($case in $cases){
 $identity=[PasOwnedV3+PinnedIdentity]::new()
 $identity.pid=123;$identity.creation_filetime=456;$identity.image='FAKE-validated-image'
 $identity.image_validation_qpc=100;$identity.handle=[IntPtr]789
 $testPid=[uint32]123;$creation=[long]456;$handle=[IntPtr]789;$exited=$true
 switch($case[0]){
  'no-cache'{$identity=$null};'pid-reused'{$testPid=124};'creation-mismatch'{$creation=457}
  'different-handle'{$handle=[IntPtr]790};'zero-handle'{$handle=[IntPtr]::Zero}
  'still-live'{$exited=$false};'empty-image'{$identity.image=''}
  'unvalidated-image'{$identity.image_validation_qpc=0}
 }
 $actual=[PasOwnedV3]::CachedImageAllowed($identity,$testPid,$creation,$handle,$exited)
 $rows+=@{id=$case[0];expected=$case[1];actual=$actual;pass=($actual -eq $case[1])}
}
$failed=@($rows|Where-Object pass -ne $true).Count
$value=@{schema='pas.owned-held-identity-controls.v3';cases=$rows;failed=$failed;actual_CreateProcess=0;fake_facts_not_platform_evidence=$true}
[IO.File]::WriteAllText($Report,($value|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
@{cases=$rows.Count;failed=$failed;actual_CreateProcess=0}|ConvertTo-Json -Compress
if($failed){exit 1};exit 0
