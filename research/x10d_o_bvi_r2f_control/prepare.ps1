. "$PSScriptRoot/common.ps1"
. "$PSScriptRoot/transaction.ps1"
if((Json "$Evidence/state.json").status -cne 'INIT_PENDING'){throw 'prepare-once'}
$mock=Json "$Evidence/mock-v1.json";$pretest=Json "$Evidence/transaction-round-1.json"
$controlReports=@(Get-ChildItem -LiteralPath $Evidence -File -Filter 'control-round-*.json'|Sort-Object Name)
if($controlReports.Count -lt 1 -or $controlReports.Count -gt 2){throw 'control-round-count'}
$control=Json $controlReports[-1].FullName
if($control.failed -ne 0 -or $control.cases.Count -ne 39 -or $control.actual_CreateProcess -ne 0){throw 'control-pretest-gate'}
if($mock.failed -ne 0 -or $pretest.failed -ne 0 -or $pretest.cases.Count -ne 32 -or $pretest.actual_CreateProcess -ne 0 -or -not (Json "$Evidence/mock-S06-cross-shell.json").rejected -or @((Json "$Evidence/transaction-cross-shell.json").rows|Where-Object rejected -ne $true).Count){throw 'pretest-gate'}
foreach($e in @($pretest.shared_source)+@($control.shared_source)){CheckEntry $e}
$pwsh=Join-Path $PSHOME 'pwsh.exe'
$cmake='C:/Users/wurre/AppData/Local/Programs/Python/Python310/Lib/site-packages/cmake/data/bin/cmake.exe'
$ninja='C:/Users/wurre/AppData/Local/Programs/Python/Python310/Scripts/ninja.exe'
$vc='C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat'
$msvc='C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717'
$depPaths=@($pwsh,$cmake,$ninja,$vc,"$msvc/bin/Hostx64/x64/cl.exe","$msvc/bin/Hostx64/x64/link.exe","$msvc/bin/Hostx64/x64/c1xx.dll","$msvc/lib/x64/libcmtd.lib","$msvc/lib/x64/libcpmtd.lib","$msvc/bin/Hostx64/x64/clang_rt.asan_dynamic-x86_64.dll","$msvc/bin/Hostx64/x64/clang_rt.asan_dbg_dynamic-x86_64.dll","$msvc/lib/x64/clang_rt.asan_dynamic-x86_64.lib","$msvc/lib/x64/clang_rt.asan_dynamic_runtime_thunk-x86_64.lib")
$dependencies=@($depPaths|ForEach-Object{Entry $_})
foreach($e in (Json "$Campaign/hold-ownership-x10d-o-bvi-r1/dependency-manifest.json").dependencies){if($e.path -like '*nlohmann*'){CheckEntry $e;$dependencies+= $e}}
$inputPaths=@("$Campaign/hold-ownership-x10d-o-bvi/normalized-execution.json","$Campaign/hold-ownership-x10d-o-bvi/oracle.json","$Campaign/hold-ownership-x10d-o-bvi-r1/typed-r1.json","$Repo/research/x10d_o_bvi/supplemental.json","$Campaign/hold-ownership-x10d-o-bvi-r1/r1-cases.json","$Campaign/hold-ownership-x10d-o-bvi-r1/expected-coverage.json")
$inputs=@($inputPaths|ForEach-Object{Entry $_})
$inputs+=Entry "$Evidence/scope-amendment.md"
$inputs+=Entry "$Campaign/hold-ownership-x10d-o-bvi-r2f/geometry-interface-review.json"
NewJson "$Evidence/child-binding.json" @{attempt=$Attempt;pwsh=$pwsh;child_script=(Join-Path $Source 'control-child.ps1');child_identity=(Join-Path $Evidence 'child-identity.json');parent_identity=(Join-Path $Evidence 'parent-identity.json');parent_poll=(Join-Path $Evidence 'parent-poll.json')}
$inputs+=Entry "$Evidence/child-binding.json"
$stages=[Collections.Generic.List[object]]::new()
function Stage($name,$exe,$argvVector,$native,$runner,$report,$total,$cap,$reserveDev,$reserveOut,$why){$stages.Add(@{name=$name;exe=$exe;argv=$argvVector;native_exit=$native;runner_exit=$runner;report=$report;total_s=$total;stream_cap=$cap;reserve_development=$reserveDev;reserve_out=$reserveOut;reserve_basis=$why;next=$null})}
Stage natural $pwsh @('-NoProfile','-Command','exit 0') 0 0 $null 30 65536 393216 0 'two64KiB streams+eight16KiB checkpoints+receipts/margin, no out'
Stage nonzero $pwsh @('-NoProfile','-Command','exit 7') 7 7 $null 30 65536 393216 0 'same bounded control output'
Stage owned-child $pwsh @('-NoProfile','-File',(Join-Path $Source 'control-parent.ps1'),'-Binding',(Join-Path $Evidence 'child-binding.json'),'-AttemptId',$Attempt) 0 125 $null 30 65536 393216 0 'same control+two4KiB identities+8KiB bounded poll within margin'
foreach($configuration in @('release','debug','asan')){
 $type=if($configuration -eq 'release'){'Release'}else{'Debug'};$asan=if($configuration -eq 'asan'){'ON'}else{'OFF'}
 $configure="@echo off`r`nset VSCMD_SKIP_SENDTELEMETRY=1`r`nset VC_DISABLE_SQM=1`r`nset VS_UNICODE_OUTPUT=`r`ncall `"$vc`" -vcvars_ver=14.50 >nul`r`nif errorlevel 1 exit /b 1`r`n`"$cmake`" -S `"$Source`" -B `"$Out/$configuration`" -G Ninja -DCMAKE_BUILD_TYPE=$type -DBVI_ASAN=$asan `"-DCMAKE_MAKE_PROGRAM=$ninja`"`r`nexit /b %errorlevel%`r`n"
 $build="@echo off`r`nset VSCMD_SKIP_SENDTELEMETRY=1`r`nset VC_DISABLE_SQM=1`r`nset VS_UNICODE_OUTPUT=`r`ncall `"$vc`" -vcvars_ver=14.50 >nul`r`nif errorlevel 1 exit /b 1`r`n`"$cmake`" --build `"$Out/$configuration`" --parallel 2`r`nexit /b %errorlevel%`r`n"
 foreach($item in @(@("configure-$configuration.cmd",$configure),@("build-$configuration.cmd",$build))){$h=NewHandle (Join-Path $Source $item[0]);try{$b=[Text.Encoding]::UTF8.GetBytes($item[1]);$h.Write($b);$h.Flush($true)}finally{$h.Dispose()}}
 $configureReserve=4194304
 if($configuration -eq 'asan'){$configureReserve+=(Get-Item "$msvc/bin/Hostx64/x64/clang_rt.asan_dynamic-x86_64.dll").Length+(Get-Item "$msvc/bin/Hostx64/x64/clang_rt.asan_dbg_dynamic-x86_64.dll").Length}
 Stage "configure-$configuration" 'C:/Windows/System32/cmd.exe' @('/d','/s','/c',(Join-Path $Source "configure-$configuration.cmd")) 0 0 $null 300 524288 1572864 $configureReserve 'oneMiB streams+checkpoint/receipt margin; compiler probes/CMake4MiB plus measured installed ASan DLLs if copied'
 Stage "build-$configuration" 'C:/Windows/System32/cmd.exe' @('/d','/s','/c',(Join-Path $Source "build-$configuration.cmd")) 0 0 $null 300 524288 1572864 41943040 'oneMiB streams+checkpoint/receipts; two C++ objs,EXE,PDB and incremental generated build <=40MiB reserve'
 if($configuration -eq 'release'){Stage wrong-contact-release "$Out/release/bvi_tests.exe" ($inputPaths+@("$Evidence/wrong-contact-release-report.json",'wrong-contact-only')) 1 1 'wrong-contact-release-report.json' 300 524288 1572864 0 'oneMiB streams+small genuine two failed-row report/receipts'}
 Stage "suite-$configuration" "$Out/$configuration/bvi_tests.exe" ($inputPaths+@("$Evidence/suite-$configuration-report.json")) 0 0 "suite-$configuration-report.json" 300 524288 6291456 0 '4MiB max report+oneMiB streams+checkpoints/receipts/margin'
}
for($i=0;$i -lt $stages.Count-1;$i++){$stages[$i].next=$stages[$i+1].name}
NewJson "$Evidence/configuration-availability.json" @{debug='installed libcmtd/libcpmtd present and SHA pinned; actual configuration execution required';asan='installed MSVC ASan debug+release DLL/libs present and SHA pinned; execution required';dependencies=$dependencies;unsupported_configs=@();no_install=$true;compiled_or_loaded_closure_verified=$false}
if($stages.Count -ne 13 -or @($stages|Where-Object{$_.argv.Count -lt 1 -or @($_.argv|Where-Object{$null -eq $_ -or [string]$_ -eq ''}).Count}).Count){throw 'argv-empty-or-stage-count'}
NewJson "$Evidence/argv-readback.json" @{schema=1;attempt=$Attempt;stages=@($stages|ForEach-Object{@{name=$_.name;exe=$_.exe;argv=$_.argv}});argv_empty=0;inspected_before_freeze=$true}
NewJson "$Evidence/contract.json" @{schema=1;attempt=$Attempt;binding=(Binding);protocol=(Entry "$Source/PROTOCOL.md");scope=(Entry "$Evidence/scope-amendment.md");inputs=$inputs;dependencies=$dependencies;stages=$stages;development_subcap=40902683;out_subcap=134217728;aggregate_carry=8285248004;development_carry=8335047;controller_reserved=7088708;PNG_used=0;max_PNG=2;PNG_stages=@();estimate_corrections_max=2;pretest=(Entry "$Evidence/transaction-round-1.json");control_pretest=(Entry $controlReports[-1].FullName)}
$sha=Sha "$Evidence/contract.json";$runner=[IO.File]::ReadAllText("$Source/run.ps1");if(-not $runner.Contains('REPLACE_CONTRACT_SHA')){throw 'runner-already-sealed'};[IO.File]::WriteAllText("$Source/run.ps1",$runner.Replace('REPLACE_CONTRACT_SHA',$sha),[Text.UTF8Encoding]::new($false))
$sources=@(Get-ChildItem -LiteralPath $Source -File|Sort-Object Name|ForEach-Object{Entry $_.FullName})
foreach($leaf in @('bvi.cpp','bvi.hpp')){if((Sha "$Source/$leaf") -cne (Sha "$Repo/research/x10d_o_bvi_r1/$leaf")){throw 'R1-candidate-sha'}}
foreach($e in @($pretest.shared_source)+@($control.shared_source)){CheckEntry $e}
NewJson "$Evidence/freeze.json" @{schema=1;attempt=$Attempt;files=$sources;contract_sha=$sha;shared_transaction_preverified=(Entry "$Evidence/transaction-round-1.json");control_preverified=(Entry $controlReports[-1].FullName);pretest_source_match=$true;formal_controls_started=$false;candidate_unchanged_from_R1=$true;fixture_oracle_modified=$false}
NewJson "$Evidence/protection-freeze.json" (Protection)
$null=Capacity 1048576 0
$h=NewHandle "$Evidence/attempt.lock";$h.Dispose()
$state=@{schema=1;attempt=$Attempt;status='READY';next='natural';running=$null;reason=$null;revision=1;native_stages_consumed=0;native_launches_known=0;contract_sha=$sha;freeze_sha=(Sha "$Evidence/freeze.json");receipts=@()}
DurableState $Evidence $state
@{source_frozen=$true;stages=$stages.Count;controls=3;product_commands=$stages.Count-3;next='natural';contract_sha=$sha;capacity=(Capacity)}|ConvertTo-Json -Depth 5 -Compress
