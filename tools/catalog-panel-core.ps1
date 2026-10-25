# Pure data/pin validation for the full-catalog manual panel. No UI, process,
# device discovery, channel or input is started by these functions.
function Get-PanelBlockerText($Job) {
    $messages=foreach($reason in @($Job.blockers)){
        foreach($part in ([string]$reason -split ',')){
            switch($part){
                'capacity_exceeded' {'同時觸控需求超過目前5指容量'}
                'projection_unavailable' {'接點路徑無法完整投影到安全畫面範圍'}
                'invisible_flick' {'Flick投影後沒有足夠的可辨識位移'}
                default {$part}
            }
        }
    }
    return (@($messages|Select-Object -Unique)-join '；')
}
function Select-PanelCatalogJobs($Jobs,[string]$Query='',[string]$Difficulty='',[bool]$ReadyOnly=$false) {
    $terms=@($Query.Trim().ToLowerInvariant() -split '\s+'|Where-Object {$_})
    foreach($job in $Jobs){
        if($Difficulty -and $job.difficulty -cne $Difficulty){continue}
        if($ReadyOnly -and -not $job.ready_for_manual_preflight){continue}
        $haystack=(@($job.display_name,$job.track_id,$job.library_id,$job.difficulty,$job.variant)-join ' ').ToLowerInvariant()
        $matches=$true;foreach($term in $terms){if(-not $haystack.Contains($term)){$matches=$false;break}}
        if($matches){$job}
    }
}
function Assert-CatalogPanelJob($Job,$CatalogChart) {
    foreach($name in @('library_id','track_id','display_name','difficulty')){Assert-PanelText $Job.$name $name}
    if($Job.library_id -cne $CatalogChart.library_id -or $Job.track_id -cne $CatalogChart.track_id -or
       $Job.difficulty -cne $CatalogChart.difficulty -or $Job.difficulty -cnotin @('IN','AT') -or
       $Job.rounds -ne 1 -or $Job.default_selected -isnot [bool] -or $Job.ready_for_manual_preflight -isnot [bool]){
        throw '完整曲庫的曲目／難度／單輪身份不一致。'
    }
    if($Job.chart.sha256 -cne $CatalogChart.chart.sha256){throw '本機執行原譜與曲庫原譜SHA不相同，不能強制別名映射。'}
    if(-not $Job.ready_for_manual_preflight){
        if($Job.default_selected -or -not @($Job.blockers).Count){throw '跳過項目必須列明原因且不得預選。'}
        return
    }
    if(@($Job.blockers).Count){throw '可前檢曲目仍有阻擋原因。'}
    $profile=Read-PanelJson $Job.profile.file
    if($profile.schema -cne 'pas.adaptive-song-profile.v1' -or $profile.song_id -cne $Job.track_id -or
       $profile.difficulty -cne $Job.difficulty -or $profile.chart_sha256 -cne $Job.chart.sha256 -or
       $profile.expected_plan_sha256 -cne $Job.compiled_fingerprint -or
       $profile.expected_contacts -ne $Job.notes -or $profile.expected_steps -ne $Job.events -or $profile.expected_peak -ne $Job.peak -or
       $Job.events -lt 2 -or $Job.events -gt 16000 -or $Job.notes -lt 1 -or $Job.notes -gt [Math]::Floor($Job.events/2) -or
       $Job.peak -lt 1 -or $Job.peak -gt 5){throw '曲庫profile／完整接點計畫身份或資源預算不相同。'}
    if($Job.difficulty -ceq 'AT' -and $profile.hud_policy -cne 'shared-difficulty-subpixel-prefix-score-v4'){
        throw 'AT必須使用獨立難度HUD policy。'
    }
}
function Get-CatalogPanelContext([string]$Path,[string]$ScriptPath) {
    $config=Read-PanelJson $Path
    if($config.schema -cne 'pas.manual-song-panel-launch.v2' -or
       $config.manual_selection -isnot [bool] -or -not $config.manual_selection -or
       $config.manual_start -isnot [bool] -or -not $config.manual_start -or $config.image_retention -cne 'none'){
        throw '完整曲庫面板必須保留人工選曲／Play與不存圖政策。'
    }
    $pins=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($key in @('plan','library','catalog','identity','build_manifest','kernel_profile','capacity_evidence','gui_script')){
        $null=Add-PanelPin $pins $config.$key
    }
    if(@($config.scripts).Count -lt 2 -or @($config.scripts).Count -gt 16){throw '無效腳本數量。'}
    foreach($ref in $config.scripts){$null=Add-PanelPin $pins $ref}
    if([IO.Path]::GetFullPath($config.gui_script.file) -ne [IO.Path]::GetFullPath($ScriptPath)){throw '此GUI與設定封存不同。'}
    if(-not [IO.Path]::IsPathFullyQualified($config.output_parent) -or -not(Test-Path -LiteralPath $config.output_parent -PathType Container)){throw '紀錄父目錄不存在。'}
    if($config.plan.file -ne $config.library.file -or $config.plan.sha256 -ne $config.library.sha256){throw '完整曲庫清單／library身份不一致。'}
    $plan=Read-PanelJson $config.plan.file;$catalog=Read-PanelJson $config.catalog.file
    if($plan.schema -cne 'pas.catalog-test-plan.v1' -or $catalog.schema -cne 'pas.catalog-library.v1' -or
       @($plan.jobs).Count -lt 1 -or @($plan.jobs).Count -gt 10000 -or @($plan.jobs).Count -ne @($catalog.charts).Count -or
       $plan.round_limit -ne @($plan.jobs).Count -or $config.round_limit -ne $plan.round_limit){throw '完整曲庫必須保留每份IN／AT及變體，且回合數有界。'}
    $index=@{};foreach($chart in $catalog.charts){if($index.ContainsKey($chart.library_id)){throw '曲庫ID重複。'};$index[$chart.library_id]=$chart}
    $identity=Read-PanelJson $config.identity.file
    if($identity.schema -cne 'pas.installed-catalog-identity.v1' -or $identity.library.sha256 -cne $config.catalog.sha256 -or
       @($identity.charts).Count -ne @($catalog.charts).Count){throw '安裝版身份報告與曲庫不一致。'}
    $identityIndex=@{};foreach($row in $identity.charts){if($identityIndex.ContainsKey($row.library_id) -or -not $index.ContainsKey($row.library_id)){throw '身份報告缺漏或重複。'};$identityIndex[$row.library_id]=$row}
    $seen=@{};$states=@{}
    foreach($job in $plan.jobs){
        if($seen.ContainsKey($job.library_id) -or -not $index.ContainsKey($job.library_id)){throw '執行清單遺失或重複曲庫身份。'}
        $seen[$job.library_id]=$true;Assert-CatalogPanelJob $job $index[$job.library_id]
        $null=Add-PanelPin $pins $job.chart
        if(-not $job.ready_for_manual_preflight){continue}
        $verified=$identityIndex[$job.library_id]
        if($verified.status -cne 'verified_exact_installed_chart' -or $verified.chart_sha256 -cne $job.chart.sha256){throw '可前檢項目未通過安裝版原譜身份核對。'}
        foreach($key in @('profile','source_manifest','state_calibration','hud_reference')){$null=Add-PanelPin $pins $job.$key}
        $source=Read-PanelJson $job.source_manifest.file;$profile=Read-PanelJson $job.profile.file
        if($source.track_id -cne $job.track_id -or $source.difficulty -cne $job.difficulty -or
           $source.chart.sha256 -cne $job.chart.sha256 -or $profile.source_manifest_sha256 -cne $job.source_manifest.sha256 -or
           $source.package -cne 'com.PigeonGames.Phigros' -or $source.version_name -cne '4.0.1' -or $source.version_code -ne 157){throw '安裝版來源／profile身份不相同。'}
        $base=Split-Path -Parent $job.source_manifest.file
        $sourceChart=Add-PanelPin $pins $source.chart $base
        if($sourceChart -ne $job.chart.file){throw '原譜來源路徑不一致。'}
        if($source.schema -ceq 'pas.installed-catalog-song-source.v1'){
            if($verified.source_manifest.file -ne $job.source_manifest.file -or $verified.source_manifest.sha256 -cne $job.source_manifest.sha256 -or
               $verified.native_audio_sha256 -cne $profile.audio_sha256){throw '原生音源／來源不屬於這份身份核對。'}
            if($source.exact_chart_bytes_verified -ne $true -or $source.audio_identity.format -cne 'native-fsb5' -or
               $source.audio_identity.sha256 -cne $profile.audio_sha256 -or $profile.installed_source_manifest -ne $job.source_manifest.file){throw '原生音源identity／profile不一致。'}
            foreach($package in $source.packages){$null=Add-PanelPin $pins $package}
            $null=Add-PanelPin $pins $source.installed_source
        }else{
            if($source.decoded_audio.sha256 -cne $profile.audio_sha256){throw '原WAV身份不一致。'}
            $null=Add-PanelPin $pins $source.decoded_audio $base
        }
        if(-not $states.ContainsKey($job.state_calibration.file)){
            $states[$job.state_calibration.file]=$true;$dataset=Read-PanelJson $job.state_calibration.file
            if($dataset.schema -cne 'pas.screen-state-dataset.v1' -or @($dataset.templates).Count -gt 64){throw '無效畫面校正。'}
            foreach($template in $dataset.templates){$null=Add-PanelPin $pins ([pscustomobject]@{file=$template.rgb.path;sha256=$template.rgb.sha256}) (Split-Path -Parent $job.state_calibration.file)}
        }
        if($job.difficulty -ceq 'AT'){
            $dataset=Read-PanelJson $job.state_calibration.file
            if($dataset.hud_difficulty -cne 'AT' -or $dataset.hud_glyph_policy -cne 'shared_difficulty_subpixel_prefix_score_v4'){throw 'AT校正字樣與難度不一致。'}
        }
    }
    $archive=[IO.Path]::GetFullPath($config.build_archive)
    if([IO.Path]::GetFullPath($config.build_manifest.file) -ne (Join-Path $archive 'manifest.json')){throw 'Binary封存路徑不一致。'}
    $build=Read-PanelJson $config.build_manifest.file
    if($build.schema -cne 'pas.adaptive-build-archive.v1' -or @($build.artifacts).Count -gt 2048){throw '無效binary封存。'}
    foreach($artifact in $build.artifacts){
        $target=[IO.Path]::GetFullPath((Join-Path $archive $artifact.file))
        if(-not $target.StartsWith($archive+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw '封存路徑越界。'}
        $null=Add-PanelPin $pins ([pscustomobject]@{file=$target;sha256=$artifact.sha256})
    }
    $bin=Join-Path $archive 'bin';$exe=Join-Path $bin 'chart_experiment.exe'
    if(-not $pins.ContainsKey($exe)){throw '執行檔未固定。'}
    foreach($binary in Get-ChildItem -LiteralPath $bin -File){if($binary.Extension -in @('.exe','.dll') -and -not $pins.ContainsKey($binary.FullName)){throw '有未固定binary／DLL。'}}
    $capacity=Read-PanelJson $config.capacity_evidence.file
    foreach($key in @('analysis','host_manifest','retrieval_report')){$null=Add-PanelPin $pins $capacity.$key}
    foreach($ref in $capacity.artifacts){$null=Add-PanelPin $pins $ref}
    if($pins.Count -gt 4096){throw '完整曲庫檔案參照超出有限預算。'}
    $null=Add-PanelPin $pins ([pscustomobject]@{file=[IO.Path]::GetFullPath($Path);sha256=(Get-FileHash -LiteralPath $Path).Hash})
    return [pscustomobject]@{Configuration=$config;Plan=$plan;Library=$plan;Jobs=@($plan.jobs);Pins=$pins;Executable=$exe;Bin=$bin;ConfigPath=[IO.Path]::GetFullPath($Path);FullCatalog=$true;Catalog=$catalog}
}
