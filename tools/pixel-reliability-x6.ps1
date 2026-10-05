param([ValidateSet('Configure','Build','Tests','Report','Negative')][string]$Mode,
      [ValidateSet('release','asan')][string]$Config='release',[string]$Tag='1')
$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$campaignPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue'
$batchPath=Join-Path $campaignPath 'pixel-reliability-x6'
$priorPath=Join-Path $repoPath 'measurements/research-next-20261001'
$buildPath=Join-Path $repoPath "out/x6/$Config"
$configuration=if($Config -eq 'asan'){'Debug'}else{'Release'}
if($Tag -notmatch '^[a-zA-Z0-9-]{1,24}$'){throw 'invalid tag'}
function Bytes($path){if(!(Test-Path -LiteralPath $path)){return 0};[long](Get-ChildItem -LiteralPath $path -File -Recurse|Measure-Object Length -Sum).Sum}
function Hash($path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
function Save-New($name,$value){
    $payload=[Text.UTF8Encoding]::new($false).GetBytes($value)
    $mutex=[Threading.Mutex]::new($false,'Local\PAS_X1ContactReplayBudget');$owned=$false
    try{$owned=$mutex.WaitOne(0);if(!$owned){throw 'batch_writer_busy'}
        if($payload.Length -gt 4194304 -or (Bytes $batchPath)+$payload.Length -gt 16777216 -or (Bytes $campaignPath)+(Bytes $priorPath)+$payload.Length -gt 8589934592){throw 'X6 quota'}
        $file=[IO.File]::Open((Join-Path $batchPath $name),[IO.FileMode]::CreateNew,[IO.FileAccess]::Write)
        try{$file.Write($payload,0,$payload.Length)}finally{$file.Dispose()}
    }finally{if($owned){$mutex.ReleaseMutex()};$mutex.Dispose()}
}
function Run($name,$exe,[string[]]$arguments,[switch]$Json,[string]$ExpectedError=''){
    if(Test-Path -LiteralPath "$batchPath/$name-command.json"){throw 'command already exists'}
    if((Bytes $batchPath)+4500000 -gt 16777216 -or (Bytes $campaignPath)+(Bytes $priorPath)+4500000 -gt 8589934592){throw 'invocation reserve'}
    $builder=[Text.StringBuilder]::new();$capturedBytes=0
    & $exe @arguments 2>&1 | ForEach-Object {$line=$_.ToString()+"`n";$capturedBytes+=[Text.Encoding]::UTF8.GetByteCount($line);if($capturedBytes -gt 4194304){throw 'console capacity'};[void]$builder.Append($line)}
    $code=$LASTEXITCODE;$message=$builder.ToString();$extension=if($Json){'json'}else{'log'}
    Save-New "$name.$extension" $message
    Save-New "$name-command.json" (([ordered]@{exe=$exe;arguments=$arguments;exit=$code;output_sha256=(Hash "$batchPath/$name.$extension");ASAN_OPTIONS=$env:ASAN_OPTIONS}|ConvertTo-Json -Depth 8)+"`n")
    Write-Output "$name exit=$code bytes=$capturedBytes"
    if($ExpectedError){if($code -eq 0 -or !$message.Contains($ExpectedError)){throw 'unexpected negative result'}}elseif($code -ne 0){throw "$name failed; preserved"}
}
$name="$Mode-$Config-$Tag"
if($Mode -in @('Configure','Build') -and (Test-Path -LiteralPath "$batchPath/final-summary.json")){throw 'frozen build: use a new batch/build root'}
if($Mode -eq 'Configure'){
    $a=@('-S',"$repoPath/apps/frame_review/x6_offline",'-B',$buildPath,'-G','Visual Studio 18 2026','-A','x64','-T','v145','-DCMAKE_GENERATOR_INSTANCE=C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools,version=18.9.12105.275',"-DX1_SOURCE=$repoPath/out/x1/main50-v2","-DX1_REPO=$repoPath","-DCMAKE_PREFIX_PATH=$repoPath/out/vcpkg_installed/x64-windows")
    if($Config -eq 'asan'){$a+='-DX1_SANITIZER_SUPPORT=C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.50.35717/include'}
    Run $name 'cmake' $a
}elseif($Mode -eq 'Build'){
    Run $name 'cmake' @('--build',$buildPath,'--config',$configuration,'--target','x6_report','x6_tests','--parallel','2')
    $dllDir=if($Config -eq 'asan'){"$repoPath/out/vcpkg_installed/x64-windows/debug/bin"}else{"$repoPath/out/vcpkg_installed/x64-windows/bin"};$z=if($Config -eq 'asan'){'zd.dll'}else{'z.dll'}
    Copy-Item -LiteralPath "$dllDir/$z","$dllDir/gtest.dll","$dllDir/gtest_main.dll" -Destination "$buildPath/$configuration"
    if((Bytes "$repoPath/out/x6") -gt 1073741824){throw 'build cap'}
}elseif($Mode -eq 'Tests'){
    $xml="$batchPath/$name.xml";if(Test-Path -LiteralPath $xml){throw 'XML exists'}
    Run $name "$buildPath/$configuration/x6_tests.exe" @('--gtest_brief=1',"--gtest_output=xml:$xml")
}elseif($Mode -eq 'Report'){
    if($Config -eq 'asan'){$env:ASAN_OPTIONS='quarantine_size_mb=16:thread_local_quarantine_size_kb=64'}
    Run $name "$buildPath/$configuration/x6_report.exe" @("$campaignPath/early-role-x5/acceptance-report-1.json") -Json
}elseif($Mode -eq 'Negative'){
    Run "$name-argc" "$buildPath/$configuration/x6_report.exe" @() -ExpectedError 'x6_report fixed_accepted_report'
    Run "$name-parent" "$buildPath/$configuration/x6_report.exe" @("$batchPath/manifest-before.json") -ExpectedError 'x6_parent_SHA'
}
