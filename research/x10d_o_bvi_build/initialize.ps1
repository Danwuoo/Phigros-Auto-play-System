. "$PSScriptRoot/common.ps1"
if((Test-Path "$Evidence/state.json") -or (Test-Path $Out)){throw 'initialize-once'}
$null=Binding
NewJson "$Evidence/protection-before.json" (Protection)
Copy-Item -LiteralPath "$Source/scope-template.md" -Destination "$Evidence/scope.md" -ErrorAction Stop
NewJson "$Evidence/capacity-initialize.json" (Capacity 1048576 0)
NewJson "$Evidence/state.json" @{schema=1;attempt=$Attempt;status='INIT_PENDING';next=$null;revision=0;native_stages_consumed=0;native_launches_known=0;receipts=@();diagnostic_receipts=@();diagnostic_slots_consumed=0;wrapper_repairs=0;product_frozen=$false}
@{initialized=$true;protection=(Json "$Evidence/protection-before.json");capacity=(Capacity)}|ConvertTo-Json -Depth 8 -Compress
