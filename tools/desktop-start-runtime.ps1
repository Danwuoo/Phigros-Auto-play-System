#Requires -Version 7.2
[CmdletBinding()]
param(
    [string]$Repository = 'C:\Users\wurre\Desktop\Phigros-Auto-play-System',
    [string]$ManualConfiguration,
    [ValidateRange(30,300)][int]$BootTimeoutSeconds = 180,
    [ValidateSet('none','dsound')][string]$EmulatorAudioBackend = 'none',
    [switch]$PlanOnly,
    [switch]$LoadCoreOnly,
    [switch]$InProcessPanel
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$Repository = [IO.Path]::GetFullPath($Repository)
$EmulatorAudioBackend = $EmulatorAudioBackend.ToLowerInvariant()
$sdk = Join-Path $env:LOCALAPPDATA 'Android\Sdk'
$adb = Join-Path $sdk 'platform-tools\adb.exe'
$emulator = Join-Path $sdk 'emulator\emulator.exe'
$catalog = Join-Path $PSScriptRoot 'start-song-library.ps1'
$library = Join-Path $Repository 'assets\phigros\4.0.1.2\library.json'
if (-not $ManualConfiguration) { $ManualConfiguration = Join-Path $Repository 'local\manual-launch.json' }
$ManualConfiguration = [IO.Path]::GetFullPath($ManualConfiguration)
$pwsh = Join-Path $PSHOME 'pwsh.exe'
$serial = 'emulator-5554'; $avd = 'phigros'; $package = 'com.PigeonGames.Phigros'
$emulatorArguments = @('-avd',$avd,'-port','5554','-grpc','8554','-grpc-use-token','-no-snapshot-load','-no-snapshot-save','-audio',$EmulatorAudioBackend)
function Get-PhigrosAudioStartupInfo([ValidateSet('none','dsound')][string]$Backend,[ValidateSet('planned_cold','cold','reuse_existing')][string]$BootMode) {
    $known=$BootMode -ne 'reuse_existing'
    $silent=if($known){$Backend -ceq 'none'}else{$null}
    $message=if(-not $known){'沿用已開啟模擬器的音訊設定。'}
        elseif($silent){'無聲模式：遊戲不由電腦喇叭播放聲音。'}else{'有聲模式：由電腦喇叭播放遊戲聲音。'}
    return [pscustomobject]@{requested_backend=$Backend;applied_backend=$(if($BootMode -ceq 'cold'){$Backend}else{$null})
        no_host_speaker_output=$silent;scope=$(if($BootMode -ceq 'planned_cold'){'planned_cold_start_only'}elseif($known){'cold_start_argument'}else{'existing_instance_not_changed_or_verified'})
        message=$message}
}
function Get-PhigrosLandscapePlan {
    return [pscustomobject]@{policy='cold-start-only-sensor-skin-landscape-lock1-v2';rotation=1;width=1280;height=720
        sensor_readback_arguments=@('-s',$serial,'emu','sensor','get','acceleration')
        clockwise_rotate_arguments=@('-s',$serial,'emu','rotate')
        maximum_clockwise_rotations=3;sensor_settle_attempts=3;sensor_settle_delay_ms=500
        set_arguments=@('-s',$serial,'shell','wm','user-rotation','lock','1')
        rotation_readback_arguments=@('-s',$serial,'shell','wm','user-rotation')
        geometry_readback_arguments=@('-s',$serial,'shell','dumpsys','input')
        reused_instance_rotation_changed=$false}
}
function Get-PhigrosAcceleration([string]$Text) {
    if($Text.Length -gt 4096 -or $Text -cmatch '(?m)^\s*(?:KO|ERROR)\b'){throw '模擬器方向感測器回應無效。'}
    $number='[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?'
    $pattern="(?m)^\s*acceleration\s*=\s*($number)\s*:\s*($number)\s*:\s*($number)\s*$"
    $sensorMatches=[regex]::Matches($Text,$pattern)
    if($sensorMatches.Count -ne 1 -or ([regex]::Replace($Text,$pattern,'') -replace '(?m)^\s*OK\s*$','').Trim().Length -ne 0){throw '無法唯一讀取模擬器方向感測器。'}
    $values=@(1..3|ForEach-Object {[double]::Parse($sensorMatches[0].Groups[$_].Value,[Globalization.CultureInfo]::InvariantCulture)})
    if(@($values|Where-Object {-not [double]::IsFinite($_)}).Count -ne 0){throw '模擬器方向感測器含無效數值。'}
    $x,$y,$z=$values;$gravity=[Math]::Sqrt($x*$x+$y*$y+$z*$z)
    if($gravity -lt 8 -or $gravity -gt 12){throw '模擬器重力感測器超出有效範圍。'}
    $skinRotation=if($x -ge 8 -and [Math]::Abs($y) -lt 2){3}
        elseif($x -le -8 -and [Math]::Abs($y) -lt 2){1}
        elseif($y -ge 8 -and [Math]::Abs($x) -lt 2){0}
        elseif($y -le -8 -and [Math]::Abs($x) -lt 2){2}else{$null}
    if($null -eq $skinRotation){throw '模擬器方向感測器尚未穩定於可確認的螢幕方向。'}
    return [pscustomobject]@{x=$x;y=$y;z=$z;gravity=$gravity;skin_rotation=$skinRotation;landscape_left=($skinRotation -eq 3)}
}
function Get-PhigrosLogicalSurface([string]$Text) {
    $surfaces=[Collections.Generic.List[string]]::new()
    foreach($line in ($Text -split '\r?\n')){
        if($line -match '\bdisplayId\s*=\s*0(?:,|\s)' -and $line -match '\borientation\s*=\s*1(?:,|\s)' -and
           $line -notmatch '\b(?:isActive|valid)\s*=\s*false'){
            $m=[regex]::Match($line,'logicalFrame\s*=\s*(?:\[\s*0,\s*0,\s*(\d+),\s*(\d+)\s*\]|Rect\(\s*0,\s*0\s*-\s*(\d+),\s*(\d+)\s*\))')
            if($m.Success){$w=$m.Groups[1].Value+$m.Groups[3].Value;$h=$m.Groups[2].Value+$m.Groups[4].Value;$surfaces.Add("${w}x${h}")}
        }
    }
    $orientation=[regex]::Match($Text,'(?m)^\s*SurfaceOrientation:\s*(\d+)\s*$')
    $width=[regex]::Match($Text,'(?m)^\s*SurfaceWidth:\s*(\d+)\s*$')
    $height=[regex]::Match($Text,'(?m)^\s*SurfaceHeight:\s*(\d+)\s*$')
    if($orientation.Success -and $orientation.Groups[1].Value -ceq '1' -and $width.Success -and $height.Success){
        $surfaces.Add($width.Groups[1].Value+'x'+$height.Groups[1].Value)
    }
    $unique=@($surfaces | Sort-Object -Unique)
    if($unique.Count -ne 1 -or $unique[0] -cne '1280x720'){throw '無法唯一確認主畫面為旋轉1、1280×720。'}
    return [pscustomobject]@{rotation=1;width=1280;height=720}
}
function Initialize-PhigrosColdLandscape([bool]$ColdStart) {
    if(-not $ColdStart){return [pscustomobject]@{policy='cold-start-only-sensor-skin-landscape-lock1-v2';applied=$false;geometry_verified=$null;sensor_verified=$null;scope='existing_instance_not_changed_or_verified'}}
    $orientation=Get-PhigrosLandscapePlan
    # WM lock alone rotates guest content but can leave the emulator skin upright
    # in portrait. Normalize physical sensor/skin first; initialOrientation is
    # ignored by some SDK cold boots. Never touch a reused, possibly playing AVD.
    $sensor=$null
    for($attempt=0;$attempt -lt $orientation.sensor_settle_attempts;$attempt++){
        try{$sensor=Get-PhigrosAcceleration (Invoke-SelectedAdb $orientation.sensor_readback_arguments 3000);break}
        catch{if($attempt -eq ($orientation.sensor_settle_attempts-1)){throw 'phigros 冷啟動後方向感測器不明；未旋轉或啟動遊戲。'}}
        Start-Sleep -Milliseconds $orientation.sensor_settle_delay_ms
    }
    $rotations=0
    while(-not $sensor.landscape_left -and $rotations -lt $orientation.maximum_clockwise_rotations){
        $expected=($sensor.skin_rotation+1)%4
        $response=Invoke-SelectedAdb $orientation.clockwise_rotate_arguments 3000
        if($response.Length -gt 4096 -or $response -cmatch '(?m)^\s*(?:KO|ERROR)\b'){throw '模擬器視窗旋轉失敗；未啟動遊戲。'}
        $rotations++
        $settled=$false
        for($attempt=0;$attempt -lt $orientation.sensor_settle_attempts;$attempt++){
            Start-Sleep -Milliseconds $orientation.sensor_settle_delay_ms
            try{
                $observed=Get-PhigrosAcceleration (Invoke-SelectedAdb $orientation.sensor_readback_arguments 3000)
                if($observed.skin_rotation -eq $expected){$sensor=$observed;$settled=$true;break}
            }catch{}
        }
        if(-not $settled){throw '模擬器視窗旋轉後未確認預期方向；未繼續旋轉或啟動遊戲。'}
    }
    if(-not $sensor.landscape_left){throw '模擬器視窗未能在旋轉次數限制內轉為橫向；未啟動遊戲。'}
    $null=Invoke-SelectedAdb $orientation.set_arguments 3000
    for($attempt=0;$attempt -lt 3;$attempt++){
        try{
            $rotation=Invoke-SelectedAdb $orientation.rotation_readback_arguments 3000
            if($rotation.Trim() -cnotmatch '^lock\s+1$'){throw '使用者旋轉鎖定未確認。'}
            $geometry=Get-PhigrosLogicalSurface (Invoke-SelectedAdb $orientation.geometry_readback_arguments 3000)
            return [pscustomobject]@{policy=$orientation.policy;applied=$true;geometry_verified=$true;sensor_verified=$true
                clockwise_rotations=$rotations;acceleration=$sensor;rotation=$geometry.rotation
                width=$geometry.width;height=$geometry.height;scope='cold_start_before_game_launch'}
        }catch{if($attempt -eq 2){throw 'phigros 冷啟動後無法確認1280×720橫向畫面；未啟動遊戲。'}}
        Start-Sleep -Milliseconds 200
    }
}
function Get-PhigrosEmulatorProcesses {
    return @(Get-CimInstance Win32_Process | Where-Object {
        $_.Name -match '^(emulator|qemu-system[^.]*)\.exe$' -and
        $_.CommandLine -match '(?:^|\s)(?:-avd\s+(?:"phigros"|phigros)|"@phigros"|@phigros)(?=\s|$)'
    })
}
function Get-PhigrosBootOutcome([bool]$BootCompleted,[bool]$TargetRunning,[bool]$LauncherExited,[bool]$DeadlineExpired) {
    if($BootCompleted){return 'ready'}
    # A wrapper may exit while its QEMU child continues. Only report an exit
    # when no process for this exact AVD remains.
    if($LauncherExited -and -not $TargetRunning){return 'emulator_exited'}
    if($DeadlineExpired){return 'timeout'}
    return 'waiting'
}
function Get-PhigrosBootFailureMessage([string]$SessionDirectory) {
    $diagnostics=foreach($name in @('emulator.stderr.log','emulator.stdout.log')){
        $path=Join-Path $SessionDirectory $name
        if(Test-Path -LiteralPath $path -PathType Leaf){Get-Content -LiteralPath $path -Tail 100}
    }
    # Map known failures to fixed messages; never copy arbitrary log contents
    # (which can include authentication metadata) into UI/status output.
    $text=$diagnostics -join "`n"
    if($text -match "(?i)Failed to load snapshot|error while loading state"){
        return "phigros 模擬器還原舊快照失敗並已退出；此入口已改用冷啟動。`n本次記錄：$SessionDirectory"
    }
    if($text -match '(?i)WHPX:.*Unexpected VP exit'){
        return "phigros 模擬器的 WHPX 程序已提前退出。`n本次記錄：$SessionDirectory"
    }
    return "phigros 模擬器程序已提前退出，無法完成開機。`n本次記錄：$SessionDirectory"
}
function Get-PhigrosFrontend {
    if(-not(Test-Path -LiteralPath $ManualConfiguration -PathType Leaf)){
        return [pscustomobject]@{Kind='catalog';Script=$catalog;Arguments=@('-NoProfile','-STA','-File',$catalog,'-Library',$library);ConfigurationSha256=$null;CapacityPolicy=$null}
    }
    $configFile=Get-Item -LiteralPath $ManualConfiguration
    if($configFile.Length -gt 1048576){throw '人工模式設定超出大小限制。'}
    $configuration=[IO.File]::ReadAllText($ManualConfiguration,[Text.Encoding]::UTF8)|ConvertFrom-Json -Depth 100
    if($configuration.schema -cne 'pas.manual-song-panel-launch.v2' -or
        $configuration.manual_selection -isnot [bool] -or -not $configuration.manual_selection -or
        $configuration.manual_start -isnot [bool] -or -not $configuration.manual_start -or
        $configuration.image_retention -cne 'result'){
        throw '人工模式設定必須保留人工選曲、人工 Play，並僅保存結算圖。'
    }
    $pin=$configuration.gui_script
    if($pin.file -isnot [string] -or -not[IO.Path]::IsPathFullyQualified($pin.file) -or
        $pin.sha256 -cnotmatch '^[a-fA-F0-9]{64}$'){
        throw '人工面板缺少完整 GUI 路徑與 SHA256。'
    }
    $expectedGui=[IO.Path]::GetFullPath((Join-Path $Repository 'tools\sweep-panel.ps1'))
    if($configuration.PSObject.Properties['frontend_release']){
        # The desktop dispatcher verifies every member before this code runs.
        # GUI and its dot-sourced core must come from that same immutable bundle.
        $releaseRoot=Split-Path -Parent $configuration.frontend_release.file
        $expectedGui=[IO.Path]::GetFullPath((Join-Path $releaseRoot 'tools\sweep-panel.ps1'))
    }
    $actualGui=[IO.Path]::GetFullPath($pin.file)
    if(-not $actualGui.Equals($expectedGui,[StringComparison]::OrdinalIgnoreCase) -or
        -not(Test-Path -LiteralPath $actualGui -PathType Leaf)){
        throw '人工設定必須綁定已安裝版本的完整曲庫面板。'
    }
    if((Get-FileHash -LiteralPath $actualGui -Algorithm SHA256).Hash -ine $pin.sha256){
        throw '已安裝的曲庫面板檔案不完整；請重新安裝桌面介面版本。'
    }
    $capacityPolicy=if($configuration.PSObject.Properties['capacity_policy'] -and
        $configuration.capacity_policy -ceq 'configured-ten-contact-v1'){'configured-ten-contact-v1'}else{$null}
    return [pscustomobject]@{Kind='manual';Script=$actualGui
        Arguments=@('-NoProfile','-STA','-File',$actualGui,'-Configuration',$ManualConfiguration)
        ConfigurationSha256=(Get-FileHash -LiteralPath $ManualConfiguration -Algorithm SHA256).Hash.ToLowerInvariant()
        CapacityPolicy=$capacityPolicy}
}
function Initialize-PhigrosObserverAccess(
    [string]$LocalHelper=(Join-Path $Repository 'local\runtime-support\observer-1bee0d00\snapshot-helper')
) {
    # Google Play images use an unprivileged shell with the input group. Check
    # the actual read-only capability; UID 0 and a touch fixture are unnecessary.
    $expected='1bee0d00ef2e69bbad846729ddd73e27b8c61b48cd61e4c03ffc2ba897f77ccb'
    $remote='/data/local/tmp/pas-evdev-slots-1bee0d00ef2e69bb'
    if(-not(Test-Path -LiteralPath $LocalHelper -PathType Leaf) -or
       (Get-FileHash -LiteralPath $LocalHelper -Algorithm SHA256).Hash -ine $expected){
        throw '唯讀觀察器的本機檔案缺失或 SHA256 不符。'
    }
    $readDigest={param([string]$RemotePath)
        try{
            $parts=(Invoke-SelectedAdb @('-s',$serial,'shell','sha256sum',$RemotePath) 3000) -split '\s+'
            if($parts.Count -eq 2 -and $parts[0] -cmatch '^[a-f0-9]{64}$' -and $parts[1] -ceq $RemotePath){return $parts[0]}
        }catch{}
        return $null
    }
    $mode='already_present'
    if((& $readDigest $remote) -cne $expected){
        $temporary=$remote+'.install-'+[Guid]::NewGuid().ToString('N')
        $renamed=$false
        try{
            $null=Invoke-SelectedAdb @('-s',$serial,'push',$LocalHelper,$temporary) 15000
            $null=Invoke-SelectedAdb @('-s',$serial,'shell','chmod','700',$temporary) 3000
            if((& $readDigest $temporary) -cne $expected){throw '上傳的唯讀觀察器 SHA256 不符。'}
            $null=Invoke-SelectedAdb @('-s',$serial,'shell','mv','-f',$temporary,$remote) 3000
            $renamed=$true
            if((& $readDigest $remote) -cne $expected){throw '安裝後的唯讀觀察器 SHA256 不符。'}
            $mode='installed'
        }finally{
            if(-not $renamed){try{$null=Invoke-SelectedAdb @('-s',$serial,'shell','rm','-f',$temporary) 3000}catch{}}
        }
    }
    $boot=(Invoke-SelectedAdb @('-s',$serial,'shell','cat','/proc/sys/kernel/random/boot_id') 3000).Trim()
    if($boot -cnotmatch '^[a-f0-9]{8}(?:-[a-f0-9]{4}){3}-[a-f0-9]{12}$'){throw '無法取得目前模擬器的開機身份。'}
    $raw=Invoke-SelectedAdb @('-s',$serial,'shell',$remote,'/dev/input/event2') 3000
    if($raw.Length -gt 8192){throw '唯讀觀察器回應超出大小限制。'}
    $snapshot=$raw|ConvertFrom-Json -Depth 16
    $integer={param($Value,[long]$Low,[long]$High) (($Value -is [int]) -or ($Value -is [long])) -and $Value -ge $Low -and $Value -le $High}
    if($snapshot.schema -cne 'pas.evdev-slot-snapshot.v1' -or $snapshot.boot_id -cne $boot -or
       $snapshot.path -cne '/dev/input/event2' -or $snapshot.name -cne 'virtio_input_multi_touch_1' -or
       -not(& $integer $snapshot.rdev 3394 3394) -or -not(& $integer $snapshot.slot_count 11 11) -or
       -not(& $integer $snapshot.error_errno 0 0) -or -not(& $integer $snapshot.active_count 0 11) -or
       $snapshot.input_enabled -isnot [bool] -or $snapshot.input_enabled -or
       $snapshot.grab_enabled -isnot [bool] -or $snapshot.grab_enabled){
        throw '唯讀觸控觀察器無法取得目前裝置資料；請查看本次啟動記錄。'
    }
    # Active pointers do not prevent opening the library. Native READY obtains
    # its own fresh zero-contact snapshot before any playback input.
    return [pscustomobject]@{mode=$mode;helper=$remote;helper_sha256=$expected;boot_id=$boot
        path=$snapshot.path;active_count=$snapshot.active_count;root_required=$false
        input_enabled=$false;capacity_test_performed=$false;scope='read-only observer availability only'}
}
if($LoadCoreOnly){return}
$plan = [ordered]@{
    schema='pas.desktop-manual-start.v1'; repository=$Repository; adb=$adb; emulator=$emulator
    serial=$serial; avd=$avd; grpc_target='127.0.0.1:8554'; emulator_arguments=$emulatorArguments
    package=$package; library=$library; manual_configuration=$ManualConfiguration
    manual_configuration_available=(Test-Path -LiteralPath $ManualConfiguration -PathType Leaf)
    image_retention='result'; automatic_song_selection=$false; automatic_play=$false
    input_enabled=$false; device_access=(-not $PlanOnly)
    boot_policy='reuse_existing_or_cold_start_without_snapshots'
    audio_backend=(Get-PhigrosAudioStartupInfo $EmulatorAudioBackend 'planned_cold')
    landscape_normalization_plan=(Get-PhigrosLandscapePlan)
}
if ($PlanOnly) {
    $frontend=Get-PhigrosFrontend
    $plan['frontend']=$frontend.Kind; $plan['panel_script']=$frontend.Script
    $plan['panel_arguments']=$frontend.Arguments; $plan['manual_configuration_sha256']=$frontend.ConfigurationSha256
    $plan['boot_verification']='not_requested_offline'
    $plan['capacity_policy']=$frontend.CapacityPolicy
    $plan['current_boot_capacity_verification_required']=$false
    $plan['fixture_verification_automatically_run']=$false
    $plan['observer_access']='deploy pinned read-only helper and use shell input-group access; no root or touch fixture'
    $plan['capacity_scope']='Launcher metadata only; configured capacity is not a claim of a new current-boot fixture test.'
    $plan | ConvertTo-Json -Depth 8; exit 0
}
foreach ($path in @($adb,$emulator,$catalog,$library,$pwsh)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "啟動所需檔案不存在：$path" }
}
function Invoke-SelectedAdb([string[]]$Arguments,[int]$TimeoutMilliseconds=10000) {
    $info=[Diagnostics.ProcessStartInfo]::new($adb)
    $info.UseShellExecute=$false; $info.CreateNoWindow=$true
    $info.RedirectStandardOutput=$true; $info.RedirectStandardError=$true
    foreach ($argument in $Arguments) { $info.ArgumentList.Add($argument) }
    $process=[Diagnostics.Process]::new(); $process.StartInfo=$info
    try {
        if (-not $process.Start()) { throw 'ADB 無法啟動。' }
        $out=$process.StandardOutput.ReadToEndAsync(); $err=$process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit($TimeoutMilliseconds)) {
            $process.Kill(); $process.WaitForExit(); throw 'ADB 操作逾時。'
        }
        $stdout=$out.GetAwaiter().GetResult(); $stderr=$err.GetAwaiter().GetResult()
        if ($stdout.Length -gt 1048576 -or $stderr.Length -gt 1048576) { throw 'ADB 回應超出範圍。' }
        if ($process.ExitCode -ne 0) { throw "ADB 操作失敗：$($stderr.Trim())" }
        return $stdout.Trim()
    } finally { $process.Dispose() }
}
$statusDirectory=Join-Path $Repository 'local\startup'
$null=New-Item -ItemType Directory -Force -Path $statusDirectory
$session=Join-Path $statusDirectory (Get-Date -Format 'yyyyMMdd-HHmmss-fff')
$null=New-Item -ItemType Directory -Path $session
$statusFile=Join-Path $Repository 'local\startup-status.json'
$sessionStatusFile=Join-Path $session 'status.json'
$launchId=Split-Path -Leaf $session
$status=[ordered]@{
    schema='pas.desktop-manual-status.v1'; status='starting'; launch_id=$launchId; started_utc=[DateTime]::UtcNow.ToString('o'); serial=$serial; avd=$avd; package=$package
    image_retention='result'; automatic_song_selection=$false; automatic_play=$false; input_enabled=$false
    manual_configuration=$ManualConfiguration; manual_configuration_available=$false
    message='正在連接 phigros 模擬器與遊戲。'; session=$session
    boot_mode='reuse_existing'; emulator_arguments=$emulatorArguments
    audio_backend=(Get-PhigrosAudioStartupInfo $EmulatorAudioBackend 'reuse_existing')
    landscape_normalization=(Initialize-PhigrosColdLandscape $false)
}
function Save-StartupStatus {
    $json=$status | ConvertTo-Json -Depth 8
    $json | Set-Content -LiteralPath $statusFile -Encoding utf8
    $json | Set-Content -LiteralPath $sessionStatusFile -Encoding utf8
}
Save-StartupStatus
$bootProcess=$null
try {
    # Reject an incomplete install before starting ADB, the emulator or game.
    $frontend=Get-PhigrosFrontend
    $null=Invoke-SelectedAdb @('start-server')
    $online=$false
    try { $online=(Invoke-SelectedAdb @('-s',$serial,'get-state')) -eq 'device' } catch { }
    if (-not $online) {
        $other=@(Get-PhigrosEmulatorProcesses)
        if (-not $other) {
            $bootProcess=Start-Process -FilePath $emulator -ArgumentList $emulatorArguments -WindowStyle Hidden -PassThru `
                -RedirectStandardOutput (Join-Path $session 'emulator.stdout.log') `
                -RedirectStandardError (Join-Path $session 'emulator.stderr.log')
            $status['emulator_process_id']=$bootProcess.Id
            $status.boot_mode='cold'
            $status.audio_backend=Get-PhigrosAudioStartupInfo $EmulatorAudioBackend 'cold'
            Save-StartupStatus
        }
    }
    $deadline=[DateTime]::UtcNow.AddSeconds($BootTimeoutSeconds)
    do {
        $booted=$false
        try {
            $online=(Invoke-SelectedAdb @('-s',$serial,'get-state') 3000) -eq 'device'
            if ($online) { $booted=(Invoke-SelectedAdb @('-s',$serial,'shell','getprop','sys.boot_completed') 3000) -eq '1' }
        } catch { }
        $outcome='ready'
        if(-not $booted){
            $targetRunning=@(Get-PhigrosEmulatorProcesses).Count -gt 0
            $launcherExited=$false
            if($null -ne $bootProcess){$bootProcess.Refresh();$launcherExited=$bootProcess.HasExited}
            else{$launcherExited=-not $targetRunning}
            $outcome=Get-PhigrosBootOutcome $booted $targetRunning $launcherExited ([DateTime]::UtcNow -ge $deadline)
        }
        if($outcome -eq 'ready'){break}
        if($outcome -eq 'emulator_exited'){
            $status['failure_stage']='emulator_process_exited'
            if($null -ne $bootProcess -and $bootProcess.HasExited){$status['emulator_exit_code']=$bootProcess.ExitCode}
            throw (Get-PhigrosBootFailureMessage $session)
        }
        if($outcome -eq 'timeout'){
            $status['failure_stage']='android_boot_timeout'
            throw "phigros 模擬器仍在執行，但 Android 開機逾時。`n本次記錄：$session"
        }
        Start-Sleep -Milliseconds 1000
    } while ($true)
    $name=(Invoke-SelectedAdb @('-s',$serial,'emu','avd','name')).Split([char]10)[0].Trim()
    if ($name -cne $avd) { throw 'emulator-5554 不是指定的 phigros；未啟動遊戲。' }
    if($status.boot_mode -ceq 'cold'){$status['failure_stage']='cold_start_landscape'}
    $status.landscape_normalization=Initialize-PhigrosColdLandscape ($status.boot_mode -ceq 'cold')
    $null=$status.Remove('failure_stage')
    $status['observer_access']=Initialize-PhigrosObserverAccess
    Save-StartupStatus
    $packagePath=Invoke-SelectedAdb @('-s',$serial,'shell','pm','path',$package)
    if (-not $packagePath.StartsWith('package:')) { throw 'phigros 模擬器內未安裝 Phigros。' }
    $activity=Invoke-SelectedAdb @('-s',$serial,'shell','cmd','package','resolve-activity','--brief','-a','android.intent.action.MAIN','-c','android.intent.category.LAUNCHER',$package)
    $component=@($activity.Split([char]10) | ForEach-Object {$_.Trim()} | Where-Object {$_ -match '^com\.PigeonGames\.Phigros/[A-Za-z0-9_.$]+$'})
    if ($component.Count -ne 1) { throw 'Phigros 的啟動 Activity 無法唯一確認。' }
    $null=Invoke-SelectedAdb @('-s',$serial,'shell','am','start','-n',$component[0]) 15000
    $status.manual_configuration_available=$frontend.Kind -ceq 'manual'
    $status['frontend']=$frontend.Kind; $status['panel_script']=$frontend.Script
    $status['manual_configuration_sha256']=$frontend.ConfigurationSha256
    $status['capacity_policy']=$frontend.CapacityPolicy
    $status['current_boot_capacity_verification_required']=$false
    $status['fixture_verification_automatically_run']=$false
    $panelArguments=$frontend.Arguments
    if ($status.manual_configuration_available) {
        $status.status='manual_panel_opened'; $status.message=$status.audio_backend.message+' 請自行選曲；面板確認 READY 後再按遊戲 Play。每輪只保存結算圖。'
    } else {
        $status.status='catalog_opened_calibration_required'
        $status.message='遊戲與曲庫已啟動。人工演奏設定尚待真實 IN／AT 開場、暫停、結算校正與原生profile準備完成。'
    }
    Save-StartupStatus
    $env:PHIGROS_MANUAL_STATUS_FILE=$statusFile
    $env:PHIGROS_DESKTOP_LAUNCH_ID=$launchId
    $env:PHIGROS_EMULATOR_AUDIO_MODE=if($status.audio_backend.applied_backend){$status.audio_backend.applied_backend}else{'reuse_existing'}
    # Native PAS discovery must resolve the same SDK/ADB used by this launcher,
    # even when an older SDK is present in the desktop process environment.
    $env:ANDROID_SDK_ROOT=$sdk
    $env:ANDROID_HOME=$sdk
    if($InProcessPanel){
        $status['panel_process_id']=$PID
        $status['panel_host']='in-process-STA'
        Save-StartupStatus
        # Preserve the frontend's original fresh-PowerShell strictness while
        # allowing a native STA desktop host to own its managed main window.
        & {
            param($FrontendScript,$FrontendKind,$ConfigurationPath,$LibraryPath)
            Set-StrictMode -Off
            if($FrontendKind -ceq 'manual'){& $FrontendScript -Configuration $ConfigurationPath}
            else{& $FrontendScript -Library $LibraryPath}
        } $frontend.Script $frontend.Kind $ManualConfiguration $library
        return
    }
    $info=[Diagnostics.ProcessStartInfo]::new($pwsh)
    $info.UseShellExecute=$false; $info.CreateNoWindow=$true; $info.WorkingDirectory=$Repository
    foreach ($argument in $panelArguments) { $info.ArgumentList.Add($argument) }
    $panel=[Diagnostics.Process]::Start($info)
    if (-not $panel) { throw '曲庫視窗無法啟動。' }
    $status['panel_process_id']=$panel.Id; Save-StartupStatus
    $panel.Dispose()
} catch {
    $status.status='failed'; $status.message=$_.Exception.Message; Save-StartupStatus
    Add-Type -AssemblyName System.Windows.Forms
    [Windows.Forms.MessageBox]::Show($status.message,'Phigros 啟動失敗',[Windows.Forms.MessageBoxButtons]::OK,[Windows.Forms.MessageBoxIcon]::Error) | Out-Null
    throw
} finally {
    if($null -ne $bootProcess){$bootProcess.Dispose()}
}
