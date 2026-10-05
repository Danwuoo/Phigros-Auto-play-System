. "$PSScriptRoot/common.ps1"
$binding=Binding
InitializeRoots
$p=Protection
$c=Capacity 41900000 134000000
[IO.Directory]::CreateDirectory($Evidence)|Out-Null
NoAlias $Evidence
NewJson "$Evidence/state.json" @{attempt=$Attempt;status='INIT_PENDING';revision=0;reason='implementation and mock gate not complete';native_launches=0}
NewJson "$Evidence/protection-before.json" $p
NewJson "$Evidence/contract-before.json" @{schema=1;binding=$binding;protocol=(Entry "$Source/PROTOCOL.md");dispatch=(Entry "$Campaign/hold-ownership-x10d-o-bvi-r2e-dispatch/dispatch-receipt.json");capacity=$c;state_native_blocked=$true;independent_oracle='PROTOCOL P01-P09 S01-S07 E01-E05 C01-C04';source_implementation_frozen=$false}
@{initialized=$true;reconstructed=$p.reconstructed;unique=$p.unique_files;git_paths=$p.git_paths;capacity=$c}|ConvertTo-Json -Depth 4
