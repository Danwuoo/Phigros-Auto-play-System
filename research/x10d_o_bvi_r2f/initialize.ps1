. "$PSScriptRoot/common.ps1"
$binding=Binding
InitializeRoots
$p=Protection
$c=Capacity 41900000 134000000
[IO.Directory]::CreateDirectory($Evidence)|Out-Null
NoAlias $Evidence
NewJson "$Evidence/state.json" @{attempt=$Attempt;status='INIT_PENDING';revision=0;reason='implementation and transaction gates pending';native_launches_known=0}
NewJson "$Evidence/protection-before.json" $p
NewJson "$Evidence/contract-before.json" @{schema=1;binding=$binding;dispatch=(Entry "$Repo/docs/HOLD_OWNERSHIP_X10D_O_BVI_R2F_DISPATCH_20261005.md");controller=(Entry "$Campaign/hold-ownership-x10d-o-bvi-r2e/controller-review/controller-receipt.json");capacity=$c;native_blocked=$true}
@{initialized=$true;protection=$p.unique_files;references=$p.references;capacity=$c}|ConvertTo-Json -Compress
