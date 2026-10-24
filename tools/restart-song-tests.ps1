[CmdletBinding()]
param(
    [string]$Configuration,
    [string]$ConfigurationSha256,
    [switch]$PlanOnly,
    [switch]$SelfTest,
    [switch]$LoadCoreOnly
)
$ErrorActionPreference='Stop'

# -SelfTest / -PlanOnly never show a window or call adb/emulator/touch_probe.
function Assert-RestartPin($Reference) {
    if($null -eq $Reference -or $Reference.file -isnot [string] -or
       -not [IO.Path]::IsPathFullyQualified($Reference.file) -or
       $Reference.sha256 -isnot [string] -or $Reference.sha256 -cnotmatch '^[a-f0-9]{64}$' -or
       -not (Test-Path -LiteralPath $Reference.file -PathType Leaf) -or
       (Get-FileHash -LiteralPath $Reference.file -Algorithm SHA256).Hash -ne $Reference.sha256){
        throw '重啟工具的固定檔案缺少或雜湊不同。'
    }
    return [IO.Path]::GetFullPath($Reference.file)
}
function Get-RestartConfiguration([string]$Path,[string]$ExpectedSha) {
    if($ExpectedSha -notmatch '^[a-fA-F0-9]{64}$' -or
       -not [IO.Path]::IsPathFullyQualified($Path) -or
       (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne $ExpectedSha){throw '重啟設定的 SHA256 不符。'}
    $value=Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json
    if($value.schema -cne 'pas.song-test-restart.v1' -or $value.serial -cne 'emulator-5554' -or $value.avd -cne 'phigros'){throw '重啟設定不是已知的 phigros AVD。'}
    foreach($key in @('python','worker','bootstrap_script','powershell','emulator','panel_config')){$null=Assert-RestartPin $value.$key}
    if([IO.Path]::GetFullPath($value.bootstrap_script.file) -ne [IO.Path]::GetFullPath($PSCommandPath)){throw '設定固定的是另一份重啟工具。'}
    return $value
}
function New-RestartWorkerInfo($Config,[string]$ConfigPath,[string]$ConfigSha,[string]$Mode,[string]$Session,[bool]$Consent=$false) {
    if($Mode -cnotin @('plan','preboot','boot','fixture')){throw 'Unknown restart stage.'}
    if($Mode -eq 'fixture' -and -not $Consent){throw 'Fixture requires explicit user consent.'}
    $info=[Diagnostics.ProcessStartInfo]::new()
    $info.FileName=$Config.python.file;$info.UseShellExecute=$false;$info.CreateNoWindow=$true
    $info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
    foreach($arg in @('-X','utf8',$Config.worker.file,'--configuration',$ConfigPath,'--configuration-sha256',$ConfigSha,'--mode',$Mode)){$info.ArgumentList.Add($arg)}
    if($Session){$info.ArgumentList.Add('--session');$info.ArgumentList.Add($Session)}
    if($Consent){$info.ArgumentList.Add('--fixture-consent')}
    return $info
}
function Request-RestartStop([string]$Session) {
    if(-not $Session -or -not (Test-Path -LiteralPath $Session -PathType Container)){return $false}
    [IO.File]::WriteAllText((Join-Path $Session 'STOP'),'user requested stop')
    return $true
}
function Wait-RestartOwnedProcess($Process,[scriptblock]$Tick,[scriptblock]$StopRequested,[scriptblock]$ForwardStop,$InitialFault=$null) {
    # Exception-safe ownership drain. A failed UI/progress/STOP write never means
    # that the child exited, and STOP is retried until its directory exists.
    $fault=$InitialFault;$sticky=($null -ne $fault)
    while($true){
        $exited=$false
        try{$exited=$Process.HasExited}catch{if($null -eq $fault){$fault=$_};$sticky=$true}
        if($exited){break}
        try{& $Tick}catch{if($null -eq $fault){$fault=$_};$sticky=$true}
        try{if(& $StopRequested){$sticky=$true}}catch{if($null -eq $fault){$fault=$_};$sticky=$true}
        if($sticky){try{$null=& $ForwardStop}catch{if($null -eq $fault){$fault=$_}}}
        try{Start-Sleep -Milliseconds 60}catch{if($null -eq $fault){$fault=$_};$sticky=$true}
    }
    return [pscustomobject]@{OwnerExited=$true;Fault=$fault;StopRequested=$sticky}
}
function Get-RestartPanelArguments([string]$Gui,[string]$ConfigPath) {
    foreach($part in @($Gui,$ConfigPath)){if(-not [IO.Path]::IsPathFullyQualified($part) -or $part -match '["%!\r\n]'){throw 'Panel path cannot be represented safely in the desktop launcher.'}}
    return '-NoProfile -STA -WindowStyle Hidden -ExecutionPolicy Bypass -File "'+$Gui+'" -Configuration "'+$ConfigPath+'"'
}
function Get-RestartKnownShortcut($Shortcut,$Config) {
    if([IO.Path]::GetFullPath($Shortcut.TargetPath) -ne [IO.Path]::GetFullPath($Config.powershell.file)){throw 'Desktop shortcut target is unrelated.'}
    if($Shortcut.Arguments -cnotmatch '^-NoProfile (?:-STA )?(?:-WindowStyle Hidden )?(?:-ExecutionPolicy Bypass )?-File "([^"]+)" -Configuration "([^"]+)"$'){throw 'Desktop shortcut arguments are not a known panel launch.'}
    $oldGui=[IO.Path]::GetFullPath($Matches[1]);$oldConfig=[IO.Path]::GetFullPath($Matches[2])
    $app=Split-Path -Parent $oldGui;$panelDir=Split-Path -Parent $app
    if((Split-Path -Leaf $oldGui) -cne 'sweep-panel.ps1' -or (Split-Path -Leaf $app) -cne 'app' -or
       (Split-Path -Leaf $panelDir) -cnotmatch '^song-panel-[0-9]{3}$' -or
       [IO.Path]::GetFullPath((Split-Path -Parent $panelDir)) -ne [IO.Path]::GetFullPath($Config.output_parent) -or
       $oldConfig -ne (Join-Path $panelDir 'launch-config.json') -or
       [IO.Path]::GetFullPath($Shortcut.WorkingDirectory) -ne $app){throw 'Desktop shortcut does not name a known sealed panel.'}
    return [pscustomobject]@{Gui=$oldGui;Configuration=$oldConfig}
}
function Update-RestartPanelShortcut($Config,[string]$Session,[string]$Gui,[string]$DerivedConfig) {
    $arguments=Get-RestartPanelArguments $Gui $DerivedConfig
    if($Config.powershell.file -match '["%!\r\n]'){throw 'PowerShell path is unsafe for a command launcher.'}
    $cmd=Join-Path $Session 'Open Panel.cmd'
    if(Test-Path -LiteralPath $cmd){throw 'Refuse to overwrite a session launcher.'}
    $content="@echo off`r`nchcp 65001 >nul`r`nstart `"`" /b `"$($Config.powershell.file)`" $arguments`r`n"
    [IO.File]::WriteAllText($cmd,$content,[Text.UTF8Encoding]::new($false))
    $report=[ordered]@{schema='pas.restart-panel-shortcut.v1';desktop_updated=$false;reason='no pinned known shortcut';
        fresh_launcher=[ordered]@{file=$cmd;sha256=(Get-FileHash -LiteralPath $cmd).Hash.ToLowerInvariant()};manual_selection=$true;manual_start=$true;invoked=$false}
    try{
        if($null -eq $Config.desktop_shortcut){throw 'No sealed desktop shortcut reference was supplied.'}
        $known=Join-Path ([Environment]::GetFolderPath('Desktop')) 'Phigros Test Panel.lnk'
        if([IO.Path]::GetFullPath($Config.desktop_shortcut.file) -ne [IO.Path]::GetFullPath($known)){throw 'Pinned desktop path is unrelated.'}
        $null=Assert-RestartPin $Config.desktop_shortcut
        $shell=New-Object -ComObject WScript.Shell
        $old=$shell.CreateShortcut($known);$oldIdentity=Get-RestartKnownShortcut $old $Config
        $oldPanel=Get-Content -LiteralPath $oldIdentity.Configuration -Raw -Encoding UTF8|ConvertFrom-Json
        if($oldPanel.gui_script.file -ne $oldIdentity.Gui){throw 'Old shortcut/config GUI mismatch.'}
        $null=Assert-RestartPin $oldPanel.gui_script
        $previous=Join-Path $Session 'DesktopShortcut.previous.lnk';$next=Join-Path $Session 'DesktopShortcut.next.lnk'
        if((Test-Path -LiteralPath $previous) -or (Test-Path -LiteralPath $next)){throw 'Session shortcut artifacts already exist.'}
        Copy-Item -LiteralPath $known -Destination $previous
        $new=$shell.CreateShortcut($next);$new.TargetPath=$Config.powershell.file;$new.Arguments=$arguments
        $new.WorkingDirectory=Split-Path -Parent $Gui;$new.WindowStyle=7;$new.Description='Manual Phigros song tests with this boot''s fresh fixture proof';$new.Save()
        $verify=$shell.CreateShortcut($next)
        if($verify.TargetPath -ne $Config.powershell.file -or $verify.Arguments -cne $arguments -or $verify.WorkingDirectory -ne (Split-Path -Parent $Gui)){throw 'New shortcut verification failed.'}
        # Recheck the named destination immediately before overwriting it. No
        # unrelated desktop link or original evidence directory is modified.
        $null=Assert-RestartPin $Config.desktop_shortcut
        Copy-Item -LiteralPath $next -Destination $known
        if((Get-FileHash -LiteralPath $known).Hash -ne (Get-FileHash -LiteralPath $next).Hash){throw 'Desktop copy verification failed; preserved previous/next links are in the session.'}
        $report.desktop_updated=$true;$report.reason='known previous hash/target/arguments verified; previous and next copies preserved'
        $report.previous=[ordered]@{file=$previous;sha256=(Get-FileHash -LiteralPath $previous).Hash.ToLowerInvariant()}
        $report.current=[ordered]@{file=$known;sha256=(Get-FileHash -LiteralPath $known).Hash.ToLowerInvariant()}
        $report.next=[ordered]@{file=$next;sha256=(Get-FileHash -LiteralPath $next).Hash.ToLowerInvariant()}
    }catch{$report.reason=$_.Exception.Message}
    $report|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $Session 'desktop-shortcut-update.json') -Encoding UTF8
    return [pscustomobject]$report
}
if($LoadCoreOnly){return}
if($SelfTest){
    $dummy=[pscustomobject]@{python=[pscustomobject]@{file='C:\fake path\python.exe'};worker=[pscustomobject]@{file='C:\fake path\worker.py'}}
    $checks=0
    foreach($mode in @('plan','preboot','boot')){
        $info=New-RestartWorkerInfo $dummy 'C:\fake path\config.json' ('a'*64) $mode 'C:\new session'
        if($info.UseShellExecute -or -not $info.CreateNoWindow -or -not $info.RedirectStandardError -or $info.ArgumentList.Contains('--fixture-consent')){throw 'Offline argument test failed.'};$checks++
    }
    $rejected=$false;try{$null=New-RestartWorkerInfo $dummy 'C:\c' ('a'*64) fixture 'C:\s'}catch{$rejected=$true}
    if(-not $rejected){throw 'Consent test failed.'};$checks++
    $info=New-RestartWorkerInfo $dummy 'C:\fake path\config.json' ('a'*64) fixture 'C:\new session' $true
    if(-not $info.ArgumentList.Contains('--fixture-consent') -or $info.ArgumentList.Count -ne 12){throw 'Fixture argument test failed.'};$checks++
    if(Request-RestartStop ''){throw 'STOP created an unowned directory.'};$checks++
    $args=Get-RestartPanelArguments 'C:\known panel\app\sweep-panel.ps1' 'C:\new session\launch-config.json'
    if($args -notmatch '^-NoProfile -STA -WindowStyle Hidden '){throw 'STA launcher failed.'};$checks++
    foreach($bad in @('C:\bad%path\x','C:\bad!path\x','relative.ps1')){
        $rejected=$false;try{$null=Get-RestartPanelArguments $bad 'C:\config.json'}catch{$rejected=$true};if(-not $rejected){throw 'Unsafe desktop argument accepted.'};$checks++
    }
    $oldGui='C:\assets\song-panel-003\app\sweep-panel.ps1';$oldCfg='C:\assets\song-panel-003\launch-config.json'
    $fakeConfig=[pscustomobject]@{powershell=[pscustomobject]@{file='C:\runtime\pwsh.exe'};output_parent='C:\assets'}
    $link=[pscustomobject]@{TargetPath='C:\runtime\pwsh.exe';Arguments=('-NoProfile -STA -WindowStyle Hidden -File "'+$oldGui+'" -Configuration "'+$oldCfg+'"');WorkingDirectory='C:\assets\song-panel-003\app'}
    $identity=Get-RestartKnownShortcut $link $fakeConfig
    if($identity.Gui -ne $oldGui -or $identity.Configuration -ne $oldCfg){throw 'Known shortcut parse failed.'};$checks++
    foreach($change in @(@('TargetPath','C:\other.exe'),@('WorkingDirectory','C:\other'),@('Arguments','-File evil.ps1'))){
        $badLink=[pscustomobject]@{TargetPath=$link.TargetPath;Arguments=$link.Arguments;WorkingDirectory=$link.WorkingDirectory};$badLink.($change[0])=$change[1]
        $rejected=$false;try{$null=Get-RestartKnownShortcut $badLink $fakeConfig}catch{$rejected=$true};if(-not $rejected){throw 'Unrelated desktop shortcut accepted.'};$checks++
    }
    $script:RestartTestPolls=0;$script:RestartTestStops=0;$script:RestartTestTicks=0
    $fakeOwner=[pscustomobject]@{}
    $fakeOwner|Add-Member ScriptProperty HasExited {$script:RestartTestPolls++;return $script:RestartTestPolls -ge 5}
    $drained=Wait-RestartOwnedProcess $fakeOwner {$script:RestartTestTicks++;throw 'progress I/O fault'} {$false} {$script:RestartTestStops++;if($script:RestartTestStops -eq 1){throw 'STOP I/O fault'}} 'async pipe setup fault'
    if(-not $drained.OwnerExited -or -not $drained.StopRequested -or $drained.Fault -ne 'async pipe setup fault' -or $script:RestartTestStops -lt 2){throw 'Exception ownership drain failed.'};$checks++
    if($script:RestartTestPolls -lt 5 -or $script:RestartTestTicks -lt 2){throw 'Owner was abandoned before exit.'};$checks++
    [pscustomobject]@{schema='pas.song-test-restart-selftest.v1';status='passed';checks=$checks;device_access=$false;processes_started=0;windows_shown=0}|ConvertTo-Json
    return
}
try{
$cfg=Get-RestartConfiguration $Configuration $ConfigurationSha256
$Configuration=[IO.Path]::GetFullPath($Configuration)
$restartConfigPath=$Configuration;$restartConfigSha=$ConfigurationSha256
# The pinned GUI's pure validator checks its entire consumed asset graph, not
# historical JSON provenance that has unrelated relative path bases.
$panel=Get-Content -LiteralPath $cfg.panel_config.file -Raw -Encoding UTF8|ConvertFrom-Json
$gui=Assert-RestartPin $panel.gui_script
. $gui -LoadCoreOnly
# Dot-sourced GUI parameters share this scope; keep the restart arguments under
# distinct names and restore them after loading its pure validation functions.
$Configuration=$restartConfigPath;$ConfigurationSha256=$restartConfigSha
$context=Get-PanelContext $cfg.panel_config.file $gui
Test-PanelPins $context
$planInfo=New-RestartWorkerInfo $cfg $Configuration $ConfigurationSha256 plan ''
$planProcess=[Diagnostics.Process]::new();$planProcess.StartInfo=$planInfo
$null=$planProcess.Start();$planOut=$planProcess.StandardOutput.ReadToEndAsync();$planErr=$planProcess.StandardError.ReadToEndAsync()
$planProcess.WaitForExit()
if($planProcess.ExitCode -ne 0){throw $planErr.GetAwaiter().GetResult()}
$plan=$planOut.GetAwaiter().GetResult()|ConvertFrom-Json
if($plan.status -cne 'offline_pins_verified' -or $plan.device_access -isnot [bool] -or $plan.device_access){throw 'Offline restart plan failed.'}
if($PlanOnly){$plan|ConvertTo-Json -Depth 10;return}
}catch{
    if($PlanOnly){throw}
    Add-Type -AssemblyName System.Windows.Forms
    [Windows.Forms.MessageBox]::Show("固定檔案驗證未通過，尚未啟動模擬器。`n`n$($_.Exception.Message)",'重啟設定無效',[Windows.Forms.MessageBoxButtons]::OK,[Windows.Forms.MessageBoxIcon]::Error)|Out-Null
    return
}

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
$answer=[Windows.Forms.MessageBox]::Show("這會冷開機啟動 phigros 模擬器，重新核對觸控路徑。請先關閉原本的模擬器與測試面板。`n`n稍後會另外詢問是否進行五指測試；不會自動開啟 Phigros 或按 Play。",'重新準備歌曲測試',[Windows.Forms.MessageBoxButtons]::OKCancel,[Windows.Forms.MessageBoxIcon]::Information)
if($answer -ne [Windows.Forms.DialogResult]::OK){return}
$session=Join-Path $cfg.output_parent ('song-test-restart-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8))
$null=New-Item -ItemType Directory -Path $session
$script:RestartStopped=$false;$script:RestartOwner=$null;$script:RestartCanClose=$false
$form=[Windows.Forms.Form]::new();$form.Text='重新準備歌曲測試';$form.ClientSize=[Drawing.Size]::new(610,195)
$form.StartPosition='CenterScreen';$form.FormBorderStyle='FixedDialog';$form.MaximizeBox=$false
$label=[Windows.Forms.Label]::new();$label.Location=[Drawing.Point]::new(22,22);$label.Size=[Drawing.Size]::new(565,95);$label.Text='核對封存…';$label.Font=[Drawing.Font]::new('Microsoft JhengHei UI',11)
$cancel=[Windows.Forms.Button]::new();$cancel.Text='停止並等待釋放';$cancel.Location=[Drawing.Point]::new(400,137);$cancel.Size=[Drawing.Size]::new(185,34)
$cancel.Add_Click({$script:RestartStopped=$true;$null=Request-RestartStop $session;$cancel.Enabled=$false;$label.Text='停止已要求。正在等待程序結束與觸控釋放，請保留此視窗。'})
$form.Add_FormClosing({param($sender,$eventArgs) if(-not $script:RestartCanClose){$eventArgs.Cancel=$true;$script:RestartStopped=$true;$null=Request-RestartStop $session;$label.Text='關閉已要求；等待程序安全結束。'}})
$form.Controls.AddRange(@($label,$cancel));$form.Show()
function Invoke-RestartWorker([string]$Mode,[bool]$Consent=$false){
    if($script:RestartStopped){throw 'user_stop'}
    $process=[Diagnostics.Process]::new();$process.StartInfo=New-RestartWorkerInfo $cfg $Configuration $ConfigurationSha256 $Mode $session $Consent
    $script:RestartOwner=$process
    $started=$false;$startFault=$null;$outTask=$null;$errTask=$null
    try{
        $started=$process.Start()
        if(-not $started){throw 'Worker process did not start.'}
        $outTask=$process.StandardOutput.ReadToEndAsync();$errTask=$process.StandardError.ReadToEndAsync()
    }catch{
        $startFault=$_
        # Even if asynchronous pipe setup threw, retain an actually started PID.
        try{if($process.Id -gt 0){$started=$true}}catch{}
    }
    if(-not $started){$script:RestartOwner=$null;throw $startFault}
    $tick={
        [Windows.Forms.Application]::DoEvents()
        $progress=Join-Path $session 'progress.json'
        if(Test-Path -LiteralPath $progress){$state=Get-Content -LiteralPath $progress -Raw -Encoding UTF8|ConvertFrom-Json;if($state.schema -ceq 'pas.song-test-restart-progress.v1'){$label.Text=$state.detail}}
    }
    try{
        $drain=Wait-RestartOwnedProcess $process $tick {$script:RestartStopped} {Request-RestartStop $session} $startFault
    }finally{
        # A future exception added around the regular monitor must also retain
        # ownership. The final drain retries STOP and never kills the worker.
        $null=Wait-RestartOwnedProcess $process {[Windows.Forms.Application]::DoEvents()} {$true} {Request-RestartStop $session}
        $script:RestartOwner=$null
    }
    # From here onward the worker has exited. Only now can exceptions escape and
    # the outer finally close the UI owner. Never call Kill/Terminate here.
    if($drain.StopRequested){$script:RestartStopped=$true}
    if($null -ne $drain.Fault){throw $drain.Fault}
    $stdout=$outTask.GetAwaiter().GetResult();$stderr=$errTask.GetAwaiter().GetResult()
    [IO.File]::WriteAllText((Join-Path $session ($Mode+'.worker.stdout.log')),$stdout)
    [IO.File]::WriteAllText((Join-Path $session ($Mode+'.worker.stderr.log')),$stderr)
    if($process.ExitCode -ne 0 -or $script:RestartStopped){throw ($stderr+' '+$(if($script:RestartStopped){'user_stop'}else{"stage failed: $Mode"}))}
}
try{
    if(@(Get-Process -Name 'emulator','qemu-system-x86_64' -ErrorAction SilentlyContinue).Count -gt 0){throw '請先關閉所有 Android 模擬器，再重新啟動此工具。'}
    Invoke-RestartWorker preboot
    $label.Text='冷開機啟動模擬器；等待 Android 就緒…';[Windows.Forms.Application]::DoEvents()
    # Only this explicit user-invoked path starts the emulator. Hidden prevents
    # a console flash; the emulator's normal application window remains its UI.
    $null=Start-Process -FilePath $cfg.emulator.file -ArgumentList @($cfg.emulator_arguments) -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $session 'emulator.stdout.log') -RedirectStandardError (Join-Path $session 'emulator.stderr.log')
    Invoke-RestartWorker boot
    $answer=[Windows.Forms.MessageBox]::Show($form,"開機與零接觸檢查已通過。接下來會開啟私人觸控測試畫面，送出五指 DOWN／MOVE／UP，並等待約 60 秒封存證據。`n`n請勿碰觸模擬器；此測試不進入遊戲。是否現在開始？",'確認五指測試',[Windows.Forms.MessageBoxButtons]::YesNo,[Windows.Forms.MessageBoxIcon]::Question)
    if($answer -ne [Windows.Forms.DialogResult]::Yes){$script:RestartStopped=$true;$null=Request-RestartStop $session;throw 'user_cancelled_before_fixture_input'}
    Invoke-RestartWorker fixture $true
    $result=Get-Content -LiteralPath (Join-Path $session 'result.json') -Raw -Encoding UTF8|ConvertFrom-Json
    if($result.status -cne 'fresh_fixture_verified'){throw 'Fresh evidence was not verified.'}
    $derived=Assert-RestartPin $result.launch_config
    $newContext=Get-PanelContext $derived $gui;Test-PanelPins $newContext
    [Windows.Forms.Application]::DoEvents();if($script:RestartStopped){throw 'user_stop'}
    $shortcut=Update-RestartPanelShortcut $cfg $session $gui $derived
    $label.Text='已取得這次開機的新五指與零接觸證據。'
    $shortcutMessage=if($shortcut.desktop_updated){'桌面的 Phigros Test Panel 已綁定這次開機。'}else{"原桌面捷徑未變動；新的入口：`n$($shortcut.fresh_launcher.file)"}
    $answer=[Windows.Forms.MessageBox]::Show($form,"五指路徑與最後零接觸已驗證。`n$shortcutMessage`n`n開啟面板後，請自行開啟 Phigros、選曲；面板顯示 READY 後再由您按遊戲 Play。`n`n現在開啟測試面板？",'準備完成',[Windows.Forms.MessageBoxButtons]::YesNo,[Windows.Forms.MessageBoxIcon]::Information)
    if($answer -eq [Windows.Forms.DialogResult]::Yes){
        # Revalidate source/build/chart refs immediately before opening the GUI.
        Test-PanelPins $newContext
        [Windows.Forms.Application]::DoEvents();if($script:RestartStopped){throw 'user_stop'}
        $arguments=Get-RestartPanelArguments $gui $derived
        $null=Start-Process -FilePath $cfg.powershell.file -ArgumentList $arguments -WindowStyle Hidden -PassThru
    }
}catch{
    $null=Request-RestartStop $session
    [Windows.Forms.MessageBox]::Show($form,"準備已停止，未開啟測試面板。`n`n$($_.Exception.Message)`n`n紀錄：$session",'準備未完成',[Windows.Forms.MessageBoxButtons]::OK,[Windows.Forms.MessageBoxIcon]::Warning)|Out-Null
}finally{
    $script:RestartCanClose=$true;$form.Close();$form.Dispose()
}
