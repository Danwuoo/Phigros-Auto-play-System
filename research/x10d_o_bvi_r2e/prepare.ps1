. "$PSScriptRoot/common.ps1"
if((Json "$Evidence/state.json").status -cne 'INIT_PENDING'){throw 'prepare-once'}
$mock=Json "$Evidence/mock-v1.json";$cross=Json "$Evidence/mock-S06-cross-shell.json"
if($mock.failed -ne 0 -or $mock.actual_CreateProcess_calls -ne 0 -or -not $cross.rejected){throw 'mock-gate'}
$pwsh=Join-Path $PSHOME 'pwsh.exe';$cmake='C:/Users/wurre/AppData/Local/Programs/Python/Python310/Lib/site-packages/cmake/data/bin/cmake.exe';$ninja='C:/Users/wurre/AppData/Local/Programs/Python/Python310/Scripts/ninja.exe';$vc='C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat';$msvc='C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717'
$installed=@($pwsh,$cmake,$ninja,$vc,"$msvc/bin/Hostx64/x64/cl.exe","$msvc/bin/Hostx64/x64/link.exe","$msvc/bin/Hostx64/x64/c1xx.dll","$msvc/bin/Hostx64/x64/clang_rt.asan_dynamic-x86_64.dll","$msvc/lib/x64/clang_rt.asan_dynamic-x86_64.lib","$msvc/lib/x64/clang_rt.asan_dynamic_runtime_thunk-x86_64.lib")
$dependencies=@();foreach($p in $installed){$dependencies+=Entry $p}
foreach($e in (Json "$Campaign/hold-ownership-x10d-o-bvi-r1/dependency-manifest.json").dependencies){if($e.path -like '*nlohmann*'){$dependencies+=$e}}
$inputManifest=Json "$Campaign/hold-ownership-x10d-o-bvi-r1/input-manifest.json"
$inputs=@($inputManifest.inputs)+@(Entry "$Evidence/png-packet.json")
$fixtureArgs=@("$Campaign/hold-ownership-x10d-o-bvi/normalized-execution.json","$Campaign/hold-ownership-x10d-o-bvi/oracle.json","$Campaign/hold-ownership-x10d-o-bvi-r1/typed-r1.json","$Repo/research/x10d_o_bvi/supplemental.json","$Campaign/hold-ownership-x10d-o-bvi-r1/r1-cases.json","$Campaign/hold-ownership-x10d-o-bvi-r1/expected-coverage.json")
foreach($f in (Json "$Evidence/png-packet.json").frames){$inputs+=@{path=$f.path;sha256=$f.png_sha256;bytes=(Get-Item -LiteralPath $f.path).Length}}
$stages=[Collections.Generic.List[object]]::new()
function Stage($name,$exe,$argvVector,$native,$runner,$report,$total=300,$cap=524288,$reserveOut=0){$stages.Add(@{name=$name;exe=$exe;argv=$argvVector;native_exit=$native;runner_exit=$runner;report=$report;total_s=$total;stream_cap=$cap;reserve_development=if($report -like '*suite*'){5242880}else{1179648};reserve_out=$reserveOut;next=$null})}
Stage natural $pwsh @('-NoProfile','-Command','exit 0') 0 0 $null 30 65536
Stage nonzero $pwsh @('-NoProfile','-Command','exit 7') 7 7 $null 30 65536
NewJson "$Evidence/child-binding.json" @{attempt=$Attempt;pwsh=$pwsh;child_script=(Join-Path $Source 'control-child.ps1');child_identity=(Join-Path $Evidence 'child-identity.json');parent_identity=(Join-Path $Evidence 'parent-identity.json')}
$inputs+=Entry "$Evidence/child-binding.json"
Stage owned-child $pwsh @('-NoProfile','-File',(Join-Path $Source 'control-parent.ps1'),'-Binding',(Join-Path $Evidence 'child-binding.json'),'-Attempt',$Attempt) 0 125 $null 30 65536
foreach($configuration in @('release','debug','asan')){
 $type=if($configuration -eq 'release'){'Release'}else{'Debug'};$asan=if($configuration -eq 'asan'){'ON'}else{'OFF'}
 $configure="@echo off`r`nset VSCMD_SKIP_SENDTELEMETRY=1`r`nset VC_DISABLE_SQM=1`r`nset VS_UNICODE_OUTPUT=`r`ncall `"$vc`" -vcvars_ver=14.50 >nul`r`nif errorlevel 1 exit /b 1`r`n`"$cmake`" -S `"$Source`" -B `"$Out/$configuration`" -G Ninja -DCMAKE_BUILD_TYPE=$type -DBVI_ASAN=$asan `"-DCMAKE_MAKE_PROGRAM=$ninja`"`r`nexit /b %errorlevel%`r`n"
 $build="@echo off`r`nset VSCMD_SKIP_SENDTELEMETRY=1`r`nset VC_DISABLE_SQM=1`r`nset VS_UNICODE_OUTPUT=`r`ncall `"$vc`" -vcvars_ver=14.50 >nul`r`nif errorlevel 1 exit /b 1`r`n`"$cmake`" --build `"$Out/$configuration`" --parallel 2`r`nexit /b %errorlevel%`r`n"
 foreach($item in @(@("configure-$configuration.cmd",$configure),@("build-$configuration.cmd",$build))){$h=NewHandle (Join-Path $Source $item[0]);try{$b=[Text.Encoding]::UTF8.GetBytes($item[1]);$h.Write($b);$h.Flush($true)}finally{$h.Dispose()}}
 Stage "configure-$configuration" 'C:/Windows/System32/cmd.exe' @('/d','/s','/c',(Join-Path $Source "configure-$configuration.cmd")) 0 0 $null 300 524288 1048576
 Stage "build-$configuration" 'C:/Windows/System32/cmd.exe' @('/d','/s','/c',(Join-Path $Source "build-$configuration.cmd")) 0 0 $null 300 524288 41943040
 if($configuration -eq 'release'){Stage wrong-contact-release "$Out/release/bvi_tests.exe" ($fixtureArgs+@("$Evidence/wrong-contact-release-report.json",'wrong-contact-only')) 1 1 'wrong-contact-release-report.json'}
 Stage "suite-$configuration" "$Out/$configuration/bvi_tests.exe" ($fixtureArgs+@("$Evidence/suite-$configuration-report.json")) 0 0 "suite-$configuration-report.json"
}
Stage png-current "$Out/release/bvi_png.exe" @("$Evidence/png-packet.json","$Evidence/png-current-report.json") 0 0 'png-current-report.json'
for($i=0;$i -lt $stages.Count-1;$i++){$stages[$i].next=$stages[$i+1].name}
NewJson "$Evidence/configuration-availability.json" @{debug='installed MSVC Debug static CRT files present; execution unverified';asan='installed MSVC ASan libs/DLL present; execution unverified';dependencies=$dependencies;alternative_compiler_search=0}
NewJson "$Evidence/geometry-sources.json" @{selection=(Entry "$Campaign/hold-ownership-x10d-o-observability/review-selection.json");historical_packet=(Entry "$Campaign/hold-ownership-x10d-o/rgb-packet.json");trace=(Entry ((Json "$Campaign/hold-ownership-x10d-o/rgb-packet.json").trace_path));legal_current_roi_source=$null;legal_all_line_source=$null;reason='The selected packet does not contain BVI front/depth/width/angle ROI measurements or a bounded-current geometry provenance adapter. Historical bank includes full-prefix tracking fields and candidate parts; no legal adapter is implemented/assumed in this package.';candidate_inputs_from_historical_bank=$false;authoring_parts_excluded=$true;clock_mapping='original host QPC ns kept; source timestamp domain separate; no calibrated render-age claim'}
NewJson "$Evidence/contract.json" @{schema=1;attempt=$Attempt;binding=(Binding);protocol=(Entry "$Source/PROTOCOL.md");inputs=$inputs;dependencies=$dependencies;stages=$stages;development_subcap=41943040;out_subcap=134217728;aggregate_carry=8283855884;png_used_before=0;max_png=2;second_png_mode=$null;geometry_sources=(Entry "$Evidence/geometry-sources.json")}
$sha=Sha "$Evidence/contract.json";$runner=[IO.File]::ReadAllText("$Source/run.ps1");if(-not $runner.Contains('REPLACE_CONTRACT_SHA')){throw 'runner-already-sealed'};[IO.File]::WriteAllText("$Source/run.ps1",$runner.Replace('REPLACE_CONTRACT_SHA',$sha),[Text.UTF8Encoding]::new($false))
$sources=@();foreach($f in Get-ChildItem -LiteralPath $Source -File){$sources+=Entry $f.FullName}
$sources+=Entry "$Evidence/mock-v1.json";$sources+=Entry "$Evidence/mock-S06-cross-shell.json";$sources+=Entry "$Evidence/geometry-sources.json"
NewJson "$Evidence/freeze.json" @{schema=1;attempt=$Attempt;files=$sources;contract_sha=$sha;candidate_unchanged_from_R1=@(Entry "$Source/bvi.cpp";Entry "$Source/bvi.hpp");negative_adapter='driver/main only; shared AssertionAccumulator, real exit1/consumer reject';formal_controls_started=$false}
$null=Capacity 16777216 125829120
$p=Protection;NewJson "$Evidence/protection-freeze.json" $p
$h=NewHandle "$Evidence/attempt.lock";$h.Dispose()
# This consumes the unique initialization permit. Runtime entry never initializes.
$state=@{schema=1;attempt=$Attempt;status='READY';next='natural';running=$null;reason=$null;revision=1;native_stages_consumed=0;native_launches_known=0;contract_sha=$sha;freeze_sha=(Sha "$Evidence/freeze.json");receipts=@()}
$h=NewHandle "$Evidence/state-preflight.pending";try{PutHandle $h $state}finally{$h.Dispose()};[IO.File]::Move("$Evidence/state-preflight.pending","$Evidence/state.json",$true)
@{source_frozen=$true;stages=$stages.Count;controls=3;product_commands=$stages.Count-3;next='natural';contract_sha=$sha;capacity=(Capacity)}|ConvertTo-Json -Depth 4
