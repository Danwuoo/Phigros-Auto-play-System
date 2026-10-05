param([ValidateSet('Configure','Build','Tests','Verify','Scope','Negative')][string]$Mode,
      [ValidateSet('release','asan')][string]$Config='release',[string]$Tag='1')
$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$campaignPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue'
$batchPath=Join-Path $campaignPath 'early-role-x5-repair'
$priorPath=Join-Path $repoPath 'measurements/research-next-20261001'
$buildPath=Join-Path $repoPath "out/x5-repair/$Config"
$configuration=if($Config -eq 'asan'){'Debug'}else{'Release'}
if($Tag -notmatch '^[a-zA-Z0-9-]{1,24}$'){throw 'invalid tag'}
function Bytes($path){[long](Get-ChildItem -LiteralPath $path -File -Recurse|Measure-Object Length -Sum).Sum}
function Hash($path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
function Save-New($name,$value){
    if($name -notmatch '^[a-zA-Z0-9_.-]+$'){throw 'output name'}
    $payload=[Text.UTF8Encoding]::new($false).GetBytes($value)
    $mutex=[Threading.Mutex]::new($false,'Local\PAS_X1ContactReplayBudget');$owned=$false
    try{
        $owned=$mutex.WaitOne(0);if(!$owned){throw 'batch_writer_busy'}
        if($payload.Length -gt 2097152 -or (Bytes $batchPath)+$payload.Length -gt 8388608 -or (Bytes $campaignPath)+(Bytes $priorPath)+$payload.Length -gt 8589934592){throw 'repair quota'}
        $file=[IO.File]::Open((Join-Path $batchPath $name),[IO.FileMode]::CreateNew,[IO.FileAccess]::Write)
        try{$file.Write($payload,0,$payload.Length)}finally{$file.Dispose()}
    }finally{if($owned){$mutex.ReleaseMutex()};$mutex.Dispose()}
}
function Run($name,$exe,[string[]]$arguments,[switch]$Json,[string]$ExpectedError=''){
    if(Test-Path -LiteralPath "$batchPath/$name-command.json"){throw 'command already exists'}
    # Reserve the entire maximum console plus command/XML budget before run.
    if((Bytes $batchPath)+2300000 -gt 8388608 -or (Bytes $campaignPath)+(Bytes $priorPath)+2300000 -gt 8589934592){throw 'insufficient invocation reserve'}
    $builder=[Text.StringBuilder]::new();$capturedBytes=0
    & $exe @arguments 2>&1 | ForEach-Object {
        $line=$_.ToString()+"`n";$capturedBytes+=[Text.Encoding]::UTF8.GetByteCount($line)
        if($capturedBytes -gt 2097152){throw 'console capacity; no silent truncation'}
        [void]$builder.Append($line)
    }
    $code=$LASTEXITCODE;$message=$builder.ToString()
    $extension=if($Json){'json'}else{'log'}
    Save-New "$name.$extension" $message
    Save-New "$name-command.json" (([ordered]@{exe=$exe;arguments=$arguments;exit=$code;output_sha256=(Hash "$batchPath/$name.$extension");ASAN_OPTIONS=$env:ASAN_OPTIONS}|ConvertTo-Json -Depth 8)+"`n")
    Write-Output "$name exit=$code bytes=$capturedBytes"
    if($ExpectedError){if($code -eq 0 -or !$message.Contains($ExpectedError)){throw 'unexpected negative result'}}
    elseif($code -ne 0){throw "$name failed; see preserved log"}
}
$name="$Mode-$Config-$Tag"
if($Mode -eq 'Configure'){
    $a=@('-S',"$repoPath/apps/frame_review/x5_offline",'-B',$buildPath,'-G','Visual Studio 18 2026','-A','x64','-T','v145','-DCMAKE_GENERATOR_INSTANCE=C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools,version=18.9.12105.275',"-DX1_SOURCE=$repoPath/out/x1/main50-v2","-DX1_REPO=$repoPath","-DCMAKE_PREFIX_PATH=$repoPath/out/vcpkg_installed/x64-windows")
    if($Config -eq 'asan'){$a+='-DX1_SANITIZER_SUPPORT=C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/include'}
    Run $name 'cmake' $a
}elseif($Mode -eq 'Build'){
    Run $name 'cmake' @('--build',$buildPath,'--config',$configuration,'--target','x5_report','x5_tests','x5_scope_report','--parallel','2')
    $dllDir=if($Config -eq 'asan'){"$repoPath/out/vcpkg_installed/x64-windows/debug/bin"}else{"$repoPath/out/vcpkg_installed/x64-windows/bin"}
    $z=if($Config -eq 'asan'){'zd.dll'}else{'z.dll'}
    Copy-Item -LiteralPath "$dllDir/$z","$dllDir/gtest.dll","$dllDir/gtest_main.dll" -Destination "$buildPath/$configuration"
}elseif($Mode -eq 'Tests'){
    $xml="$batchPath/$name.xml";if(Test-Path -LiteralPath $xml){throw 'XML exists'}
    Run $name "$buildPath/$configuration/x5_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$xml")
}elseif($Mode -eq 'Verify'){
    if($Config -eq 'asan'){$env:ASAN_OPTIONS='quarantine_size_mb=16:thread_local_quarantine_size_kb=64'}
    Run $name "$buildPath/$configuration/x5_report.exe" @("$campaignPath/early-role-x5/input-manifest.json","$campaignPath/early-role-x5/acceptance-report-1.json",'--verify-existing')
}elseif($Mode -eq 'Scope'){
    if($Config -eq 'asan'){$env:ASAN_OPTIONS='quarantine_size_mb=16:thread_local_quarantine_size_kb=64'}
    Run $name "$buildPath/$configuration/x5_scope_report.exe" @("$campaignPath/early-role-x5/acceptance-report-1.json") -Json
}elseif($Mode -eq 'Negative'){
    Run "$name-arguments" "$buildPath/$configuration/x5_scope_report.exe" @() -ExpectedError 'x5_scope_report accepted_report_json'
    Run "$name-wrong-parent" "$buildPath/$configuration/x5_scope_report.exe" @("$campaignPath/early-role-x5/input-manifest.json") -ExpectedError 'scope_parent_SHA'
}
