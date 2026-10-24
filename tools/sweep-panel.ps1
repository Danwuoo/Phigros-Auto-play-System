#Requires -Version 7.2
[CmdletBinding()]
param(
    [string]$Configuration,
    [switch]$SelfTest,
    [string]$RenderPreview,
    [ValidateSet('default','waiting_for_manual_play','waiting_for_valid_HUD','waiting_for_first_note','playing','input_complete_result_unconfirmed','result_confirmed')][string]$PreviewStage='default',
    [switch]$LoadCoreOnly
)
$ErrorActionPreference = 'Stop'

# Event handlers run entirely in C#: PowerShell delegates on a background
# reader thread have no runspace. This helper never starts anything itself.
if (-not ('PasSweep.NativeOperation' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Text;
using System.Diagnostics;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Security.Cryptography;
using System.Threading;
namespace PasSweep {
 public sealed class Notice {
  public string Kind, Text; public int Count;
  public Notice(string kind,string text,int count=0){Kind=kind;Text=text;Count=count;}
 }
 public sealed class Pin { public string File, Sha256; public Pin(string file,string sha){File=file;Sha256=sha;} }
 public sealed class HashOperation {
  public readonly ConcurrentQueue<Notice> Queue=new ConcurrentQueue<Notice>();
  volatile bool cancel; public volatile bool Done; public bool Success; public string Error="";
  public void Cancel(){cancel=true;}
  public void Start(Pin[] pins){ThreadPool.QueueUserWorkItem(_=>{
   try {int count=0;foreach(var pin in pins){
    if(cancel){Queue.Enqueue(new Notice("cancelled","已取消檔案核對"));return;}
    using(var input=new FileStream(pin.File,FileMode.Open,FileAccess.Read,FileShare.Read)){
     var actual=Convert.ToHexString(SHA256.HashData(input));
     if(!String.Equals(actual,pin.Sha256,StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("檔案雜湊不符："+pin.File);
    }
    Queue.Enqueue(new Notice("hash",pin.File,++count));
   } Success=true;Queue.Enqueue(new Notice("hash_done","檔案核對完成",count));
   }catch(Exception e){Error=e.Message;Queue.Enqueue(new Notice("error",Error));}finally{Done=true;}
  });}
 }
 public sealed class NativeOperation:IDisposable {
  public readonly ConcurrentQueue<Notice> Queue=new ConcurrentQueue<Notice>();
  readonly object gate=new object(); Process process; StreamWriter output,error;
  volatile bool outputEnd,errorEnd; int queued; long bytes; public int DroppedLines; public bool LogLimit; public volatile string LogError=""; public bool Started;
  const int QueueLimit=512; const long LogBytesLimit=32*1024*1024;
  public static ProcessStartInfo BuildStartInfo(string executable,string cwd,string[] args){
   var info=new ProcessStartInfo(executable){WorkingDirectory=cwd,UseShellExecute=false,CreateNoWindow=true,
    RedirectStandardOutput=true,RedirectStandardError=true,StandardOutputEncoding=new UTF8Encoding(false),StandardErrorEncoding=new UTF8Encoding(false)};
   foreach(var arg in args)info.ArgumentList.Add(arg);return info;
  }
  void Line(bool stderr,string text){
   if(text==null){if(stderr)errorEnd=true;else outputEnd=true;return;}
   var bounded=text.Length>32768?text.Substring(0,32768)+" [截斷]":text;
   lock(gate){
    bytes+=Encoding.UTF8.GetByteCount(bounded)+1;
    try {if(bytes<=LogBytesLimit){(stderr?error:output).WriteLine(bounded);(stderr?error:output).Flush();}else LogLimit=true;}
    catch(Exception e){LogError=e.Message;}
   }
   var display=bounded.Length>2048?bounded.Substring(0,2048)+" [詳細請開啟紀錄]":bounded;
   if(Interlocked.Increment(ref queued)<=QueueLimit)Queue.Enqueue(new Notice(stderr?"stderr":"stdout",display));
   else {Interlocked.Decrement(ref queued);Interlocked.Increment(ref DroppedLines);}
  }
  public void Start(string executable,string cwd,string[] args,string stdoutFile,string stderrFile){
   if(process!=null)throw new InvalidOperationException("已有執行中的程序");
   bool outReading=false,errReading=false;
   try {output=new StreamWriter(stdoutFile,false,new UTF8Encoding(false));error=new StreamWriter(stderrFile,false,new UTF8Encoding(false));
    process=new Process{StartInfo=BuildStartInfo(executable,cwd,args)};
    process.OutputDataReceived+=(s,e)=>Line(false,e.Data);process.ErrorDataReceived+=(s,e)=>Line(true,e.Data);
    if(!process.Start())throw new InvalidOperationException("無法啟動封存程式");
    Started=true;process.BeginOutputReadLine();outReading=true;process.BeginErrorReadLine();errReading=true;
   }catch{
    if(Started){
     // Preserve ownership and drain a pipe whose async reader failed to start.
     if(!outReading)ThreadPool.QueueUserWorkItem(_=>Drain(process.StandardOutput,false));
     if(!errReading)ThreadPool.QueueUserWorkItem(_=>Drain(process.StandardError,true));
    }else{if(output!=null)output.Dispose();if(error!=null)error.Dispose();if(process!=null)process.Dispose();process=null;}
    throw;
   }
  }
  void Drain(StreamReader reader,bool stderr){try{string line;while((line=reader.ReadLine())!=null)Line(stderr,line);}catch(Exception e){LogError=e.Message;}finally{Line(stderr,null);}}
  public bool TryRead(out Notice notice){if(Queue.TryDequeue(out notice)){Interlocked.Decrement(ref queued);return true;}return false;}
  public bool Complete{get{return Started&&process!=null&&process.HasExited&&outputEnd&&errorEnd;}}
  public int ExitCode{get{if(!Complete)throw new InvalidOperationException("程序尚未完成");return process.ExitCode;}}
  public void Dispose(){
   if(Started&&process!=null&&!Complete)throw new InvalidOperationException("執行中程序必須等待 STOP 與自然結束");
   lock(gate){if(output!=null)output.Dispose();if(error!=null)error.Dispose();}if(process!=null)process.Dispose();
  }
 }
}
'@
}

function Read-PanelJson([string]$Path,[long]$Limit=8388608) {
    if (-not [IO.Path]::IsPathFullyQualified($Path)) { throw "必須使用完整路徑：$Path" }
    $item=Get-Item -LiteralPath $Path -ErrorAction Stop
    if ($item.PSIsContainer -or $item.Length -gt $Limit) { throw "檔案超出大小限制：$Path" }
    return [IO.File]::ReadAllText($item.FullName,[Text.Encoding]::UTF8) | ConvertFrom-Json -Depth 100
}
function Assert-PanelText($Value,[string]$Name) {
    if ($Value -isnot [string] -or $Value.Length -lt 1 -or $Value.Length -gt 512 -or $Value -match '[\x00-\x1f]') { throw "無效的 $Name" }
}
function Add-PanelPin($Map,$Reference,[string]$Base='') {
    if ($null -eq $Reference -or $Reference.file -isnot [string] -or $Reference.sha256 -notmatch '^[a-fA-F0-9]{64}$') { throw '缺少完整的檔案與 SHA256 身分。' }
    $path=$Reference.file
    if (-not [IO.Path]::IsPathFullyQualified($path)) {
        if (-not $Base) { throw "必須使用完整資產路徑：$path" }
        $path=Join-Path $Base $path
    }
    $path=[IO.Path]::GetFullPath($path)
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "找不到資產：$path" }
    if ($Map.ContainsKey($path) -and $Map[$path] -ne $Reference.sha256) { throw "資產身分相互衝突：$path" }
    $Map[$path]=$Reference.sha256.ToLowerInvariant()
    return $path
}
function Add-PanelNestedPins($Map,$Value,[string]$Base,[int]$Depth=0) {
    if ($Depth -gt 48) { throw '資產參照巢狀深度過大。' }
    if ($null -eq $Value -or $Value -is [string] -or $Value -is [ValueType]) { return }
    if ($Value -is [Array]) { foreach($part in $Value){ Add-PanelNestedPins $Map $part $Base ($Depth+1) };return }
    if ($Value.PSObject.Properties['file'] -and $Value.PSObject.Properties['sha256']) { $null=Add-PanelPin $Map $Value $Base }
    foreach($property in $Value.PSObject.Properties){ Add-PanelNestedPins $Map $property.Value $Base ($Depth+1) }
}
function Get-PanelContext([string]$Path,[string]$ScriptPath=$PSCommandPath) {
    $config=Read-PanelJson $Path
    if ($config.schema -ne 'pas.manual-song-panel-launch.v1' -or $config.manual_selection -isnot [bool] -or $config.manual_selection -ne $true -or
        $config.manual_start -isnot [bool] -or $config.manual_start -ne $true) { throw '設定必須保留人工選曲與人工 Play。' }
    $pins=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
    if(@($config.scripts).Count -lt 2 -or @($config.scripts).Count -gt 16){throw '啟動腳本參照數量無效。'}
    foreach($name in @('plan','library','build_manifest','kernel_profile','capacity_evidence','gui_script')) { $null=Add-PanelPin $pins $config.$name }
    foreach($reference in $config.scripts){$null=Add-PanelPin $pins $reference}
    if ([IO.Path]::GetFullPath($config.gui_script.file) -ne [IO.Path]::GetFullPath($ScriptPath)) { throw '此介面與設定封存的 GUI 腳本不同。' }
    if (-not [IO.Path]::IsPathFullyQualified($config.output_parent) -or -not (Test-Path -LiteralPath $config.output_parent -PathType Container)) { throw '輸出父目錄不存在。' }
    $archive=[IO.Path]::GetFullPath($config.build_archive)
    if (-not [IO.Path]::IsPathFullyQualified($config.build_archive) -or [IO.Path]::GetFullPath($config.build_manifest.file) -ne (Join-Path $archive 'manifest.json')) { throw 'Binary 封存與 manifest 不一致。' }
    $plan=Read-PanelJson $config.plan.file
    $library=Read-PanelJson $config.library.file
    if ($plan.schema -ne 'pas.chapter-IN-sweep-plan.v1' -or $plan.chapter_id -ne 'MainStoryLegacy' -or $plan.difficulty -ne 'IN' -or
        $plan.song_count -ne 20 -or @($plan.jobs).Count -ne 20 -or $plan.round_limit -ne 20 -or $config.round_limit -ne 20) { throw '正式 Chapter Legacy 清單必須保留原生 20 首、各一輪。' }
    if ($library.schema -ne 'pas.local-song-library.v1' -or @($library.songs).Count -ne 21 -or
        $library.source_plan.file -ne $config.plan.file -or $library.source_plan.sha256 -ne $config.plan.sha256) { throw '本機曲庫必須綁定正式清單，包含 20 首與一首暖身。' }
    $chapters=@($library.songs|Where-Object role -eq 'chapter'|Sort-Object native_order)
    $warmups=@($library.songs|Where-Object role -eq 'warmup')
    if ($chapters.Count -ne 20 -or $warmups.Count -ne 1) { throw '曲庫的章節／暖身分類不完整。' }
    $seen=@{}
    foreach($job in $library.songs) {
        foreach($name in @('track_id','display_name','library_id')){Assert-PanelText $job.$name $name}
        if ($job.library_id -ne ($job.track_id+':IN') -or $seen.ContainsKey($job.track_id) -or $job.difficulty -ne 'IN' -or $job.rounds -ne 1 -or
            $job.role -notin @('chapter','warmup') -or $job.default_selected -isnot [bool]) { throw '曲庫存在重複／無效的單曲項目。' }
        $seen[$job.track_id]=$true
        foreach($name in @('chart','profile','state_calibration','hud_reference')){$null=Add-PanelPin $pins $job.$name}
        if ($job.role -eq 'warmup') {if ($null -ne $job.native_order -or $job.group -ne 'Additional') {throw '暖身曲不能混入 Chapter Legacy 原生順序。'};$null=Add-PanelPin $pins $job.source_manifest}
    }
    for($i=0;$i -lt 20;$i++) {
        $native=$plan.jobs[$i];$copy=$chapters[$i]
        if ($native.order -ne $i+1 -or $copy.native_order -ne $native.order -or $copy.group -ne 'Chapter Legacy') { throw 'Chapter Legacy 原生順序已改變。' }
        foreach($key in @('track_id','difficulty','rounds','compiled_fingerprint','notes','events','peak')) {if ($copy.$key -ne $native.$key){throw "曲庫與正式清單不一致：$key"}}
        foreach($key in @('chart','profile','state_calibration','hud_reference')) {if ($copy.$key.file -ne $native.$key.file -or $copy.$key.sha256 -ne $native.$key.sha256){throw "曲庫資產與正式清單不一致：$key"}}
    }
    $build=Read-PanelJson $config.build_manifest.file
    if ($build.schema -ne 'pas.adaptive-build-archive.v1' -or @($build.artifacts).Count -gt 2048) { throw '無效的封存 binary manifest。' }
    $bin=Join-Path $archive 'bin';$binPrefix=$bin+[IO.Path]::DirectorySeparatorChar;$declared=@{}
    foreach($artifact in $build.artifacts) {
        $target=[IO.Path]::GetFullPath((Join-Path $archive $artifact.file))
        if (-not $target.StartsWith($archive+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {throw '封存資產路徑越界。'}
        $null=Add-PanelPin $pins ([pscustomobject]@{file=$target;sha256=$artifact.sha256})
        if ($target.StartsWith($binPrefix,[StringComparison]::OrdinalIgnoreCase)) {$declared[$target]=$true}
    }
    $exe=Join-Path $bin 'chart_experiment.exe'
    if (-not $declared.ContainsKey($exe)) { throw '封存沒有固定 chart_experiment.exe。' }
    foreach($binary in Get-ChildItem -LiteralPath $bin -File) {if ($binary.Extension -in @('.exe','.dll') -and -not $declared.ContainsKey($binary.FullName)){throw "未固定的 binary／DLL：$($binary.FullName)"}}
    # Pin consumed schemas only. Historical provenance paths have other bases
    # and are not runtime dependencies merely because they contain file/SHA.
    $null=Add-PanelPin $pins ([pscustomobject]@{file=[IO.Path]::GetFullPath($Path);sha256=(Get-FileHash -LiteralPath $Path).Hash})
    $indexFile=Add-PanelPin $pins $plan.chapter_index
    $null=Add-PanelPin $pins $plan.readiness_summary
    $index=Read-PanelJson $indexFile;$indexRoot=Split-Path -Parent $indexFile
    $states=@{}
    foreach($job in $library.songs){
        $stateFile=$job.state_calibration.file
        if(-not $states.ContainsKey($stateFile)){
            $states[$stateFile]=$true;$dataset=Read-PanelJson $stateFile
            if($dataset.schema -ne 'pas.screen-state-dataset.v1' -or @($dataset.templates).Count -gt 64){throw '無效的畫面資料集。'}
            foreach($template in $dataset.templates){$null=Add-PanelPin $pins ([pscustomobject]@{file=$template.rgb.path;sha256=$template.rgb.sha256}) (Split-Path -Parent $stateFile)}
        }
        $sourceRef=$job.source_manifest
        if($job.role -eq 'chapter'){
            $indexed=@($index.songs|Where-Object track_id -eq $job.track_id)
            if($indexed.Count -ne 1){throw '章節來源缺少本首。'}
            $choice=@($indexed[0].charts|Where-Object difficulty -eq 'IN')
            if($choice.Count -ne 1){throw '章節來源 IN 身分不唯一。'}
            $sourceFile=Add-PanelPin $pins $choice[0].source_manifest $indexRoot
        }else{$sourceFile=Add-PanelPin $pins $sourceRef}
        $source=Read-PanelJson $sourceFile;$sourceRoot=Split-Path -Parent $sourceFile
        if($source.track_id -ne $job.track_id -or $source.difficulty -ne 'IN' -or $source.chart.sha256 -ne $job.chart.sha256){throw '曲目來源 manifest 身分不一致。'}
        $sourceChart=Add-PanelPin $pins $source.chart $sourceRoot
        if($sourceChart -ne $job.chart.file){throw '曲目來源譜面路徑不一致。'}
        $null=Add-PanelPin $pins $source.decoded_audio $sourceRoot
        if($source.native_audio){$null=Add-PanelPin $pins $source.native_audio $sourceRoot}
        $profile=Read-PanelJson $job.profile.file
        if($profile.chart_sha256 -ne $job.chart.sha256 -or $profile.audio_sha256 -ne $source.decoded_audio.sha256){throw 'Profile 的譜面／音源身分不一致。'}
    }
    $capacity=Read-PanelJson $config.capacity_evidence.file
    foreach($name in @('analysis','host_manifest','retrieval_report')){$null=Add-PanelPin $pins $capacity.$name}
    if(@($capacity.artifacts).Count -gt 8){throw '容量證據資產數超出限制。'}
    foreach($reference in $capacity.artifacts){$null=Add-PanelPin $pins $reference}
    if($pins.Count -gt 4096){throw '資產參照數量超出限制。'}
    $ordered=@($warmups)+@($chapters)
    return [pscustomobject]@{Configuration=$config;Plan=$plan;Library=$library;Jobs=$ordered;Pins=$pins;Executable=$exe;Bin=$bin;ConfigPath=[IO.Path]::GetFullPath($Path)}
}
function Test-PanelPins($Context) {
    foreach($entry in $Context.Pins.GetEnumerator()) {if ((Get-FileHash -LiteralPath $entry.Key -Algorithm SHA256).Hash -ne $entry.Value){throw "資產已改變：$($entry.Key)"}}
}
function New-PanelArguments($Context,$Job,[string]$Output,[switch]$CheckOnly) {
    $mode=if($CheckOnly){'check-adaptive'}else{'run-adaptive'}
    return [string[]]@($mode,'emulator-5554','phigros',$Job.chart.file,$Job.state_calibration.file,$Context.Configuration.kernel_profile.file,$Output,'-80',
        '--experimental-whole-song','--song-profile',$Job.profile.file,'--experimental-hud-reference',$Job.hud_reference.file,'--capacity-evidence',$Context.Configuration.capacity_evidence.file)
}
function Request-PanelStop([string]$RoundDirectory) {
    # Do not create the native output directory: run-adaptive requires it NEW.
    if (-not $RoundDirectory -or -not (Test-Path -LiteralPath $RoundDirectory -PathType Container)) {return $false}
    $stop=Join-Path $RoundDirectory 'STOP'
    if (-not (Test-Path -LiteralPath $stop)) {[IO.File]::WriteAllText($stop,'operator requested graceful stop',[Text.UTF8Encoding]::new($false))}
    return $true
}
function Invoke-PanelOwnedStart($Owner,$Operation,[string]$Executable,[string]$WorkingDirectory,[string[]]$Arguments,[string]$Stdout,[string]$Stderr) {
    $Owner.Process=$Operation
    try{$Operation.Start($Executable,$WorkingDirectory,$Arguments,$Stdout,$Stderr)}
    catch{if(-not $Operation.Started){$Owner.Process=$null;$Operation.Dispose()};throw}
}
function Test-PanelManifestCounters($Manifest,$ExpectedJob=$null) {
    $complete=$Manifest.whole_song_sequence_complete -is [bool] -and $Manifest.whole_song_sequence_complete
    foreach($name in @('full_chart_notes','full_compiled_steps','receipt_rows','down_attempts','up_attempts')){
        if($Manifest.PSObject.Properties[$name]){
            $number=$Manifest.$name
            if(($number -isnot [int] -and $number -isnot [long]) -or $number -lt 0 -or $number -gt 2000000){return $false}
            if($complete -and $number -eq 0){return $false}
        }
    }
    if($ExpectedJob){
        foreach($pair in @(@('full_chart_notes','notes'),@('full_compiled_steps','events'))){
            if($Manifest.PSObject.Properties[$pair[0]] -and $null -ne $ExpectedJob.($pair[1]) -and $Manifest.($pair[0]) -ne $ExpectedJob.($pair[1])){return $false}
        }
    }
    if($complete){
        foreach($pair in @(@('receipt_rows','full_compiled_steps'),@('down_attempts','full_chart_notes'),@('up_attempts','full_chart_notes'))){
            if($Manifest.PSObject.Properties[$pair[0]] -and $Manifest.PSObject.Properties[$pair[1]] -and $Manifest.($pair[0]) -ne $Manifest.($pair[1])){return $false}
        }
    }
    if($null -ne $Manifest.runtime_final){
        if($Manifest.runtime_final -isnot [pscustomobject]){return $false}
        $runtime=$Manifest.runtime_final
        foreach($name in @('selected_contacts','selected_steps','completed_contacts','active','pending')){
            if($runtime.PSObject.Properties[$name]){$number=$runtime.$name;if(($number -isnot [int] -and $number -isnot [long]) -or $number -lt 0 -or $number -gt 2000000){return $false}}
        }
        foreach($pair in @(@('selected_contacts','full_chart_notes'),@('selected_steps','full_compiled_steps'))){
            if($runtime.PSObject.Properties[$pair[0]] -and $Manifest.PSObject.Properties[$pair[1]] -and $runtime.($pair[0]) -ne $Manifest.($pair[1])){return $false}
        }
        if($complete){
            if(($runtime.PSObject.Properties['active'] -and $runtime.active -ne 0) -or ($runtime.PSObject.Properties['pending'] -and $runtime.pending -ne 0)){return $false}
            if($runtime.PSObject.Properties['completed_contacts'] -and $Manifest.PSObject.Properties['full_chart_notes'] -and $runtime.completed_contacts -ne $Manifest.full_chart_notes){return $false}
        }
    }
    return $true
}
function Test-PanelKernelReleaseEvidence($Manifest) {
    if($Manifest.kernel_final_zero -isnot [bool] -or -not $Manifest.kernel_final_zero -or $null -eq $Manifest.kernel_final){return $true}
    $snapshot=$Manifest.kernel_final
    if($snapshot -isnot [pscustomobject]){return $false}
    if($snapshot.PSObject.Properties['active_count'] -and (($snapshot.active_count -isnot [int] -and $snapshot.active_count -isnot [long]) -or $snapshot.active_count -ne 0)){return $false}
    if($snapshot.PSObject.Properties['causal_host_interval_valid'] -and ($snapshot.causal_host_interval_valid -isnot [bool] -or -not $snapshot.causal_host_interval_valid)){return $false}
    if($snapshot.PSObject.Properties['slot_count'] -and (($snapshot.slot_count -isnot [int] -and $snapshot.slot_count -isnot [long]) -or $snapshot.slot_count -lt 1 -or $snapshot.slot_count -gt 256)){return $false}
    if($snapshot.PSObject.Properties['tracking_ids']){
        if($snapshot.tracking_ids -isnot [Array] -or $snapshot.tracking_ids.Count -gt 256){return $false}
        if($snapshot.PSObject.Properties['slot_count'] -and $snapshot.tracking_ids.Count -ne $snapshot.slot_count){return $false}
        foreach($tracking in $snapshot.tracking_ids){if(($tracking -isnot [int] -and $tracking -isnot [long]) -or $tracking -ne -1){return $false}}
    }
    return $true
}
function Get-PanelRoundResult([string]$RoundDirectory,[int]$ExitCode,[string]$StderrTail='',$ExpectedJob=$null,$Context=$null) {
    $path=Join-Path $RoundDirectory 'manifest.json';$manifest=$null;$failure=$null;$hash=$null
    try {$manifest=Read-PanelJson $path 1048576;$hash=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
    catch {$failure='未取得完整原生 manifest：'+$_.Exception.Message}
    $identityMatched=$null -ne $manifest -and $manifest.schema -eq 'pas.experimental-whole-song.v1'
    if($identityMatched -and $ExpectedJob){
        $identityMatched=$manifest.song_id -eq $ExpectedJob.track_id -and $manifest.difficulty -eq $ExpectedJob.difficulty -and
            $manifest.chart_sha256 -eq $ExpectedJob.chart.sha256 -and $manifest.song_profile_sha256 -eq $ExpectedJob.profile.sha256 -and
            $manifest.screen_calibration_sha256 -eq $ExpectedJob.state_calibration.sha256 -and $manifest.expected_plan_sha256 -eq $ExpectedJob.compiled_fingerprint
    }
    if($identityMatched -and $Context){$identityMatched=$manifest.executable_sha256 -eq $Context.Pins[$Context.Executable] -and $manifest.kernel_profile_sha256 -eq $Context.Configuration.kernel_profile.sha256}
    $countersConsistent=$null -ne $manifest -and (Test-PanelManifestCounters $manifest $ExpectedJob)
    if(-not $countersConsistent){$identityMatched=$false}
    $releaseConsistent=$null -ne $manifest -and (Test-PanelKernelReleaseEvidence $manifest)
    $success=$identityMatched -and $releaseConsistent -and $ExitCode -eq 0 -and
        $manifest.status -eq 'experimental_whole_song_sent_judgment_unreviewed' -and $manifest.PSObject.Properties['failure'] -and $null -eq $manifest.failure -and
        $manifest.whole_song_sequence_complete -is [bool] -and $manifest.whole_song_sequence_complete -eq $true -and
        $manifest.manual_result_confirmed -is [bool] -and $manifest.manual_result_confirmed -eq $true -and
        $manifest.kernel_final_zero -is [bool] -and $manifest.kernel_final_zero -eq $true
    if (-not $success -and $null -ne $manifest){$failure=if($manifest.failure){$manifest.failure}else{'程序或結束／釋放證據未通過。'}}
    if (-not $success -and $StderrTail){$failure=[string]$failure+'；'+$StderrTail}
    $execution='unknown';$resultState='unknown';$release='unknown'
    if($identityMatched){
        if($manifest.whole_song_sequence_complete -is [bool]){$execution=if($manifest.whole_song_sequence_complete){'completed'}else{'incomplete'}}
        if($manifest.manual_result_confirmed -is [bool]){$resultState=if($manifest.manual_result_confirmed){'confirmed_by_native'}else{'not_confirmed'}}
        if($manifest.kernel_final_zero -is [bool]){$release=if(-not $manifest.kernel_final_zero){'kernel_zero_not_confirmed'}elseif($releaseConsistent){'kernel_zero_confirmed'}else{'unknown'}}
    }
    $images=[Collections.Generic.List[object]]::new()
    foreach($name in @('result.png','last-observed.png')){
        $imagePath=Join-Path $RoundDirectory $name
        if(Test-Path -LiteralPath $imagePath -PathType Leaf){
            try{
                $image=Get-Item -LiteralPath $imagePath
                if($image.Length -gt 0 -and $image.Length -le 16777216){$images.Add([pscustomobject]@{file=$image.FullName;sha256=(Get-FileHash -LiteralPath $imagePath -Algorithm SHA256).Hash.ToLowerInvariant();bytes=$image.Length;kind='captured_image_unreviewed'})}
            }catch{} # Optional evidence never overwrites the native failure.
        }
    }
    return [pscustomobject]@{Success=$success;ExitCode=$ExitCode;Failure=$failure;KernelFinalZero=if($manifest){$manifest.kernel_final_zero}else{$null};ManifestFile=if($manifest){$path}else{$null};ManifestSha256=$hash;Manifest=$manifest;
        ManifestIdentityMatched=$identityMatched;CountersConsistent=$countersConsistent;ReleaseEvidenceConsistent=$releaseConsistent;ExecutionState=$execution;ResultState=$resultState;ReleaseState=$release;JudgmentState='unreviewed';EvidenceFiles=$images.ToArray()}
}
function Get-PanelRoundPresentation($Result) {
    $hasResult=@($Result.EvidenceFiles|Where-Object{[IO.Path]::GetFileName($_.file) -eq 'result.png'}).Count -gt 0
    $hasLast=@($Result.EvidenceFiles|Where-Object{[IO.Path]::GetFileName($_.file) -eq 'last-observed.png'}).Count -gt 0
    $imageText=if($hasResult){'結算圖片檔已保存'}elseif($hasLast){'最後畫面已保存'}else{'未找到結算圖片，請開啟紀錄檢查'}
    if($Result.Success -is [bool] -and $Result.Success){return [pscustomobject]@{Title='本首完成，結算已確認';Detail=$imageText+'；分數與判定仍待複核。';RowStatus='完成 · 待複核';InputCompleteResultUnconfirmed=$false}}
    if($Result.ExecutionState -eq 'completed' -and $Result.ResultState -eq 'not_confirmed' -and $Result.ReleaseState -eq 'kernel_zero_confirmed'){
        return [pscustomobject]@{Title='輸入已完成，結算辨識尚未確認';Detail='觸控已釋放，佇列已停止；'+$imageText+'，分數與判定待複核。';RowStatus='輸入完成／待確認';InputCompleteResultUnconfirmed=$true}
    }
    if($Result.ExecutionState -eq 'completed'){
        $releaseText=if($Result.ReleaseState -eq 'kernel_zero_confirmed'){'觸控已釋放'}else{'觸控零接觸尚未確認'}
        return [pscustomobject]@{Title='輸入已完成，結束狀態待確認';Detail=$releaseText+'，佇列已停止；'+$imageText+'，判定待複核。';RowStatus='輸入完／待檢查';InputCompleteResultUnconfirmed=$false}
    }
    return [pscustomobject]@{Title='本輪停止，佇列未繼續';Detail=[string]$Result.Failure;RowStatus=if($Result.ExecutionState -eq 'incomplete'){'輸入未完成'}else{'停止／狀態未知'};InputCompleteResultUnconfirmed=$false}
}
function Assert-PanelPreflight([string]$Path,$Job,$Context) {
    $result=Read-PanelJson $Path 1048576
    if($result.status -ne 'offline_preflight_passed' -or $result.device_discovery -isnot [bool] -or $result.device_discovery -ne $false -or
       $result.input_enabled -isnot [bool] -or $result.input_enabled -ne $false -or $result.song_id -ne $Job.track_id -or
       $result.difficulty -ne $Job.difficulty -or $result.chart_sha256 -ne $Job.chart.sha256 -or $result.compiled_sha256 -ne $Job.compiled_fingerprint -or
       $result.executable_sha256 -ne $Context.Pins[$Context.Executable] -or $result.screen_calibration_sha256 -ne $Job.state_calibration.sha256 -or
       $result.HUD_reference_sha256 -ne $Job.hud_reference.sha256 -or $result.kernel_profile_sha256 -ne $Context.Configuration.kernel_profile.sha256) {throw '原生離線檢查回應與本首／封存身分不一致。'}
}
function ConvertFrom-PanelAcquisitionProgress([string]$Line) {
    $prefix='ACQUISITION STATUS: '
    if(-not $Line.StartsWith($prefix,[StringComparison]::Ordinal) -or $Line.Length -gt 4096){return $null}
    try{$value=$Line.Substring($prefix.Length)|ConvertFrom-Json -Depth 8 -ErrorAction Stop}catch{return $null}
    if($null -eq $value -or $value -is [Array] -or $value.schema -cne 'pas.opening-acquisition-progress.v1' -or
        $value.stage -cnotin @('waiting_for_manual_play','waiting_for_valid_HUD','waiting_for_first_note') -or
        $value.reason -isnot [string] -or $value.reason.Length -gt 512 -or $value.reason -match '[\x00-\x1f]'){return $null}
    foreach($name in @('pause_ok','prefix_ok','score_ok','hud_ok')){if($value.$name -isnot [bool]){return $null}}
    # A diagnostic does not itself grant a UI lease or prove source pixel age.
    # Refuse internally contradictory stage claims instead of showing success.
    if(($value.stage -eq 'waiting_for_first_note' -and (-not $value.hud_ok -or -not $value.pause_ok -or -not $value.prefix_ok -or -not $value.score_ok)) -or
        ($value.stage -eq 'waiting_for_valid_HUD' -and $value.hud_ok)){return $null}
    return $value
}
function Get-PanelAcquisitionReason($Progress) {
    if($Progress.stage -eq 'waiting_for_manual_play'){return '請在遊戲按 Play；程式目前尚未送出觸控。'}
    if($Progress.stage -eq 'waiting_for_valid_HUD'){
        $missing=[Collections.Generic.List[string]]::new()
        if(-not $Progress.pause_ok){$missing.Add('暫停圖示')}
        if(-not $Progress.prefix_ok){$missing.Add('IN 難度標籤')}
        if(-not $Progress.score_ok){$missing.Add('分數欄')}
        if($missing.Count){return ($missing.ToArray()-join '、')+'尚未通過辨識；請保持遊戲在前景。'}
        return '畫面一致性尚未通過；請保持遊戲在前景。'
    }
    switch($Progress.reason){
        'waiting_for_second_distinct_native_group_frame' {return '已觀察首音，等待下一張有效畫面；尚未送出觸控。'}
        'native_group_temporal_or_source_ambiguity_retained' {return '首音時間或來源仍有歧義；尚未送出觸控。'}
        'receipt_expired_during_processing' {return '畫面已過期，正在等待新畫面；尚未送出觸控。'}
        default {return '等待可確定的首音與時間；尚未送出觸控。'}
    }
}
function Get-PanelNativeStageUpdate([string]$Line,[string]$Phase,[bool]$StopRequested=$false) {
    if($StopRequested -or $Phase -notin @('starting','ready','playing')){return $null}
    if($Line.StartsWith('ACQUISITION STATUS: ',[StringComparison]::Ordinal)){
        if($Phase -ne 'ready'){return $null}
        $progress=ConvertFrom-PanelAcquisitionProgress $Line
        if(-not $progress){return $null}
        $title=switch($progress.stage){
            'waiting_for_manual_play' {'READY｜程式備妥，請到遊戲按 Play'}
            'waiting_for_valid_HUD' {'遊戲 HUD 尚未通過｜尚未送出觸控'}
            'waiting_for_first_note' {'遊戲已辨識，等待首音｜尚未送出觸控'}
        }
        $reason=Get-PanelAcquisitionReason $progress
        $key=@($progress.stage,$progress.reason,$progress.pause_ok,$progress.prefix_ok,$progress.score_ok,$progress.hud_ok)-join '|'
        return [pscustomobject]@{Kind='acquisition';Phase='ready';Title=$title;Detail=$reason;Log=$title+'。'+$reason;Key=$key;Progress=$progress}
    }
    if($Phase -eq 'starting' -and $Line.StartsWith('WHOLE SONG EXPERIMENT READY:',[StringComparison]::Ordinal)){
        return [pscustomobject]@{Kind='ready';Phase='ready';Title='READY｜程式備妥，請到遊戲按 Play';Detail='READY 代表程式已備妥；通過遊戲 HUD 與首音辨識後，才會送出觸控。';Log='程式已備妥，請到遊戲按 Play。首音確認前尚未送出觸控。';Key='ready';Progress=$null}
    }
    if($Phase -in @('starting','ready') -and $Line.StartsWith('FIRST NOTE ACQUIRED:',[StringComparison]::Ordinal)){
        return [pscustomobject]@{Kind='playing';Phase='playing';Title='正在演奏';Detail='首音已由原生程式確認。保持遊戲前景；隨時可按「停止」。';Log='首音已確認，原生程式進入演奏階段。';Key='playing';Progress=$null}
    }
    if($Phase -eq 'playing' -and $Line.StartsWith('WHOLE SONG RPC SEQUENCE COMPLETED;',[StringComparison]::Ordinal)){
        return [pscustomobject]@{Kind='awaiting_result';Phase='awaiting_result';Title='輸入已完成，正在等候遊戲結算';Detail='確認原生結束與釋放紀錄後，才能前往下一首。';Log='原生輸入序列完成，等候遊戲結算與釋放紀錄。';Key='awaiting_result';Progress=$null}
    }
    return $null
}

if ($LoadCoreOnly) {return}
if ($SelfTest) {
    $checks=0;$temporary=Join-Path ([IO.Path]::GetTempPath()) ('PAS 面板離線測試 '+[Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $temporary|Out-Null
    try {
        $argv=[string[]]@('check-adaptive',"C:\含空格\O'Brien\譜面.json",'--song-profile','完整 Unicode')
        $info=[PasSweep.NativeOperation]::BuildStartInfo('C:\fake.exe',$temporary,$argv)
        if ($info.UseShellExecute -or -not $info.CreateNoWindow -or $info.ArgumentList.Count -ne 4 -or $info.ArgumentList[1] -ne $argv[1]){throw 'ArgumentList 或隱藏程序設定錯誤。'};$checks++
        $future=Join-Path $temporary '尚未建立'
        if ((Request-PanelStop $future) -or (Test-Path -LiteralPath $future)){throw 'STOP 搶先建立原生目錄。'};$checks++
        New-Item -ItemType Directory -Path $future|Out-Null
        if (-not (Request-PanelStop $future) -or -not (Test-Path -LiteralPath (Join-Path $future 'STOP'))){throw 'STOP 未保存。'};$checks++
        $missing=Get-PanelRoundResult $future 0
        if($missing.Success -or $null -ne $missing.KernelFinalZero){throw '缺少 manifest 被視為成功。'};$checks++
        $valid=[ordered]@{schema='pas.experimental-whole-song.v1';status='experimental_whole_song_sent_judgment_unreviewed';failure=$null;whole_song_sequence_complete=$true;manual_result_confirmed=$true;kernel_final_zero=$true}
        $valid|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $future 'manifest.json') -Encoding utf8
        if (-not (Get-PanelRoundResult $future 0).Success -or (Get-PanelRoundResult $future 2).Success){throw '原生結束狀態檢查錯誤。'};$checks+=2
        $valid.kernel_final_zero=$false;$valid.failure='actual_UI_receipt_expired'
        $valid|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $future 'manifest.json') -Encoding utf8
        $failed=Get-PanelRoundResult $future 1
        if($failed.Success -or $failed.KernelFinalZero -ne $false -or $failed.Failure -notmatch 'actual_UI_receipt_expired'){throw '故障或 kernel 非零證據遺失。'};$checks++
        $pins=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
        $file=Join-Path $temporary '資產.txt';[IO.File]::WriteAllText($file,'原件')
        $null=Add-PanelPin $pins ([pscustomobject]@{file=$file;sha256=(Get-FileHash -LiteralPath $file).Hash})
        Test-PanelPins ([pscustomobject]@{Pins=$pins});$checks++
        [IO.File]::WriteAllText($file,'已修改');$rejected=$false
        try{Test-PanelPins ([pscustomobject]@{Pins=$pins})}catch{$rejected=$true}
        if(-not $rejected){throw '雜湊異動未拒絕。'};$checks++
        foreach($started in @($false,$true)){
            $owner=@{Process=$null};$fake=[pscustomobject]@{Started=$started;Disposed=$false}
            $fake|Add-Member ScriptMethod Start {param($exe,$cwd,$args,$stdout,$stderr)throw 'synthetic partial start'}
            $fake|Add-Member ScriptMethod Dispose {$this.Disposed=$true}
            try{Invoke-PanelOwnedStart $owner $fake 'fake' $temporary @('none') 'stdout' 'stderr'}catch{}
            if($started -and ($owner.Process -ne $fake -or $fake.Disposed)){throw '部分啟動失敗遺失程序擁有權。'}
            if(-not $started -and ($null -ne $owner.Process -or -not $fake.Disposed)){throw '未啟動的失敗未正確清理。'};$checks++
        }
        $progress=[ordered]@{schema='pas.opening-acquisition-progress.v1';stage='waiting_for_manual_play';reason='waiting_for_manual_play';pause_ok=$false;prefix_ok=$false;score_ok=$false;hud_ok=$false}
        $line='ACQUISITION STATUS: '+($progress|ConvertTo-Json -Compress)
        if(-not (ConvertFrom-PanelAcquisitionProgress $line) -or (Get-PanelNativeStageUpdate $line 'ready').Kind -ne 'acquisition'){throw '人工 Play 狀態未解析。'};$checks++
        $progress.stage='waiting_for_valid_HUD';$progress.reason='fixed_glyph_or_negative_space_missing';$progress.pause_ok=$true;$progress.score_ok=$true
        $line='ACQUISITION STATUS: '+($progress|ConvertTo-Json -Compress)
        $update=Get-PanelNativeStageUpdate $line 'ready'
        if($update.Title -notmatch '尚未送出觸控' -or $update.Detail -notmatch 'IN 難度標籤' -or $update.Phase -ne 'ready'){throw 'HUD 失敗被誤報為演奏。'};$checks++
        foreach($phase in @('starting','playing','awaiting_result','result','failed','stopped','complete')){if(Get-PanelNativeStageUpdate $line $phase){throw '過期開場訊息覆寫現有階段。'};$checks++}
        if(Get-PanelNativeStageUpdate $line 'ready' $true){throw '停止後仍更新開場狀態。'};$checks++
        $progress.stage='waiting_for_first_note';$progress.prefix_ok=$true;$progress.hud_ok=$true
        $line='ACQUISITION STATUS: '+($progress|ConvertTo-Json -Compress)
        if((Get-PanelNativeStageUpdate $line 'ready').Title -notmatch '等待首音'){throw '有效 HUD 未保持等待首音。'};$checks++
        foreach($invalid in @('ACQUISITION STATUS: {','prefix ACQUISITION STATUS: {}','ACQUISITION STATUS: []','ACQUISITION STATUS: '+('x'*4097))){if(ConvertFrom-PanelAcquisitionProgress $invalid){throw '無效的 progress 被接受。'};$checks++}
        foreach($bad in @('true',1,$null)){$copy=$progress|ConvertTo-Json -Compress|ConvertFrom-Json;$copy.hud_ok=$bad;if(ConvertFrom-PanelAcquisitionProgress ('ACQUISITION STATUS: '+($copy|ConvertTo-Json -Compress))){throw 'progress boolean 未嚴格檢查。'};$checks++}
        $copy=$progress|ConvertTo-Json -Compress|ConvertFrom-Json;$copy.stage='playing';if(ConvertFrom-PanelAcquisitionProgress ('ACQUISITION STATUS: '+($copy|ConvertTo-Json -Compress))){throw 'progress 偽造演奏階段。'};$checks++
        if((Get-PanelNativeStageUpdate 'FIRST NOTE ACQUIRED: native' 'ready').Phase -ne 'playing' -or (Get-PanelNativeStageUpdate 'WHOLE SONG EXPERIMENT READY: duplicate' 'playing')){throw '原生階段排序錯誤。'};$checks++
        $stages=[ordered]@{schema='pas.experimental-whole-song.v1';status='failed_or_stopped';failure='manual_round_ended_without_result_confirmation';whole_song_sequence_complete=$true;manual_result_confirmed=$false;kernel_final_zero=$true;AP_verified=$true;game_judgment='AP'}
        $stagePath=Join-Path $future 'manifest.json';$stages|ConvertTo-Json|Set-Content -LiteralPath $stagePath -Encoding utf8
        $separated=Get-PanelRoundResult $future 2
        if($separated.Success -or $separated.ExecutionState -ne 'completed' -or $separated.ResultState -ne 'not_confirmed' -or $separated.ReleaseState -ne 'kernel_zero_confirmed' -or $separated.JudgmentState -ne 'unreviewed'){throw '輸入／結算／判定／釋放狀態未獨立保留。'};$checks++
        $presentation=Get-PanelRoundPresentation $separated
        if(-not $presentation.InputCompleteResultUnconfirmed -or $presentation.Title -notmatch '輸入已完成' -or $presentation.Detail -notmatch '判定待複核' -or $presentation.Detail -match 'AP'){throw '已完成輸入被誤報成 AP 或遺失待確認狀態。'};$checks++
        $png=[Convert]::FromBase64String('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAusB9Wl6V2EAAAAASUVORK5CYII=')
        [IO.File]::WriteAllBytes((Join-Path $future 'last-observed.png'),$png)
        $withImage=Get-PanelRoundResult $future 2
        if($withImage.Success -or $withImage.ResultState -ne 'not_confirmed' -or @($withImage.EvidenceFiles).Count -ne 1 -or (Get-PanelRoundPresentation $withImage).Detail -notmatch '最後畫面已保存'){throw '圖片存在被誤當成結算確認。'};$checks++
        $wrong=[pscustomobject]@{track_id='another-song';difficulty='IN';chart=[pscustomobject]@{sha256=('a'*64)};profile=[pscustomobject]@{sha256=('b'*64)};state_calibration=[pscustomobject]@{sha256=('c'*64)};compiled_fingerprint=('d'*64)}
        $foreign=Get-PanelRoundResult $future 2 '' $wrong
        if($foreign.ExecutionState -ne 'unknown' -or $foreign.ReleaseState -ne 'unknown' -or $foreign.ManifestIdentityMatched){throw '他曲 manifest 被歸因為本曲完成／釋放。'};$checks++
        $stages.kernel_final_zero=$false;$stages|ConvertTo-Json|Set-Content -LiteralPath $stagePath -Encoding utf8
        $notZero=Get-PanelRoundResult $future 2
        if($notZero.ReleaseState -ne 'kernel_zero_not_confirmed' -or (Get-PanelRoundPresentation $notZero).Detail -match '觸控已釋放'){throw 'kernel 非零／未確認被誤報為釋放。'};$checks++
        $stages.whole_song_sequence_complete=$false;$stages|ConvertTo-Json|Set-Content -LiteralPath $stagePath -Encoding utf8
        if((Get-PanelRoundResult $future 0).ExecutionState -ne 'incomplete'){throw '未完整送出被誤報完成。'};$checks++
        $stages.whole_song_sequence_complete='true';$stages.kernel_final_zero='true';$stages.manual_result_confirmed='true';$stages|ConvertTo-Json|Set-Content -LiteralPath $stagePath -Encoding utf8
        $illtyped=Get-PanelRoundResult $future 0
        if($illtyped.Success -or $illtyped.ExecutionState -ne 'unknown' -or $illtyped.ReleaseState -ne 'unknown' -or $illtyped.ResultState -ne 'unknown'){throw '非 boolean 的狀態被信任。'};$checks++
        $stages.whole_song_sequence_complete=$stages.kernel_final_zero=$stages.manual_result_confirmed=$true;$stages.status='experimental_whole_song_sent_judgment_unreviewed';$stages.failure=$null
        $stages|ConvertTo-Json|Set-Content -LiteralPath $stagePath -Encoding utf8;[IO.File]::WriteAllBytes((Join-Path $future 'result.png'),$png)
        $confirmed=Get-PanelRoundResult $future 0
        if(-not $confirmed.Success -or $confirmed.JudgmentState -ne 'unreviewed' -or (Get-PanelRoundPresentation $confirmed).Detail -notmatch '結算圖片檔已保存'){throw '已保存結果的提示或待複核判定錯誤。'};$checks++
        $stages['full_chart_notes']=1285;$stages['full_compiled_steps']=3358;$stages['receipt_rows']=3358
        $stages['runtime_final']=[pscustomobject]@{selected_contacts=1285;selected_steps=3358;completed_contacts=1285;active=0;pending=0}
        $stages|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $stagePath -Encoding utf8
        if(-not (Get-PanelRoundResult $future 0).CountersConsistent){throw '完整原生數量被錯誤拒絕。'};$checks++
        foreach($badCount in @('receipt','typed','completed','active')){
            $copy=$stages|ConvertTo-Json -Depth 8|ConvertFrom-Json
            switch($badCount){'receipt'{$copy.receipt_rows=3357}'typed'{$copy.full_chart_notes='1285'}'completed'{$copy.runtime_final.completed_contacts=1284}'active'{$copy.runtime_final.active=1}}
            $copy|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $stagePath -Encoding utf8;$inconsistent=Get-PanelRoundResult $future 0
            if($inconsistent.Success -or $inconsistent.CountersConsistent -or $inconsistent.ExecutionState -ne 'unknown' -or $inconsistent.ReleaseState -ne 'unknown'){throw '數量矛盾的原生紀錄被歸因為完整完成。'};$checks++
        }
        $stages['kernel_final']=[pscustomobject]@{active_count=0;slot_count=2;tracking_ids=@(-1,-1);causal_host_interval_valid=$true}
        $stages|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $stagePath -Encoding utf8
        if(-not (Get-PanelRoundResult $future 0).ReleaseEvidenceConsistent){throw '一致的零接觸觀測被拒絕。'};$checks++
        foreach($badRelease in @('active','tracking','typed','slots','causal')){
            $copy=$stages|ConvertTo-Json -Depth 8|ConvertFrom-Json
            switch($badRelease){'active'{$copy.kernel_final.active_count=1}'tracking'{$copy.kernel_final.tracking_ids=@(-1,3)}'typed'{$copy.kernel_final.active_count='0'}'slots'{$copy.kernel_final.slot_count=3}'causal'{$copy.kernel_final.causal_host_interval_valid=$false}}
            $copy|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $stagePath -Encoding utf8;$contradiction=Get-PanelRoundResult $future 0
            if($contradiction.Success -or $contradiction.ReleaseEvidenceConsistent -or $contradiction.ReleaseState -ne 'unknown' -or $contradiction.ExecutionState -ne 'completed'){throw '矛盾釋放紀錄被誤報為零接觸或抹去已完成輸入。'};$checks++
        }
        [pscustomobject]@{schema='pas.sweep-panel-selftest.v1';passed=$checks;devices='NOT RUN';native_processes_started=0;windows_shown=0}|ConvertTo-Json -Compress
    } finally {
        $absolute=[IO.Path]::GetFullPath($temporary);$parent=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
        if($absolute.StartsWith($parent,[StringComparison]::OrdinalIgnoreCase) -and [IO.Path]::GetFileName($absolute).StartsWith('PAS 面板離線測試 ')){Remove-Item -LiteralPath $absolute -Recurse -Force}
    }
    return
}

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[Windows.Forms.Application]::EnableVisualStyles()
$script:Panel=@{Context=$null;Queue=@();Index=0;CheckIndex=0;Phase='idle';Hash=$null;HashPurpose='queue';Process=$null;Purpose='';Session='';Round='';StopRequested=$false;CloseRequested=$false;Ledger=$null;ReadySeen=$false;Started=$null;AcquisitionKey='';SuppressStdoutJson=$false;ErrorTail='';PendingFailure='';LogDirty=$false;LogLines=[Collections.Generic.Queue[string]]::new()}
try {
    if ($Configuration) {$script:Panel.Context=Get-PanelContext ([IO.Path]::GetFullPath($Configuration)) $PSCommandPath}
    elseif (-not $RenderPreview) {throw '請使用已封存的 launch-config.json 開啟介面。'}
}catch{
    if($RenderPreview){throw}
    [Windows.Forms.MessageBox]::Show("無法開啟測試台。請重新確認封存設定與檔案路徑。`r`n`r`n$($_.Exception.Message)`r`n`r`n設定：$Configuration",'設定檢查失敗',[Windows.Forms.MessageBoxButtons]::OK,[Windows.Forms.MessageBoxIcon]::Error)|Out-Null
    return
}

$form=[Windows.Forms.Form]::new();$form.Text='Phigros 譜面測試台';$form.StartPosition='CenterScreen';$form.ClientSize=[Drawing.Size]::new(1120,780)
$form.MinimumSize=[Drawing.Size]::new(1040,740);$form.BackColor=[Drawing.Color]::FromArgb(245,247,250)
$form.Font=[Drawing.Font]::new('Microsoft JhengHei UI',10)
$layout=[Windows.Forms.TableLayoutPanel]::new();$layout.Dock='Fill';$layout.Padding=[Windows.Forms.Padding]::new(24);$layout.ColumnCount=1;$layout.RowCount=4
foreach($height in @(80,84,0,78)){$style=[Windows.Forms.RowStyle]::new();if($height){$style.SizeType='Absolute';$style.Height=$height}else{$style.SizeType='Percent';$style.Height=100};$layout.RowStyles.Add($style)|Out-Null}
$form.Controls.Add($layout)
$header=[Windows.Forms.Panel]::new();$header.Dock='Fill';$layout.Controls.Add($header,0,0)
$title=[Windows.Forms.Label]::new();$title.Text='譜面測試台';$title.Font=[Drawing.Font]::new('Microsoft JhengHei UI',23,[Drawing.FontStyle]::Bold);$title.AutoSize=$true;$title.Location=[Drawing.Point]::new(0,0);$title.ForeColor=[Drawing.Color]::FromArgb(25,39,56);$header.Controls.Add($title)
$subtitle=[Windows.Forms.Label]::new();$subtitle.Text='Verruckt 暖身 → Chapter Legacy 20 首 IN　｜　由你選曲、由你按 Play';$subtitle.AutoSize=$true;$subtitle.Location=[Drawing.Point]::new(2,47);$subtitle.ForeColor=[Drawing.Color]::FromArgb(92,106,122);$header.Controls.Add($subtitle)
$statusPanel=[Windows.Forms.Panel]::new();$statusPanel.Dock='Fill';$statusPanel.BackColor=[Drawing.Color]::White;$statusPanel.Padding=[Windows.Forms.Padding]::new(16,10,16,10);$layout.Controls.Add($statusPanel,0,1)
$status=[Windows.Forms.Label]::new();$status.AutoSize=$false;$status.Dock='Top';$status.Height=29;$status.Font=[Drawing.Font]::new('Microsoft JhengHei UI',13,[Drawing.FontStyle]::Bold);$status.Text='選擇要測的歌曲';$statusPanel.Controls.Add($status)
$progress=[Windows.Forms.ProgressBar]::new();$progress.Dock='Bottom';$progress.Height=8;$progress.Maximum=1000;$statusPanel.Controls.Add($progress)
$detail=[Windows.Forms.Label]::new();$detail.Dock='Bottom';$detail.Height=22;$detail.ForeColor=[Drawing.Color]::FromArgb(92,106,122);$detail.Text='勾選曲目後，先按「開始佇列」完成離線檢查。';$statusPanel.Controls.Add($detail)
$split=[Windows.Forms.SplitContainer]::new();$split.Size=[Drawing.Size]::new(1072,400);$split.Dock='Fill';$split.SplitterDistance=590;$split.Panel1MinSize=480;$split.Panel2MinSize=340;$split.Margin=[Windows.Forms.Padding]::new(0,18,0,0);$layout.Controls.Add($split,0,2)
$grid=[Windows.Forms.DataGridView]::new();$grid.Dock='Fill';$grid.BackgroundColor=[Drawing.Color]::White;$grid.BorderStyle='None';$grid.RowHeadersVisible=$false;$grid.AllowUserToAddRows=$false;$grid.AllowUserToDeleteRows=$false;$grid.AllowUserToResizeRows=$false;$grid.MultiSelect=$false;$grid.SelectionMode='FullRowSelect';$grid.AutoSizeColumnsMode='Fill';$grid.RowTemplate.Height=39;$grid.ColumnHeadersHeight=38;$grid.EnableHeadersVisualStyles=$false
$grid.ColumnHeadersDefaultCellStyle.BackColor=[Drawing.Color]::FromArgb(232,237,243);$grid.ColumnHeadersDefaultCellStyle.SelectionBackColor=$grid.ColumnHeadersDefaultCellStyle.BackColor;$grid.ColumnHeadersDefaultCellStyle.ForeColor=[Drawing.Color]::FromArgb(53,69,86)
$grid.DefaultCellStyle.SelectionBackColor=[Drawing.Color]::FromArgb(225,241,248);$grid.DefaultCellStyle.SelectionForeColor=[Drawing.Color]::FromArgb(20,65,88)
$column=[Windows.Forms.DataGridViewCheckBoxColumn]::new();$column.Name='Selected';$column.HeaderText='測試';$column.FillWeight=13;$grid.Columns.Add($column)|Out-Null
foreach($spec in @(@('Song','曲目',53),@('Group','分類',27),@('Result','狀態',31))){$c=[Windows.Forms.DataGridViewTextBoxColumn]::new();$c.Name=$spec[0];$c.HeaderText=$spec[1];$c.FillWeight=$spec[2];$c.ReadOnly=$true;$grid.Columns.Add($c)|Out-Null}
$split.Panel1.Controls.Add($grid)
$right=[Windows.Forms.TableLayoutPanel]::new();$right.Dock='Fill';$right.ColumnCount=1;$right.RowCount=4;$right.Padding=[Windows.Forms.Padding]::new(18,0,0,0)
foreach($spec in @(@('Absolute',68),@('Absolute',170),@('Absolute',28),@('Percent',100))){$r=[Windows.Forms.RowStyle]::new();$r.SizeType=$spec[0];$r.Height=$spec[1];$right.RowStyles.Add($r)|Out-Null};$split.Panel2.Controls.Add($right)
$songTitle=[Windows.Forms.Label]::new();$songTitle.Dock='Fill';$songTitle.Font=[Drawing.Font]::new('Microsoft JhengHei UI',17,[Drawing.FontStyle]::Bold);$songTitle.Text='Verruckt';$songTitle.ForeColor=[Drawing.Color]::FromArgb(25,39,56);$right.Controls.Add($songTitle,0,0)
$instructions=[Windows.Forms.Label]::new();$instructions.Dock='Fill';$instructions.Text="1　先在遊戲選好本首 IN，停在選曲頁。`r`n`r`n2　按下方「已選曲，啟動本首」。`r`n`r`n3　看到 READY 後，再到遊戲按 Play。`r`n`r`nREADY 是程式備妥；遊戲 HUD 與首音確認前，尚未送出觸控。";$instructions.ForeColor=[Drawing.Color]::FromArgb(60,77,94);$right.Controls.Add($instructions,0,1)
$logLabel=[Windows.Forms.Label]::new();$logLabel.Text='執行紀錄';$logLabel.Dock='Fill';$logLabel.ForeColor=[Drawing.Color]::FromArgb(92,106,122);$right.Controls.Add($logLabel,0,2)
$log=[Windows.Forms.TextBox]::new();$log.Multiline=$true;$log.ReadOnly=$true;$log.ScrollBars='Vertical';$log.Dock='Fill';$log.BorderStyle='FixedSingle';$log.BackColor=[Drawing.Color]::White;$log.Font=[Drawing.Font]::new('Microsoft JhengHei UI',9);$right.Controls.Add($log,0,3)
$buttons=[Windows.Forms.FlowLayoutPanel]::new();$buttons.Dock='Fill';$buttons.Padding=[Windows.Forms.Padding]::new(0,18,0,0);$buttons.WrapContents=$false;$layout.Controls.Add($buttons,0,3)
function New-PanelButton([string]$Text,[int]$Width,[bool]$Primary=$false){$b=[Windows.Forms.Button]::new();$b.Text=$Text;$b.Width=$Width;$b.Height=42;$b.FlatStyle='Flat';$b.Margin=[Windows.Forms.Padding]::new(0,0,9,0);$b.BackColor=if($Primary){[Drawing.Color]::FromArgb(20,105,132)}else{[Drawing.Color]::White};$b.ForeColor=if($Primary){[Drawing.Color]::White}else{[Drawing.Color]::FromArgb(40,58,76)};$buttons.Controls.Add($b);return $b}
$startButton=New-PanelButton '開始佇列' 115
$launchButton=New-PanelButton '已選曲，啟動本首' 195 $true
$onlyButton=New-PanelButton '只測本首' 110
$nextButton=New-PanelButton '下一首' 100
$stopButton=New-PanelButton '停止' 90
$logsButton=New-PanelButton '開啟紀錄' 110

$jobs=if($script:Panel.Context){$script:Panel.Context.Jobs}else{@([pscustomobject]@{display_name='Verruckt';role='warmup';track_id='Verruckt';default_selected=$true;ready_for_manual_preflight=$true;blockers=@()})+@(1..20|ForEach-Object{[pscustomobject]@{display_name=@('Glaciaxion','Eradication Catastrophe','Credits','Dlyrotz')[(($_-1)%4)];role='chapter';track_id="preview-$_";default_selected=$true;ready_for_manual_preflight=$true;blockers=@()}})}
foreach($job in $jobs){$index=$grid.Rows.Add([bool]$job.default_selected,$job.display_name, $(if($job.role -eq 'warmup'){'額外暖身'}else{'Chapter Legacy'}),'待測');$grid.Rows[$index].Tag=$job}

function Add-PanelLog([string]$Text){if($Text.Length -gt 1200){$Text=$Text.Substring(0,1200)+' [詳細請開啟紀錄]'};$line=(Get-Date -Format HH:mm:ss)+'  '+$Text;$script:Panel.LogLines.Enqueue($line);while($script:Panel.LogLines.Count -gt 200){$null=$script:Panel.LogLines.Dequeue()};$script:Panel.LogDirty=$true}
function Update-PanelLogView {if($script:Panel.LogDirty){$log.Text=($script:Panel.LogLines.ToArray()-join "`r`n");$log.SelectionStart=$log.TextLength;$log.ScrollToCaret();$script:Panel.LogDirty=$false}}
function Save-PanelLedger {
    if(-not $script:Panel.Ledger){return}
    $temporary=Join-Path $script:Panel.Session 'ledger.next.json';$final=Join-Path $script:Panel.Session 'ledger.json'
    [IO.File]::WriteAllText($temporary,($script:Panel.Ledger|ConvertTo-Json -Depth 15),[Text.UTF8Encoding]::new($false));[IO.File]::Move($temporary,$final,$true)
}
function Set-PanelStatus([string]$Text,[string]$Explanation='',[switch]$ErrorState){$status.Text=$Text;$detail.Text=$Explanation;$status.ForeColor=if($ErrorState){[Drawing.Color]::FromArgb(166,47,45)}else{[Drawing.Color]::FromArgb(20,83,106)}}
function Get-PanelCurrentJob {if($script:Panel.Phase -notin @('idle','complete','failed','stopped') -and $script:Panel.Queue.Count -and $script:Panel.Index -lt $script:Panel.Queue.Count){return $script:Panel.Queue[$script:Panel.Index]};if($grid.CurrentRow){return $grid.CurrentRow.Tag};return $null}
function Update-PanelButtons {
    $idle=$script:Panel.Phase -in @('idle','complete','failed','stopped');$waiting=$script:Panel.Phase -eq 'waiting'
    $startButton.Enabled=$idle;$onlyButton.Enabled=$idle;$launchButton.Enabled=$waiting -and -not $script:Panel.StopRequested
    $nextButton.Enabled=$script:Panel.Phase -eq 'result';$stopButton.Enabled=-not $idle -and -not $script:Panel.StopRequested
    $logsButton.Enabled=[bool]$script:Panel.Session;$grid.ReadOnly=-not $idle
    $current=Get-PanelCurrentJob;if($current){$songTitle.Text=$current.display_name+"`r`nIN"
        if(-not $idle -and $grid.CurrentRow.Tag.library_id -ne $current.library_id){foreach($row in $grid.Rows){if($row.Tag.library_id -eq $current.library_id){$grid.CurrentCell=$row.Cells['Song'];break}}}
    }
}
function Set-PanelRowStatus($Job,[string]$Text){foreach($row in $grid.Rows){if($row.Tag.library_id -eq $Job.library_id){$row.Cells['Result'].Value=$Text}}}
function Stop-PanelQueue([string]$Reason,[switch]$Failed) {
    $script:Panel.Phase=if($Failed){'failed'}else{'stopped'}
    if($script:Panel.Ledger){$script:Panel.Ledger.status=$script:Panel.Phase;$script:Panel.Ledger.failure=$Reason;$script:Panel.Ledger.stopped_utc=[DateTime]::UtcNow.ToString('o');Save-PanelLedger}
    Set-PanelStatus $(if($Failed){'本輪停止，佇列未繼續'}else{'已停止'}) $Reason -ErrorState:$Failed
    Add-PanelLog $Reason;Update-PanelButtons
}
function Start-PanelNative($Job,[switch]$CheckOnly) {
    if($script:Panel.Process){throw '已有程序執行中。'}
    $ordinal=if($CheckOnly){$script:Panel.CheckIndex+1}else{$script:Panel.Index+1}
    $prefix=if($CheckOnly){'check'}else{'round'}
    $out=Join-Path $script:Panel.Session ('{0}-{1:D2}' -f $prefix,$ordinal)
    if(Test-Path -LiteralPath $out){throw '本輪輸出目錄已存在，拒絕覆寫。'}
    $arguments=New-PanelArguments $script:Panel.Context $Job $out -CheckOnly:$CheckOnly
    $operation=[PasSweep.NativeOperation]::new()
    $script:Panel.Purpose=$prefix;$script:Panel.ErrorTail='';$script:Panel.SuppressStdoutJson=$false;$script:Panel.AcquisitionKey=''
    if($CheckOnly){$script:Panel.Phase='checking';Set-PanelStatus "離線檢查 $ordinal / $($script:Panel.Queue.Count)" $Job.display_name;Add-PanelLog ('離線檢查：'+$Job.display_name)}
    else {
        $script:Panel.Round=$out;$script:Panel.Phase='starting';$script:Panel.ReadySeen=$false;$script:Panel.Started=$null
        $entry=[ordered]@{order=$ordinal;library_id=$Job.library_id;track_id=$Job.track_id;status='starting';directory=$out;exit_code=$null;failure=$null;runtime_failure=$null;controller_failure=$null;kernel_final_zero=$null;manifest_file=$null;manifest_sha256=$null;manifest_identity_matched=$null;counters_consistent=$null;release_evidence_consistent=$null;execution_state='not_started';result_state='not_confirmed';release_state='unknown';judgment='unreviewed';evidence_files=@();stop_requested=$false;acquisition_status=$null;acquisition_progress_lines=0}
        $script:Panel.Ledger.rounds+=@($entry);$script:Panel.Ledger.status='running';Save-PanelLedger;Set-PanelRowStatus $Job '啟動中';Set-PanelStatus '啟動中，請稍候 READY' '先停在遊戲選曲頁，不要按 Play。'
        Add-PanelLog ('已啟動：'+$Job.display_name+'；等待程式 READY。')
    }
    Invoke-PanelOwnedStart $script:Panel $operation $script:Panel.Context.Executable $script:Panel.Context.Bin $arguments (Join-Path $script:Panel.Session "$prefix-$ordinal.stdout.log") (Join-Path $script:Panel.Session "$prefix-$ordinal.stderr.log")
    Update-PanelButtons
}
function Start-PanelHash([string]$Purpose){
    $hash=[PasSweep.HashOperation]::new();$pins=[Collections.Generic.List[PasSweep.Pin]]::new()
    foreach($entry in $script:Panel.Context.Pins.GetEnumerator()){$pins.Add([PasSweep.Pin]::new($entry.Key,$entry.Value))}
    $script:Panel.Hash=$hash;$script:Panel.HashPurpose=$Purpose;$script:Panel.Phase='hashing'
    Set-PanelStatus $(if($Purpose -eq 'round'){'啟動前重新核對封存檔案'}else{'核對封存檔案'}) '曲庫、譜面、設定與程式將全部核對；此步沒有裝置操作。'
    $hash.Start($pins.ToArray());Update-PanelButtons
}
function Begin-PanelQueue([bool]$OnlyCurrent) {
    if($script:Panel.Hash -or $script:Panel.Process -or $script:Panel.Phase -notin @('idle','complete','failed','stopped')){throw '目前佇列尚未結束。'}
    $grid.EndEdit()|Out-Null
    $selected=if($OnlyCurrent){@($grid.CurrentRow.Tag)}else{@($grid.Rows|Where-Object{$_.Cells['Selected'].Value -eq $true}|ForEach-Object{$_.Tag})}
    if(-not $selected.Count){throw '請先勾選至少一首曲目。'}
    foreach($job in $selected){if($job.ready_for_manual_preflight -ne $true -or @($job.blockers).Count){throw "此曲仍有準備阻擋：$($job.display_name)"}}
    $script:Panel.Context=Get-PanelContext $script:Panel.Context.ConfigPath $PSCommandPath
    $fresh=@{};foreach($job in $script:Panel.Context.Jobs){$fresh[$job.library_id]=$job}
    $selected=@($selected|ForEach-Object{if(-not $fresh.ContainsKey($_.library_id)){throw '已選曲目不在重新核對的曲庫中。'};$fresh[$_.library_id]})
    foreach($job in $selected){if($job.ready_for_manual_preflight -ne $true -or @($job.blockers).Count){throw "此曲仍有準備阻擋：$($job.display_name)"}}
    $script:Panel.Queue=$selected;$script:Panel.Index=0;$script:Panel.CheckIndex=0;$script:Panel.StopRequested=$false;$script:Panel.Session='';$script:Panel.Ledger=$null;$script:Panel.PendingFailure=''
    Start-PanelHash 'queue'
}
function Request-PanelQueueStop {
    $script:Panel.StopRequested=$true
    if($script:Panel.Hash){$script:Panel.Hash.Cancel()}
    if($script:Panel.Purpose -eq 'round' -and $script:Panel.Process){$null=Request-PanelStop $script:Panel.Round
        $script:Panel.Ledger.rounds[-1].stop_requested=$true;$script:Panel.Ledger.status='stop_requested';Save-PanelLedger
        Set-PanelStatus '停止要求已送出，正在等待釋放' '介面會保留，直到原生程序結束並讀取 kernel／釋放紀錄。'
    }elseif(-not $script:Panel.Hash -and -not $script:Panel.Process){Stop-PanelQueue '操作者停止佇列。'}
    else{Set-PanelStatus '正在停止準備程序' '不會啟動任何後續實測。'}
    Update-PanelButtons
}
function Invoke-PanelAction([scriptblock]$Action){try{& $Action}catch{
    $reason=$_.Exception.Message
    if($script:Panel.Hash -or $script:Panel.Process){
        $script:Panel.StopRequested=$true;$script:Panel.PendingFailure=$reason
        if($script:Panel.Hash){$script:Panel.Hash.Cancel()}
        if($script:Panel.Process -and $script:Panel.Purpose -eq 'round'){try{$null=Request-PanelStop $script:Panel.Round}catch{}}
        Set-PanelStatus '介面發生錯誤，等待安全停止' $reason -ErrorState;Update-PanelButtons
    }else{Stop-PanelQueue $reason -Failed}
}}
$startButton.Add_Click({Invoke-PanelAction {Begin-PanelQueue $false}})
$onlyButton.Add_Click({Invoke-PanelAction {Begin-PanelQueue $true}})
$launchButton.Add_Click({Invoke-PanelAction {if($script:Panel.Phase -ne 'waiting' -or $script:Panel.StopRequested){throw '目前不在等待人工選曲的階段。'};Start-PanelHash 'round'}})
$nextButton.Add_Click({Invoke-PanelAction {
    if($script:Panel.Phase -ne 'result'){return};$script:Panel.Index++
    if($script:Panel.Index -ge $script:Panel.Queue.Count){$script:Panel.Phase='complete';$script:Panel.Ledger.status='completed';Save-PanelLedger;Set-PanelStatus '本次佇列完成' '原生結果與分數仍需個別檢視。'}
    else{$script:Panel.Phase='waiting';$script:Panel.Ledger.status='waiting_for_operator';Save-PanelLedger;Set-PanelStatus '請選擇下一首歌曲' "第 $($script:Panel.Index+1) / $($script:Panel.Queue.Count) 首；選好 IN 後再啟動本首。"};Update-PanelButtons
}})
$stopButton.Add_Click({Invoke-PanelAction {Request-PanelQueueStop}})
$logsButton.Add_Click({if($script:Panel.Session){$explorer=[Diagnostics.ProcessStartInfo]::new('explorer.exe');$explorer.UseShellExecute=$true;$explorer.ArgumentList.Add($script:Panel.Session);[Diagnostics.Process]::Start($explorer)|Out-Null}})
$grid.Add_CurrentCellDirtyStateChanged({if($grid.IsCurrentCellDirty){$grid.CommitEdit([Windows.Forms.DataGridViewDataErrorContexts]::Commit)|Out-Null}})
$grid.Add_SelectionChanged({Update-PanelButtons})
$form.Add_FormClosing({param($sender,$eventArgs)
    if($script:Panel.Hash -or $script:Panel.Process){$eventArgs.Cancel=$true;$script:Panel.CloseRequested=$true;Invoke-PanelAction {Request-PanelQueueStop}}
})
$timer=[Windows.Forms.Timer]::new();$timer.Interval=100
$timer.Add_Tick({Invoke-PanelAction {
    if($script:Panel.Hash){
        $notice=$null;while($script:Panel.Hash.Queue.TryDequeue([ref]$notice)){if($notice.Kind -eq 'hash'){$detail.Text="核對 $($notice.Count) / $($script:Panel.Context.Pins.Count) 個檔案"}}
        if($script:Panel.Hash.Done){$operation=$script:Panel.Hash;$script:Panel.Hash=$null
            if($script:Panel.StopRequested){Stop-PanelQueue '操作者取消離線準備。'}
            elseif(-not $operation.Success){Stop-PanelQueue $operation.Error -Failed}
            elseif($script:Panel.HashPurpose -eq 'round'){Start-PanelNative (Get-PanelCurrentJob)}
            else {
                $session=Join-Path $script:Panel.Context.Configuration.output_parent ('chapter-IN-panel-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,6))
                if(Test-Path -LiteralPath $session){throw '新的紀錄目錄發生衝突。'};New-Item -ItemType Directory -Path $session|Out-Null;$script:Panel.Session=$session
                $script:Panel.Ledger=[ordered]@{schema='pas.manual-IN-panel-ledger.v1';status='offline_preflight';configuration=$script:Panel.Context.ConfigPath;configuration_sha256=(Get-FileHash -LiteralPath $script:Panel.Context.ConfigPath).Hash.ToLowerInvariant();library=$script:Panel.Context.Configuration.library;plan=$script:Panel.Context.Configuration.plan;selected_ids=@($script:Panel.Queue|ForEach-Object library_id);started_utc=[DateTime]::UtcNow.ToString('o');automatic_play=$false;automatic_retry=$false;failure=$null;stopped_utc=$null;preflights=@();rounds=@()};Save-PanelLedger
                Start-PanelNative $script:Panel.Queue[0] -CheckOnly
            }
        }
    }
    if($script:Panel.Process){
        if($script:Panel.Process.LogError -and -not $script:Panel.StopRequested){$script:Panel.PendingFailure='紀錄寫入失敗：'+$script:Panel.Process.LogError;Request-PanelQueueStop}
        if($script:Panel.StopRequested -and $script:Panel.Purpose -eq 'round'){$null=Request-PanelStop $script:Panel.Round}
        $line=$null;$drained=0
        while($drained++ -lt 120 -and $script:Panel.Process.TryRead([ref]$line)){
            if($line.Kind -eq 'stderr'){
                Add-PanelLog $line.Text;$script:Panel.ErrorTail=($script:Panel.ErrorTail+' '+$line.Text)
                if($script:Panel.ErrorTail.Length -gt 1500){$script:Panel.ErrorTail=$script:Panel.ErrorTail.Substring($script:Panel.ErrorTail.Length-1500)}
            }elseif($line.Kind -eq 'stdout'){
                $update=if($script:Panel.Purpose -eq 'round'){Get-PanelNativeStageUpdate $line.Text $script:Panel.Phase $script:Panel.StopRequested}else{$null}
                if($update){
                    if($update.Kind -eq 'acquisition'){
                        $script:Panel.Ledger.rounds[-1].acquisition_progress_lines++
                        if($script:Panel.AcquisitionKey -ne $update.Key){
                            $script:Panel.AcquisitionKey=$update.Key;$script:Panel.Ledger.rounds[-1].acquisition_status=$update.Progress
                            $script:Panel.Ledger.rounds[-1].status=$update.Progress.stage;Save-PanelLedger
                            Set-PanelStatus $update.Title $update.Detail;Add-PanelLog $update.Log
                            Set-PanelRowStatus (Get-PanelCurrentJob) $(switch($update.Progress.stage){'waiting_for_manual_play'{'等候 Play'}'waiting_for_valid_HUD'{'HUD 未通過'}default{'等待首音'}})
                        }
                    }else{
                        $script:Panel.Phase=$update.Phase;$script:Panel.Ledger.rounds[-1].status=$update.Phase
                        if($update.Kind -eq 'ready'){$script:Panel.ReadySeen=$true}
                        if($update.Kind -eq 'playing'){$script:Panel.Started=[Diagnostics.Stopwatch]::StartNew();$script:Panel.Ledger.rounds[-1].execution_state='in_progress';Set-PanelRowStatus (Get-PanelCurrentJob) '演奏中'}
                        if($update.Kind -eq 'awaiting_result'){$script:Panel.Ledger.rounds[-1].execution_state='completed';$script:Panel.Ledger.rounds[-1].result_state='waiting_for_confirmation'}
                        Save-PanelLedger;Set-PanelStatus $update.Title $update.Detail;Add-PanelLog $update.Log
                    }
                }elseif(-not $line.Text.StartsWith('ACQUISITION STATUS: ',[StringComparison]::Ordinal)){
                    # Pretty JSON remains complete in stdout.log; the panel
                    # presents lifecycle summaries rather than scrolling it.
                    if($line.Text.TrimStart().StartsWith('{',[StringComparison]::Ordinal)){$script:Panel.SuppressStdoutJson=$true}
                    elseif(-not $script:Panel.SuppressStdoutJson){Add-PanelLog $line.Text}
                }
            }
        }
        if($script:Panel.Process.Complete){
            $exitCode=$script:Panel.Process.ExitCode;$purpose=$script:Panel.Purpose;$dropped=$script:Panel.Process.DroppedLines;$limited=$script:Panel.Process.LogLimit
            $script:Panel.Process.Dispose();$script:Panel.Process=$null
            if($purpose -eq 'check'){
                $job=$script:Panel.Queue[$script:Panel.CheckIndex];$script:Panel.Ledger.preflights+=@([ordered]@{library_id=$job.library_id;exit_code=$exitCode;error=$script:Panel.ErrorTail;dropped_ui_lines=$dropped;log_limited=$limited});Save-PanelLedger
                if($script:Panel.StopRequested){Stop-PanelQueue $(if($script:Panel.PendingFailure){$script:Panel.PendingFailure}else{'操作者取消；未啟動實測。'}) -Failed:([bool]$script:Panel.PendingFailure)}
                elseif($exitCode -ne 0){Stop-PanelQueue ("離線檢查失敗："+$job.display_name+'；'+$script:Panel.ErrorTail) -Failed}
                else{
                    Assert-PanelPreflight (Join-Path $script:Panel.Session "check-$($script:Panel.CheckIndex+1).stdout.log") $job $script:Panel.Context
                    Add-PanelLog ('離線檢查通過：'+$job.display_name)
                    $script:Panel.CheckIndex++
                    if($script:Panel.CheckIndex -lt $script:Panel.Queue.Count){Start-PanelNative $script:Panel.Queue[$script:Panel.CheckIndex] -CheckOnly}
                    else{$script:Panel.Phase='waiting';$script:Panel.Ledger.status='waiting_for_operator';Save-PanelLedger;Set-PanelStatus '離線檢查完成，請先選好本首 IN' '選曲後按「已選曲，啟動本首」；看到 READY 再按遊戲 Play。'}
                }
            }else{
                $result=Get-PanelRoundResult $script:Panel.Round $exitCode $script:Panel.ErrorTail (Get-PanelCurrentJob) $script:Panel.Context;$entry=$script:Panel.Ledger.rounds[-1]
                $entry.exit_code=$exitCode;$entry.failure=$result.Failure;$entry.kernel_final_zero=$result.KernelFinalZero;$entry.manifest_file=$result.ManifestFile;$entry.manifest_sha256=$result.ManifestSha256
                $entry.execution_state=$result.ExecutionState;$entry.result_state=$result.ResultState;$entry.release_state=$result.ReleaseState;$entry.judgment=$result.JudgmentState;$entry.evidence_files=@($result.EvidenceFiles)
                $entry.manifest_identity_matched=$result.ManifestIdentityMatched;$entry.counters_consistent=$result.CountersConsistent;$entry.release_evidence_consistent=$result.ReleaseEvidenceConsistent
                $entry.controller_failure=if($script:Panel.PendingFailure){$script:Panel.PendingFailure}else{$null}
                $entry.runtime_failure=if($result.Manifest){$result.Manifest.failure}else{$null};$entry.status=if($result.Success){'sent_result_unreviewed'}else{'failed_or_stopped'};Save-PanelLedger
                $presentation=Get-PanelRoundPresentation $result
                if($script:Panel.PendingFailure){Set-PanelRowStatus (Get-PanelCurrentJob) '介面停止';Stop-PanelQueue $script:Panel.PendingFailure -Failed}
                elseif(-not $result.Success){
                    Set-PanelRowStatus (Get-PanelCurrentJob) $presentation.RowStatus;Stop-PanelQueue ([string]$result.Failure) -Failed
                    if($result.ExecutionState -eq 'completed'){Set-PanelStatus $presentation.Title $presentation.Detail;Add-PanelLog ($presentation.Title+'。'+$presentation.Detail)}
                }
                elseif($script:Panel.StopRequested){Stop-PanelQueue '停止後已取得原生結束紀錄；佇列不再繼續。'}
                else{
                    Set-PanelRowStatus (Get-PanelCurrentJob) $presentation.RowStatus;Add-PanelLog ('本首完成：'+(Get-PanelCurrentJob).display_name+'；'+$presentation.Detail);$progress.Value=[Math]::Min(1000,[int](1000*($script:Panel.Index+1)/$script:Panel.Queue.Count))
                    if($script:Panel.Index+1 -eq $script:Panel.Queue.Count){$script:Panel.Phase='complete';$script:Panel.Ledger.status='completed';Save-PanelLedger;Set-PanelStatus '本次佇列完成' $presentation.Detail}
                    else{$script:Panel.Phase='result';$script:Panel.Ledger.status='waiting_for_next_selection';Save-PanelLedger;Set-PanelStatus $presentation.Title ($presentation.Detail+' 按「下一首」後，由你重新選曲。')}
                }
            };Update-PanelButtons
        }
    }
    if($script:Panel.Phase -eq 'playing' -and $script:Panel.Started){$detail.Text="第 $($script:Panel.Index+1) / $($script:Panel.Queue.Count) 首　·　本首經過 $($script:Panel.Started.Elapsed.ToString('mm\:ss'))（主機時間）"}
    Update-PanelLogView
    if($script:Panel.CloseRequested -and -not $script:Panel.Hash -and -not $script:Panel.Process){$timer.Stop();$form.Close()}
}})
Update-PanelButtons
if($RenderPreview){
    $preview=[IO.Path]::GetFullPath($RenderPreview);if(Test-Path -LiteralPath $preview){throw '預覽輸出已存在。'}
    Set-PanelStatus '畫面預覽｜未執行任何測試' '21 首曲目 · 手動選曲 → 原生 READY → 手動 Play'
    Add-PanelLog '離線畫面預覽。沒有裝置連線、原生程序或輸入。'
    if($PreviewStage -ne 'default'){
        if($PreviewStage -in @('input_complete_result_unconfirmed','result_confirmed')){
            $mockResult=[pscustomobject]@{Success=($PreviewStage -eq 'result_confirmed');ExecutionState='completed';ResultState=if($PreviewStage -eq 'result_confirmed'){'confirmed_by_native'}else{'not_confirmed'};ReleaseState='kernel_zero_confirmed';EvidenceFiles=@([pscustomobject]@{file=if($PreviewStage -eq 'result_confirmed'){'preview/result.png'}else{'preview/last-observed.png'}})}
            $presentation=Get-PanelRoundPresentation $mockResult
            $update=[pscustomobject]@{Title=$presentation.Title;Detail=$presentation.Detail;Log=$presentation.Title+'。'+$presentation.Detail}
        }
        elseif($PreviewStage -eq 'playing'){$update=Get-PanelNativeStageUpdate 'FIRST NOTE ACQUIRED: synthetic preview' 'ready'}
        else{
            $mock=[ordered]@{schema='pas.opening-acquisition-progress.v1';stage=$PreviewStage;reason='fixed_glyph_or_negative_space_missing';pause_ok=($PreviewStage -ne 'waiting_for_manual_play');prefix_ok=($PreviewStage -eq 'waiting_for_first_note');score_ok=($PreviewStage -ne 'waiting_for_manual_play');hud_ok=($PreviewStage -eq 'waiting_for_first_note')}
            $update=Get-PanelNativeStageUpdate ('ACQUISITION STATUS: '+($mock|ConvertTo-Json -Compress)) 'ready'
        }
        Set-PanelStatus ('［預覽］'+$update.Title) $update.Detail;Add-PanelLog ('［預覽］'+$update.Log)
    }
    Update-PanelLogView
    function Initialize-PanelPreviewHandles($control){$null=$control.Handle;foreach($child in $control.Controls){Initialize-PanelPreviewHandles $child};$control.PerformLayout()}
    Initialize-PanelPreviewHandles $form;$form.PerformLayout();$bitmap=[Drawing.Bitmap]::new($form.Width,$form.Height)
    try{$form.DrawToBitmap($bitmap,[Drawing.Rectangle]::new(0,0,$form.Width,$form.Height));$bitmap.Save($preview,[Drawing.Imaging.ImageFormat]::Png)}finally{$bitmap.Dispose();$form.Dispose()}
    return
}
$timer.Start()
try{[Windows.Forms.Application]::Run($form)}finally{$timer.Dispose();$form.Dispose()}
