#Requires -Version 7.2
<#
Browse a pinned public chart catalogue without a device or native build.
The legacy panel remains the owner of all live checks, STOP and touch release.
SelfTest and LoadCoreOnly never load WinForms, launch a process or use a device.
#>
[CmdletBinding()]
param(
    [string]$Library,
    [string]$LegacyConfiguration,
    [string]$PreviewDirectory,
    [switch]$SelfTest,
    [switch]$LoadCoreOnly
)
$ErrorActionPreference = 'Stop'

function Resolve-CatalogInputPath([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) { throw '請提供檔案或目錄路徑。' }
    $provider = $null; $drive = $null
    $resolved = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Path, [ref]$provider, [ref]$drive)
    if ($provider.Name -cne 'FileSystem') { throw '曲庫入口只接受檔案系統路徑。' }
    return $resolved
}
function Get-CatalogValue($Value, [string]$Name, $Fallback = $null) {
    if ($null -ne $Value -and $Value.PSObject.Properties[$Name]) { return $Value.$Name }
    return $Fallback
}
function Assert-CatalogText($Value, [string]$Name, [int]$Limit = 1024) {
    if ($Value -isnot [string] -or [string]::IsNullOrWhiteSpace($Value) -or
        $Value.Length -gt $Limit -or $Value -match '[\x00-\x1f]') {
        throw "無效的曲庫欄位：$Name"
    }
}
function Resolve-CatalogAssetPath([string]$Root, [string]$Relative) {
    Assert-CatalogText $Relative 'asset.file' 4096
    if ([IO.Path]::IsPathRooted($Relative) -or $Relative -match '[:\\]' -or
        @($Relative.Split('/') | Where-Object { $_ -in @('', '.', '..') }).Count) {
        throw '資產路徑必須相對於曲庫目錄，且不能越界。'
    }
    $base = [IO.Path]::GetFullPath($Root)
    $path = [IO.Path]::GetFullPath((Join-Path $base $Relative))
    $comparison = if ($IsWindows) { [StringComparison]::OrdinalIgnoreCase } else { [StringComparison]::Ordinal }
    if (-not $path.StartsWith($base.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar, $comparison)) {
        throw '資產路徑超出曲庫目錄。'
    }
    return $path
}
function Read-CatalogJson([string]$Path, [long]$MaxBytes = 16777216) {
    $path = Resolve-CatalogInputPath $Path
    $item = Get-Item -LiteralPath $path -ErrorAction Stop
    if ($item.PSIsContainer -or $item.Length -gt $MaxBytes) { throw '曲庫 JSON 過大或不是檔案。' }
    $bytes = [IO.File]::ReadAllBytes($path)
    if ($bytes.Length -gt $MaxBytes) { throw '曲庫 JSON 在讀取時超出限制。' }
    $text = [Text.Encoding]::UTF8.GetString($bytes).TrimStart([char]0xfeff)
    if ($text.StartsWith('version https://git-lfs.github.com/spec/v1', [StringComparison]::Ordinal)) {
        throw '這份索引仍是 Git LFS pointer；請先在本機執行 git lfs pull。'
    }
    return ($text | ConvertFrom-Json -Depth 64)
}
function Get-CatalogStatus($Row) {
    $archive = [string](Get-CatalogValue $Row 'archive_status' 'missing')
    $analysis = [string](Get-CatalogValue $Row 'analysis_status' 'structural_only')
    if ($archive -eq 'lfs_pointer') { return [pscustomobject]@{ Code = 'lfs_pointer'; Text = '待取回 LFS 原件' } }
    if ($archive -eq 'failed') { return [pscustomobject]@{ Code = 'failed'; Text = '原件取得失敗' } }
    if ($archive -ne 'verified') { return [pscustomobject]@{ Code = 'missing'; Text = '原件尚未齊備' } }
    if ($analysis -in @('unsupported', 'unsupported_format')) { return [pscustomobject]@{ Code = 'unsupported'; Text = '已入庫・格式待支援' } }
    if ($analysis -eq 'failed') { return [pscustomobject]@{ Code = 'failed'; Text = '已入庫・分析待處理' } }
    return [pscustomobject]@{ Code = 'local_pending'; Text = '已入庫・待本機核對' }
}
function Get-CatalogLocalPresence([string]$Path) {
    if (-not [IO.File]::Exists($Path)) { return 'missing' }
    try {
        $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
        try {
            $head = [byte[]]::new(128)
            $read = $stream.Read($head, 0, $head.Length)
            if ([Text.Encoding]::UTF8.GetString($head, 0, $read).StartsWith('version https://git-lfs.github.com/spec/v1', [StringComparison]::Ordinal)) { return 'lfs_pointer' }
        } finally { $stream.Dispose() }
        return 'present_unhashed'
    } catch { return 'unreadable' }
}
function Read-CatalogNativeAudit([string]$Root, [string]$LibraryPath, $Charts) {
    $path = Join-Path $Root 'manifests/native-import-report.json'
    $index = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::Ordinal)
    if (-not [IO.File]::Exists($path)) { return [pscustomobject]@{ Rows = $index; Status = '未附原生匯入報告'; Path = $path } }
    try {
        $report = Read-CatalogJson $path
        if ($report.schema -cne 'pas.catalog-native-import-report.v1' -or
            $report.gameplay_qualified -isnot [bool] -or $report.gameplay_qualified -or
            $report.library.sha256 -ine (Get-FileHash -LiteralPath $LibraryPath -Algorithm SHA256).Hash -or
            @($report.charts).Count -ne @($Charts).Count) { throw '報告未綁定這份曲庫的完整身份。' }
        $expected = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::Ordinal)
        foreach ($chart in $Charts) { $expected.Add($chart.library_id, $chart) }
        foreach ($row in $report.charts) {
            if (-not $expected.ContainsKey($row.library_id) -or $index.ContainsKey($row.library_id)) { throw '報告含缺漏或重複身份。' }
            $chart = $expected[$row.library_id]
            if ($row.chart_sha256 -ine $chart.chart.sha256 -or $row.chart_file -cne $chart.chart.file -or
                $row.status -cnotin @('passed', 'unsupported_format', 'rejected', 'timeout', 'integrity_error', 'inspector_error') -or
                $row.gameplay_qualified -isnot [bool] -or $row.gameplay_qualified) { throw '報告與譜面身份不符。' }
            $index.Add($row.library_id, $row)
        }
        return [pscustomobject]@{ Rows = $index; Status = '已綁定原生匯入報告（僅離線解析）'; Path = $path }
    } catch {
        $index.Clear()
        return [pscustomobject]@{ Rows = $index; Status = '原生匯入報告已過期或身份不符，未套用'; Path = $path }
    }
}
function Get-CatalogNativeIssue($Audit) {
    $detail = [string](Get-CatalogValue $Audit 'reason' (Get-CatalogValue $Audit 'stderr' ''))
    if ($detail.Length -gt 4096) { $detail = $detail.Substring(0, 4096) + '…' }
    $summary = switch ([string]$Audit.status) {
        'passed' { '通過嚴格原生匯入；接點編譯、HUD 與實測仍待本機驗證。' }
        'unsupported_format' { '原件已封存；現行原生匯入器尚未支援此譜面格式。' }
        'timeout' { '原生匯入檢查逾時；需要在本機檢視原因並重新驗證。' }
        'integrity_error' { '原件身份核對失敗；請先核對來源與 SHA256。' }
        'inspector_error' { '原生檢查工具未正常完成；請核對工具建置與輸出紀錄。' }
        default {
            if ($detail -match 'reversed event interval') {
                '原譜含起訖倒序的判定線事件，嚴格匯入器因此拒絕；需確認原生語義及相容策略，原件保持不變。'
            } elseif ($detail -match '/blockAreaList: unsupported field') {
                '原譜含 blockAreaList 欄位，目前嚴格 v3 匯入器尚未支援；需要補齊語義與測試。'
            } elseif ($detail -match 'unsupported field') {
                '原譜含目前匯入器未支援的欄位；需要確認該欄位的原生語義。'
            } else {
                '嚴格原生匯入未通過；請依下方技術細節檢視原譜與匯入規則。'
            }
        }
    }
    return [pscustomobject]@{ Summary = $summary; Detail = $detail.Trim() }
}
function Read-SongCatalog([string]$Path) {
    $path = Resolve-CatalogInputPath $Path
    $data = Read-CatalogJson $path
    if ((Get-CatalogValue $data 'schema') -cne 'pas.catalog-library.v1') { throw '不支援的曲庫格式；需要 pas.catalog-library.v1。' }
    $charts = @(Get-CatalogValue $data 'charts' @())
    if ($charts.Count -lt 1 -or $charts.Count -gt 10000) { throw '曲庫必須包含 1–10000 份譜面。' }
    $root = Split-Path -Parent $path
    $nativeAudit = Read-CatalogNativeAudit $root $path $charts
    $ids = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $songs = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($chart in $charts) {
        foreach ($name in @('library_id', 'song_id', 'track_id', 'title', 'difficulty', 'version')) {
            Assert-CatalogText (Get-CatalogValue $chart $name) $name
        }
        if (-not $ids.Add($chart.library_id)) { throw "重複的譜面識別：$($chart.library_id)" }
        if ($chart.difficulty -cnotin @('IN', 'AT')) { throw '此曲庫入口只接受明確 IN／AT 項目。' }
        $liveReady = Get-CatalogValue $chart 'live_ready' $false
        if ($liveReady -isnot [bool] -or $liveReady) { throw '公共曲庫不能宣告本機實測資格。請使用獨立的本機實測面板。' }
        $file = Get-CatalogValue (Get-CatalogValue $chart 'chart') 'file'
        Assert-CatalogText $file 'chart.file' 4096
        if ([IO.Path]::GetExtension($file) -cne '.json') { throw '譜面原件必須是 .json 檔。' }
        $assetPath = Resolve-CatalogAssetPath $root $file
        $hash = [string](Get-CatalogValue $chart.chart 'sha256' '')
        if ($hash -and $hash -cnotmatch '^[0-9a-fA-F]{64}$') { throw '譜面 SHA256 格式錯誤。' }
        if ($chart.archive_status -eq 'verified' -and -not $hash) { throw '已入庫項目缺少 SHA256。' }
        $variant = [string](Get-CatalogValue $chart 'variant' '0')
        Assert-CatalogText $variant 'variant' 64
        $status = Get-CatalogStatus $chart
        $presence = Get-CatalogLocalPresence $assetPath
        if ($presence -eq 'missing') { $status = [pscustomobject]@{ Code = 'missing'; Text = '本機原件尚未取得' } }
        elseif ($presence -eq 'lfs_pointer') { $status = [pscustomobject]@{ Code = 'lfs_pointer'; Text = '待取回 LFS 原件' } }
        elseif ($presence -eq 'unreadable') { $status = [pscustomobject]@{ Code = 'failed'; Text = '本機原件無法讀取' } }
        $composer = [string](Get-CatalogValue $chart 'composer' '')
        $messages = [Collections.Generic.List[string]]::new()
        foreach ($blocker in @(Get-CatalogValue $chart 'blockers' @())) {
            $message = [string](Get-CatalogValue $blocker 'message' '')
            if ($message) { $messages.Add($message) }
        }
        if (-not $messages.Count) { $messages.Add('尚未比對本機遊戲版本、原譜、音源及實測設定。') }
        $nativeStatus = 'not_audited'; $nativeText = $nativeAudit.Status; $nativeDetail = ''
        if ($nativeAudit.Rows.ContainsKey($chart.library_id)) {
            $audit = $nativeAudit.Rows[$chart.library_id]
            $nativeStatus = $audit.status
            $issue = Get-CatalogNativeIssue $audit
            $nativeText = $issue.Summary; $nativeDetail = $issue.Detail
            if ($presence -eq 'present_unhashed' -and $chart.archive_status -eq 'verified') {
                if ($nativeStatus -eq 'passed') { $status = [pscustomobject]@{ Code = 'local_pending'; Text = '匯入通過・待本機核對' } }
                elseif ($nativeStatus -eq 'unsupported_format') { $status = [pscustomobject]@{ Code = 'unsupported'; Text = '已入庫・格式待支援' } }
                else { $status = [pscustomobject]@{ Code = 'import_rejected'; Text = '已入庫・原生匯入拒絕' } }
            }
            if ($nativeStatus -notin @('passed', 'unsupported_format')) { $messages.Insert(0, $nativeText) }
        }
        [void]$songs.Add($chart.song_id)
        $source = Get-CatalogValue $chart 'source'
        $url = [string](Get-CatalogValue $chart.chart 'raw_url' (Get-CatalogValue $source 'url' ''))
        $audio = Get-CatalogValue $chart 'audio'
        $audioPath = ''
        $audioSha = [string](Get-CatalogValue $audio 'sha256' '')
        if ($audio -and (Get-CatalogValue $audio 'file')) {
            if ([IO.Path]::GetExtension($audio.file) -cnotin @('.ogg', '.wav', '.fsb')) { throw '來源音檔格式不支援。' }
            $audioPath = Resolve-CatalogAssetPath $root $audio.file
            if ($audioSha -and $audioSha -cnotmatch '^[0-9a-fA-F]{64}$') { throw '音源 SHA256 格式錯誤。' }
        }
        $rows.Add([pscustomobject]@{
            Id = $chart.library_id; SongId = $chart.song_id; TrackId = $chart.track_id
            Title = $chart.title; Composer = $composer; Difficulty = $chart.difficulty
            Variant = $variant; Rating = Get-CatalogValue $chart 'rating'
            Version = $chart.version; File = $assetPath; Sha256 = $hash
            DeclaredBytes = Get-CatalogValue $chart.chart 'bytes'
            GitBlobSha = Get-CatalogValue $chart.chart 'git_blob_sha' ''
            Notes = Get-CatalogValue $chart 'notes'; LineCount = Get-CatalogValue $chart 'line_count'
            FormatVersion = Get-CatalogValue $chart 'format_version'
            StateCode = $status.Code; StateText = $status.Text
            LocalPresence = $presence
            NativeImportStatus = $nativeStatus; NativeImportText = $nativeText; NativeImportDetail = $nativeDetail; NativeImportReport = $nativeAudit.Path
            Blockers = $messages.ToArray(); Source = $source; SourceUrl = $url; Raw = $chart
            AudioFile = $audioPath; AudioSha256 = $audioSha
            SearchText = (@($chart.title, $composer, $chart.track_id, $chart.song_id, $chart.library_id) -join ' ').ToLowerInvariant()
        })
    }
    $counts = Get-CatalogValue $data 'counts'
    if ($counts -and ((Get-CatalogValue $counts 'chart_count' $charts.Count) -ne $charts.Count -or
        (Get-CatalogValue $counts 'song_count' $songs.Count) -ne $songs.Count)) { throw '曲庫宣告數量與實際項目不一致。' }
    return [pscustomobject]@{
        Path = $path; Root = $root; Rows = $rows.ToArray(); Source = Get-CatalogValue $data 'source'
        SongCount = $songs.Count; ChartCount = $rows.Count
        IN = @($rows | Where-Object Difficulty -CEQ 'IN').Count
        AT = @($rows | Where-Object Difficulty -CEQ 'AT').Count
        Variants = @($rows.Variant | Sort-Object -Unique)
        NativeAuditStatus = $nativeAudit.Status
        SourceData = $data
    }
}
function Select-CatalogRows($Rows, [string]$Query = '', [string]$Difficulty = '', [string]$Variant = '', [string]$Status = '') {
    $terms = @($Query.Trim().ToLowerInvariant() -split '\s+' | Where-Object { $_ })
    foreach ($row in $Rows) {
        if ($Difficulty -and $row.Difficulty -cne $Difficulty) { continue }
        if ($Variant -and $row.Variant -cne $Variant) { continue }
        if ($Status) {
            $stateMatch = switch ($Status) {
                'import_rejected' { $row.NativeImportStatus -cin @('rejected', 'timeout', 'integrity_error', 'inspector_error') }
                'unsupported' { $row.StateCode -ceq 'unsupported' -or $row.NativeImportStatus -ceq 'unsupported_format' -or $row.Raw.analysis_status -cin @('unsupported', 'unsupported_format') }
                default { $row.StateCode -ceq $Status }
            }
            if (-not $stateMatch) { continue }
        }
        $matches = $true
        foreach ($term in $terms) { if (-not $row.SearchText.Contains($term)) { $matches = $false; break } }
        if ($matches) { $row }
    }
}
function Format-CatalogDetails($Row) {
    if (-not $Row) { return '選取一份譜面，即可查看原始檔案、來源與尚待完成的本機準備。' }
    $lines = [Collections.Generic.List[string]]::new()
    foreach ($line in @(
        $Row.Title, $Row.Composer, '',
        "難度：$($Row.Difficulty)    變體：.$($Row.Variant)    定數：$($Row.Rating)",
        "來源版本：$($Row.Version)（與本機版本尚未比對）",
        "狀態：$($Row.StateText)",
        "音符：$($Row.Notes)    判定線：$($Row.LineCount)    格式：$($Row.FormatVersion)",
        '', '原生匯入', $Row.NativeImportText,
        $(if ($Row.NativeImportDetail) { '技術細節：' + [Environment]::NewLine + $Row.NativeImportDetail } else { '' }),
        '', '本機準備', ($Row.Blockers -join [Environment]::NewLine),
        '', '原生識別', $Row.TrackId, '', '譜面原件', $Row.File,
        '', 'SHA256', $Row.Sha256, '', '來源 Git blob', [string]$Row.GitBlobSha,
        '', '來源倉庫', [string](Get-CatalogValue $Row.Source 'repository' ''),
        ('固定 commit：' + [string](Get-CatalogValue $Row.Source 'revision' '')),
        '', '來源網址', $Row.SourceUrl,
        '', '來源音檔', $Row.AudioFile, '音檔 SHA256', $Row.AudioSha256,
        '', '入庫不會啟動模擬器或注入觸控。實測須在獨立面板核對本機版本與設定。'
    )) { $lines.Add([string]$line) }
    return ($lines -join [Environment]::NewLine)
}
function Test-CatalogAsset([string]$Path, [string]$ExpectedSha256, [long]$MaxBytes = 67108864) {
    $Path = Resolve-CatalogInputPath $Path
    if (-not [IO.File]::Exists($Path)) { throw '原件尚未下載；請先執行 git lfs pull，再重新整理曲庫。' }
    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    try {
        if ($stream.Length -gt $MaxBytes) { throw '原件超出讀取預算。' }
        $head = [byte[]]::new([int][Math]::Min(256, $stream.Length))
        $read = $stream.Read($head, 0, $head.Length)
        if ([Text.Encoding]::UTF8.GetString($head, 0, $read).StartsWith('version https://git-lfs.github.com/spec/v1', [StringComparison]::Ordinal)) {
            throw '目前檔案是 Git LFS pointer，尚未取回原件。請在倉庫根目錄執行 git lfs pull。'
        }
        $stream.Position = 0
        $actual = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($stream)).ToLowerInvariant()
        if ($ExpectedSha256 -cnotmatch '^[0-9a-fA-F]{64}$' -or $actual -ine $ExpectedSha256) { throw '原件 SHA256 不符；請先核對或重新取得來源。' }
        return [pscustomobject]@{ File = $Path; Sha256 = $actual; Bytes = $stream.Length; Verified = $true }
    } finally { $stream.Dispose() }
}
function Get-CatalogLegacyLaunch([string]$Configuration) {
    $path = Resolve-CatalogInputPath $Configuration
    $config = Read-CatalogJson $path 1048576
    if ($config.schema -cne 'pas.manual-song-panel-launch.v1' -or
        $config.manual_selection -isnot [bool] -or -not $config.manual_selection -or
        $config.manual_start -isnot [bool] -or -not $config.manual_start) { throw '請選擇既有人工選曲／Play 面板的 launch-config.json。' }
    $gui = Get-CatalogValue $config 'gui_script'
    if (-not $gui -or -not [IO.Path]::IsPathFullyQualified($gui.file) -or
        [IO.Path]::GetFileName($gui.file) -cne 'sweep-panel.ps1') { throw '設定未綁定既有實測面板腳本。' }
    $verified = Test-CatalogAsset $gui.file $gui.sha256 1048576
    return [pscustomobject]@{ Configuration = $path; Script = $verified.File; Sha256 = $verified.Sha256 }
}
function New-CatalogReadOperation([string]$Script, [string]$Operation, [object[]]$Arguments, [string]$After = '') {
    if ($Operation -cnotin @('load', 'verify', 'legacy')) { throw 'Unknown catalogue operation' }
    $shell = [Management.Automation.PowerShell]::Create()
    try {
        [void]$shell.AddScript({
            param($scriptPath, $operation, $arguments)
            . $scriptPath -LoadCoreOnly
            switch ($operation) {
                'load' { Read-SongCatalog $arguments[0] }
                'verify' { Test-CatalogAsset $arguments[0] $arguments[1] }
                'legacy' { Get-CatalogLegacyLaunch $arguments[0] }
            }
        }).AddArgument($Script).AddArgument($Operation).AddArgument($Arguments)
        return @{ Shell = $shell; Async = $shell.BeginInvoke(); Operation = $Operation; After = $After }
    } catch { $shell.Dispose(); throw }
}
function Invoke-CatalogCoreTest {
    $root = Join-Path ([IO.Path]::GetTempPath()) ('pas-catalog-ui-' + [Guid]::NewGuid().ToString('N'))
    [IO.Directory]::CreateDirectory((Join-Path $root 'charts')) | Out-Null
    $checks = 0
    try {
        $native = '{"formatVersion":3,"synthetic":true}'
        $asset = Join-Path $root 'charts/a.json'
        [IO.File]::WriteAllText($asset, $native, [Text.UTF8Encoding]::new($false))
        $sha = (Get-FileHash -LiteralPath $asset -Algorithm SHA256).Hash.ToLowerInvariant()
        $rows = foreach ($difficulty in @('IN', 'AT')) {
            [ordered]@{
                library_id = "test/song/$difficulty"; song_id = 'synthetic.song'; track_id = 'synthetic.song.0'
                title = '測試星光'; composer = "O'Brien & 測試"; difficulty = $difficulty; version = '4.0.1.2'
                variant = '0'; chart = @{ file = 'charts/a.json'; sha256 = $sha; bytes = 34 }
                archive_status = 'verified'; prepare_status = 'blocked'; live_ready = $false
                blockers = @(@{ code = 'version_match_required'; message = '版本仍待本機比對。' })
            }
        }
        $library = Join-Path $root 'library.json'
        $data = [ordered]@{ schema = 'pas.catalog-library.v1'; charts = @($rows); counts = @{ song_count = 1; chart_count = 2 } }
        $data | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $library -Encoding utf8
        $model = Read-SongCatalog $library
        if ($model.ChartCount -ne 2 -or $model.SongCount -ne 1 -or $model.IN -ne 1 -or $model.AT -ne 1) { throw '多難度數量錯誤。' }; $checks++
        if (@(Select-CatalogRows $model.Rows "星光 o'brien" 'AT').Count -ne 1) { throw 'Unicode／作者搜尋錯誤。' }; $checks++
        if (@(Select-CatalogRows $model.Rows 'synthetic.song' '' '0' 'local_pending').Count -ne 2) { throw '變體／狀態篩選錯誤。' }; $checks++
        if (@(Select-CatalogRows $model.Rows '[*]').Count -ne 0) { throw '搜尋錯用 wildcard 語義。' }; $checks++
        if ((Format-CatalogDetails $model.Rows[0]) -notmatch '版本仍待本機比對') { throw '遺漏準備阻擋原因。' }; $checks++
        if (-not (Test-CatalogAsset $asset $sha).Verified) { throw '原件 SHA 核對失敗。' }; $checks++
        foreach ($bad in @('../outside.json', 'charts/../../outside.json', '/etc/passwd', 'charts\bad.json', 'https://example.com/a.json')) {
            $rejected = $false; try { Resolve-CatalogAssetPath $root $bad | Out-Null } catch { $rejected = $true }
            if (-not $rejected) { throw '越界路徑未拒絕。' }; $checks++
        }
        [IO.File]::WriteAllText($asset, 'version https://git-lfs.github.com/spec/v1')
        $rejected = $false; try { Test-CatalogAsset $asset $sha | Out-Null } catch { $rejected = $_.Exception.Message -match 'LFS pointer' }
        if (-not $rejected) { throw 'LFS pointer 被當作原件。' }; $checks++
        [IO.File]::WriteAllText($asset, 'changed')
        $rejected = $false; try { Test-CatalogAsset $asset $sha | Out-Null } catch { $rejected = $true }
        if (-not $rejected) { throw '變動原件未拒絕。' }; $checks++
        $data.charts[0].live_ready = $true
        $data | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $library -Encoding utf8
        $rejected = $false; try { Read-SongCatalog $library | Out-Null } catch { $rejected = $true }
        if (-not $rejected) { throw '公共索引錯誤授予了實測資格。' }; $checks++
        return [pscustomobject]@{
            schema = 'pas.catalog-panel-selftest.v1'; status = 'passed'; checks = $checks
            windows_shown = 0; native_processes_started = 0; device_access = $false; tests_scope = 'offline data, identity, search and file handling'
        }
    } finally { if ([IO.Directory]::Exists($root)) { Remove-Item -LiteralPath $root -Recurse -Force } }
}

if ($LoadCoreOnly) { return }
if ($SelfTest) { Invoke-CatalogCoreTest | ConvertTo-Json -Depth 4; return }
if (-not $IsWindows) { throw '曲庫視窗需要 Windows 與 PowerShell 7.2+；離線資料檢查可使用 -SelfTest。' }
if (-not $Library) { $Library = Join-Path (Split-Path -Parent $PSScriptRoot) 'assets/phigros/4.0.1.2/library.json' }
$Library = Resolve-CatalogInputPath $Library
if ($LegacyConfiguration) { $LegacyConfiguration = Resolve-CatalogInputPath $LegacyConfiguration }
if ($PreviewDirectory) { $PreviewDirectory = Resolve-CatalogInputPath $PreviewDirectory }
if ([Threading.Thread]::CurrentThread.GetApartmentState() -ne [Threading.ApartmentState]::STA) { throw '請使用 pwsh -STA -File tools/song-library-panel.ps1 開啟曲庫。' }
if ($PreviewDirectory -and $LegacyConfiguration) { throw '畫面預覽不能同時指定本機實測設定。' }

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[void][Windows.Forms.Application]::SetHighDpiMode([Windows.Forms.HighDpiMode]::PerMonitorV2)
[Windows.Forms.Application]::EnableVisualStyles()

$script:CatalogUi = @{
    Model = $null; Job = $null; Visible = @(); Updating = $false; Closing = $false
    Legacy = $null; PendingLegacy = $LegacyConfiguration; LegacyPath = $LegacyConfiguration
    Script = $PSCommandPath; Library = $Library
}
$ink = [Drawing.Color]::FromArgb(25, 42, 58)
$muted = [Drawing.Color]::FromArgb(85, 103, 120)
$accent = [Drawing.Color]::FromArgb(13, 106, 119)
$background = [Drawing.Color]::FromArgb(244, 247, 250)
$border = [Drawing.Color]::FromArgb(218, 226, 233)
$form = [Windows.Forms.Form]::new()
$form.Text = 'Phigros 譜面曲庫'
$form.StartPosition = 'CenterScreen'
$form.ClientSize = [Drawing.Size]::new(1260, 780)
$form.MinimumSize = [Drawing.Size]::new(1000, 640)
$form.AutoScaleMode = [Windows.Forms.AutoScaleMode]::Dpi
$form.AutoScaleDimensions = [Drawing.SizeF]::new(96, 96)
$form.Font = [Drawing.Font]::new('Microsoft JhengHei UI', 10)
$form.BackColor = $background
$form.ForeColor = $ink
$form.KeyPreview = $true
$form.AutoScroll = $true
$layout = [Windows.Forms.TableLayoutPanel]::new()
$layout.Dock = 'Fill'; $layout.Padding = [Windows.Forms.Padding]::new(24, 20, 24, 12)
$layout.ColumnCount = 1; $layout.RowCount = 6
foreach ($mode in @('AutoSize', 'AutoSize', 'AutoSize', 'Percent', 'AutoSize', 'AutoSize')) {
    $style = [Windows.Forms.RowStyle]::new()
    $style.SizeType = [Windows.Forms.SizeType]::$mode
    if ($mode -eq 'Percent') { $style.Height = 100 }
    [void]$layout.RowStyles.Add($style)
}
$form.Controls.Add($layout)
$header = [Windows.Forms.TableLayoutPanel]::new()
$header.Dock = 'Fill'; $header.AutoSize = $true; $header.ColumnCount = 1; $header.RowCount = 2
$heading = [Windows.Forms.Label]::new()
$heading.Text = '譜面曲庫'; $heading.AutoSize = $true
$heading.Font = [Drawing.Font]::new('Microsoft JhengHei UI', 24, [Drawing.FontStyle]::Bold)
$heading.Margin = [Windows.Forms.Padding]::new(0, 0, 0, 4)
$subheading = [Windows.Forms.Label]::new()
$subheading.Text = '瀏覽全曲 IN／AT、核對原件與來源，再接續本機實測。'
$subheading.AutoSize = $true; $subheading.ForeColor = $muted
$subheading.Margin = [Windows.Forms.Padding]::new(1, 0, 0, 16)
$header.Controls.Add($heading, 0, 0); $header.Controls.Add($subheading, 0, 1)
$layout.Controls.Add($header, 0, 0)

$summary = [Windows.Forms.FlowLayoutPanel]::new()
$summary.AutoSize = $true; $summary.Dock = 'Fill'; $summary.WrapContents = $true
$summary.Margin = [Windows.Forms.Padding]::new(0, 0, 0, 14)
$statLabels = @{}
foreach ($spec in @(@('songs', '— 首歌曲'), @('charts', '— 份譜面'), @('IN', '— IN'), @('AT', '— AT'))) {
    $label = [Windows.Forms.Label]::new()
    $label.Text = $spec[1]; $label.AutoSize = $true; $label.BackColor = [Drawing.Color]::White
    $label.Padding = [Windows.Forms.Padding]::new(16, 9, 16, 9)
    $label.Margin = [Windows.Forms.Padding]::new(0, 0, 10, 4)
    $label.Font = [Drawing.Font]::new('Microsoft JhengHei UI', 11, [Drawing.FontStyle]::Bold)
    $label.AccessibleName = $spec[1]
    $summary.Controls.Add($label); $statLabels[$spec[0]] = $label
}
$layout.Controls.Add($summary, 0, 1)

$filters = [Windows.Forms.FlowLayoutPanel]::new()
$filters.AutoSize = $true; $filters.Dock = 'Fill'; $filters.WrapContents = $true
$filters.Margin = [Windows.Forms.Padding]::new(0, 0, 0, 12)
$search = [Windows.Forms.TextBox]::new()
$search.Width = 340; $search.PlaceholderText = '搜尋曲名、作者或 track ID（Ctrl+F）'
$search.AccessibleName = '搜尋曲名、作者或原生識別'
$search.Margin = [Windows.Forms.Padding]::new(0, 2, 10, 8)
$filters.Controls.Add($search)
function New-CatalogFilter([string]$Name, [string[]]$Items, [int]$Width) {
    $combo = [Windows.Forms.ComboBox]::new()
    $combo.DropDownStyle = 'DropDownList'; $combo.Width = $Width
    $combo.AccessibleName = $Name; $combo.Margin = [Windows.Forms.Padding]::new(0, 2, 10, 8)
    foreach ($item in $Items) { [void]$combo.Items.Add($item) }
    $combo.SelectedIndex = 0; $filters.Controls.Add($combo)
    return $combo
}
$difficultyFilter = New-CatalogFilter '難度篩選' @('全部難度', 'IN', 'AT') 110
$variantFilter = New-CatalogFilter '資產變體篩選' @('全部變體') 130
$stateFilter = New-CatalogFilter '準備狀態篩選' @('全部準備狀態', '待本機核對', '格式待支援', '原生匯入拒絕', '原件未齊備', 'LFS 尚未取回', '取得／分析失敗') 190
$stateCodes = @('', 'local_pending', 'unsupported', 'import_rejected', 'missing', 'lfs_pointer', 'failed')
function New-CatalogButton([string]$Text, [bool]$Primary = $false) {
    $button = [Windows.Forms.Button]::new()
    $button.Text = $Text; $button.AutoSize = $true
    $button.Padding = [Windows.Forms.Padding]::new(10, 4, 10, 4)
    $button.Margin = [Windows.Forms.Padding]::new(0, 0, 8, 7)
    $button.FlatStyle = 'Flat'; $button.FlatAppearance.BorderColor = $border
    $button.BackColor = if ($Primary) { $accent } else { [Drawing.Color]::White }
    $button.ForeColor = if ($Primary) { [Drawing.Color]::White } else { $ink }
    return $button
}
$resetButton = New-CatalogButton '清除篩選'
$reloadButton = New-CatalogButton '重新整理'
$filters.Controls.Add($resetButton); $filters.Controls.Add($reloadButton)
$layout.Controls.Add($filters, 0, 2)

$split = [Windows.Forms.SplitContainer]::new()
$split.Dock = 'Fill'; $split.Size = [Drawing.Size]::new(1200, 430)
$split.SplitterDistance = 720; $split.Panel1MinSize = 410; $split.Panel2MinSize = 300
$split.SplitterWidth = 12; $split.Margin = [Windows.Forms.Padding]::new(0)
$left = [Windows.Forms.TableLayoutPanel]::new()
$left.Dock = 'Fill'; $left.RowCount = 2; $left.ColumnCount = 1
[void]$left.RowStyles.Add([Windows.Forms.RowStyle]::new([Windows.Forms.SizeType]::AutoSize))
[void]$left.RowStyles.Add([Windows.Forms.RowStyle]::new([Windows.Forms.SizeType]::Percent, 100))
$resultsLabel = [Windows.Forms.Label]::new()
$resultsLabel.AutoSize = $true; $resultsLabel.Text = '正在載入曲庫…'
$resultsLabel.Margin = [Windows.Forms.Padding]::new(0, 0, 0, 10); $resultsLabel.ForeColor = $muted
$left.Controls.Add($resultsLabel, 0, 0)
$grid = [Windows.Forms.DataGridView]::new()
$bufferProperty = $grid.GetType().GetProperty('DoubleBuffered', [Reflection.BindingFlags]'Instance,NonPublic')
if ($bufferProperty) { $bufferProperty.SetValue($grid, $true) }
$grid.Dock = 'Fill'; $grid.ReadOnly = $true; $grid.AllowUserToAddRows = $false
$grid.AllowUserToDeleteRows = $false; $grid.AllowUserToResizeRows = $false
$grid.RowHeadersVisible = $false; $grid.MultiSelect = $false; $grid.SelectionMode = 'FullRowSelect'
$grid.AutoSizeColumnsMode = 'Fill'; $grid.BorderStyle = 'None'; $grid.BackgroundColor = [Drawing.Color]::White
$grid.EnableHeadersVisualStyles = $false; $grid.ColumnHeadersHeight = 42; $grid.RowTemplate.Height = 40
$grid.ColumnHeadersDefaultCellStyle.BackColor = [Drawing.Color]::FromArgb(231, 238, 244)
$grid.ColumnHeadersDefaultCellStyle.ForeColor = $ink
$grid.ColumnHeadersDefaultCellStyle.Font = [Drawing.Font]::new('Microsoft JhengHei UI', 10, [Drawing.FontStyle]::Bold)
$grid.DefaultCellStyle.SelectionBackColor = [Drawing.Color]::FromArgb(220, 240, 242)
$grid.DefaultCellStyle.SelectionForeColor = $ink; $grid.GridColor = [Drawing.Color]::FromArgb(235, 240, 244)
$grid.AlternatingRowsDefaultCellStyle.BackColor = [Drawing.Color]::FromArgb(249, 251, 253)
$grid.CellBorderStyle = 'SingleHorizontal'; $grid.AccessibleName = 'IN／AT 譜面列表'
foreach ($spec in @(@('Song', '曲目', 30), @('Composer', '作者', 21), @('Difficulty', '難度', 8), @('Variant', '變體', 8), @('Notes', '音符', 9), @('State', '準備狀態', 24))) {
    $column = [Windows.Forms.DataGridViewTextBoxColumn]::new()
    $column.Name = $spec[0]; $column.HeaderText = $spec[1]; $column.FillWeight = [float]$spec[2]
    $column.SortMode = 'Automatic'; $column.MinimumWidth = if ($spec[0] -in @('Difficulty', 'Variant', 'Notes')) { 48 } else { 90 }
    [void]$grid.Columns.Add($column)
}
$left.Controls.Add($grid, 0, 1); $split.Panel1.Controls.Add($left)
$right = [Windows.Forms.TableLayoutPanel]::new()
$right.Dock = 'Fill'; $right.RowCount = 3; $right.ColumnCount = 1
$right.Padding = [Windows.Forms.Padding]::new(8, 0, 0, 0)
[void]$right.RowStyles.Add([Windows.Forms.RowStyle]::new([Windows.Forms.SizeType]::AutoSize))
[void]$right.RowStyles.Add([Windows.Forms.RowStyle]::new([Windows.Forms.SizeType]::Percent, 100))
[void]$right.RowStyles.Add([Windows.Forms.RowStyle]::new([Windows.Forms.SizeType]::AutoSize))
$detailHeading = [Windows.Forms.Label]::new()
$detailHeading.Text = '譜面詳情與來源'; $detailHeading.AutoSize = $true
$detailHeading.Font = [Drawing.Font]::new('Microsoft JhengHei UI', 11, [Drawing.FontStyle]::Bold)
$detailHeading.Margin = [Windows.Forms.Padding]::new(0, 0, 0, 10)
$details = [Windows.Forms.RichTextBox]::new()
$details.Dock = 'Fill'; $details.ReadOnly = $true; $details.BackColor = [Drawing.Color]::White
$details.BorderStyle = 'None'; $details.DetectUrls = $false; $details.WordWrap = $true
$details.ScrollBars = 'Vertical'; $details.AccessibleName = '選取譜面的來源、雜湊與準備說明'
$details.Text = Format-CatalogDetails $null
$actions = [Windows.Forms.FlowLayoutPanel]::new()
$actions.AutoSize = $true; $actions.Dock = 'Fill'; $actions.WrapContents = $true
$actions.Padding = [Windows.Forms.Padding]::new(0, 10, 0, 0)
$openButton = New-CatalogButton '核對並開啟原譜' $true
$folderButton = New-CatalogButton '所在目錄'
$audioButton = New-CatalogButton '核對並開啟音源'
$sourceButton = New-CatalogButton '來源網頁'
$copyButton = New-CatalogButton '複製識別'
foreach ($button in @($openButton, $audioButton, $folderButton, $sourceButton, $copyButton)) { $button.Enabled = $false; $actions.Controls.Add($button) }
$right.Controls.Add($detailHeading, 0, 0); $right.Controls.Add($details, 0, 1); $right.Controls.Add($actions, 0, 2)
$split.Panel2.Controls.Add($right); $layout.Controls.Add($split, 0, 3)

$footer = [Windows.Forms.FlowLayoutPanel]::new()
$footer.AutoSize = $true; $footer.Dock = 'Fill'; $footer.WrapContents = $true
$footer.Margin = [Windows.Forms.Padding]::new(0, 14, 0, 0)
$linkLegacyButton = New-CatalogButton '連結本機面板設定…'
$launchLegacyButton = New-CatalogButton '開啟既有實測面板'
$launchLegacyButton.Enabled = $false
$legacyLabel = [Windows.Forms.Label]::new()
$legacyLabel.AutoSize = $true; $legacyLabel.ForeColor = $muted; $legacyLabel.Padding = [Windows.Forms.Padding]::new(3, 9, 0, 0)
$legacyLabel.Text = '未連結本機設定；曲庫瀏覽與原件核對均可使用。'
$footer.Controls.Add($linkLegacyButton); $footer.Controls.Add($launchLegacyButton); $footer.Controls.Add($legacyLabel)
$layout.Controls.Add($footer, 0, 4)
$status = [Windows.Forms.Label]::new()
$status.AutoSize = $true; $status.Dock = 'Fill'; $status.ForeColor = $muted
$status.Text = '正在載入索引…    Ctrl+F 搜尋  ·  F5 重新整理  ·  Esc 清除篩選'
$status.Margin = [Windows.Forms.Padding]::new(0, 4, 0, 0)
$layout.Controls.Add($status, 0, 5)
$tooltip = [Windows.Forms.ToolTip]::new()
$tooltip.SetToolTip($openButton, '先在背景核對 SHA256 及 LFS 原件，再以預設編輯器開啟。')
$tooltip.SetToolTip($launchLegacyButton, '開啟獨立面板；此曲庫選取項目不會直接送入實測。原面板仍核對版本、設定與釋放條件。')
$tooltip.SetToolTip($variantFilter, '保留來源的原生變體識別；不同變體不會互相覆蓋。')

function Set-CatalogUiStatus([string]$Text, [bool]$Failed = $false) {
    $status.Text = $Text
    $status.ForeColor = if ($Failed) { [Drawing.Color]::FromArgb(165, 50, 48) } else { $muted }
}
function Get-CatalogSelection {
    if ($grid.CurrentRow) { return $grid.CurrentRow.Tag }
    return $null
}
function Update-CatalogSelection {
    if ($script:CatalogUi.Updating) { return }
    $row = Get-CatalogSelection
    $details.Text = Format-CatalogDetails $row
    $openButton.Enabled = $null -ne $row -and -not $script:CatalogUi.Job
    $audioButton.Enabled = $null -ne $row -and [bool]$row.AudioFile -and -not $script:CatalogUi.Job
    $folderButton.Enabled = $null -ne $row
    $copyButton.Enabled = $null -ne $row
    $sourceButton.Enabled = $null -ne $row -and [bool]$row.SourceUrl
}
function Update-CatalogFilter {
    if (-not $script:CatalogUi.Model) { return }
    $selected = Get-CatalogSelection
    $selectedId = if ($selected) { $selected.Id } else { '' }
    $difficulty = if ($difficultyFilter.SelectedIndex -gt 0) { [string]$difficultyFilter.SelectedItem } else { '' }
    $variant = if ($variantFilter.SelectedIndex -gt 0) { $script:CatalogUi.Model.Variants[$variantFilter.SelectedIndex - 1] } else { '' }
    $state = $stateCodes[[Math]::Max(0, $stateFilter.SelectedIndex)]
    $visible = @(Select-CatalogRows $script:CatalogUi.Model.Rows $search.Text $difficulty $variant $state)
    $script:CatalogUi.Visible = $visible; $script:CatalogUi.Updating = $true
    $grid.SuspendLayout()
    try {
        $grid.Rows.Clear(); $restore = -1
        foreach ($row in $visible) {
            $i = $grid.Rows.Add([object[]]@($row.Title, $row.Composer, $row.Difficulty, '.' + $row.Variant, $row.Notes, $row.StateText))
            $grid.Rows[$i].Tag = $row
            $grid.Rows[$i].Cells['Song'].ToolTipText = $row.TrackId
            $grid.Rows[$i].Cells['State'].ToolTipText = $row.Blockers -join [Environment]::NewLine
            if ($row.Id -ceq $selectedId) { $restore = $i }
        }
        if ($visible.Count) {
            $i = if ($restore -ge 0) { $restore } else { 0 }
            $grid.CurrentCell = $grid.Rows[$i].Cells['Song']
            $grid.Rows[$i].Selected = $true
        }
    } finally { $grid.ResumeLayout(); $script:CatalogUi.Updating = $false }
    $songCount = @($visible.SongId | Sort-Object -Unique).Count
    $resultsLabel.Text = if ($visible.Count) { "顯示 $($visible.Count) / $($script:CatalogUi.Model.ChartCount) 份譜面 · $songCount 首歌曲" } else { '沒有符合條件的譜面；可清除篩選後再搜尋。' }
    Update-CatalogSelection
}
function Start-CatalogJob([string]$Operation, [object[]]$Arguments, [string]$After = '') {
    if ($script:CatalogUi.Job) { Set-CatalogUiStatus '上一項核對尚未完成，請稍候。'; return }
    try { $script:CatalogUi.Job = New-CatalogReadOperation $script:CatalogUi.Script $Operation $Arguments $After }
    catch { Set-CatalogUiStatus $_.Exception.Message $true; return }
    $reloadButton.Enabled = $false; $openButton.Enabled = $false; $audioButton.Enabled = $false
    $linkLegacyButton.Enabled = $false; $launchLegacyButton.Enabled = $false
    Set-CatalogUiStatus $(switch ($Operation) { 'load' { '正在背景讀取曲庫索引…' }; 'verify' { '正在背景核對原件 SHA256…' }; 'legacy' { '正在背景核對本機面板設定…' } })
}
function Open-CatalogDocument([string]$Path) {
    $info = [Diagnostics.ProcessStartInfo]::new($Path)
    $info.UseShellExecute = $true
    [void][Diagnostics.Process]::Start($info)
}
function Start-CatalogLegacyPanel($Launch) {
    $pwsh = Join-Path $PSHOME 'pwsh.exe'
    $info = [Diagnostics.ProcessStartInfo]::new($pwsh)
    $info.UseShellExecute = $false; $info.CreateNoWindow = $true
    foreach ($argument in @('-NoProfile', '-STA', '-WindowStyle', 'Hidden', '-File', $Launch.Script, '-Configuration', $Launch.Configuration)) { $info.ArgumentList.Add($argument) }
    [void][Diagnostics.Process]::Start($info)
    Set-CatalogUiStatus '已開啟獨立實測面板；請在該面板選曲並依 READY 操作。曲庫不會自動傳送或播放選取譜面。'
}
function Clear-CatalogFilters {
    $search.Clear(); $difficultyFilter.SelectedIndex = 0; $variantFilter.SelectedIndex = 0; $stateFilter.SelectedIndex = 0
    Update-CatalogFilter; $search.Focus() | Out-Null
}
$debounce = [Windows.Forms.Timer]::new(); $debounce.Interval = 160
$debounce.Add_Tick({ $debounce.Stop(); Update-CatalogFilter })
$search.Add_TextChanged({ $debounce.Stop(); $debounce.Start() })
foreach ($combo in @($difficultyFilter, $variantFilter, $stateFilter)) { $combo.Add_SelectedIndexChanged({ if (-not $script:CatalogUi.Updating) { Update-CatalogFilter } }) }
$resetButton.Add_Click({ Clear-CatalogFilters })
$reloadButton.Add_Click({ Start-CatalogJob 'load' @($script:CatalogUi.Library) })
$grid.Add_SelectionChanged({ Update-CatalogSelection })
$openButton.Add_Click({ $row = Get-CatalogSelection; if ($row) { Start-CatalogJob 'verify' @($row.File, $row.Sha256) 'open' } })
$audioButton.Add_Click({ $row = Get-CatalogSelection; if ($row -and $row.AudioFile) { Start-CatalogJob 'verify' @($row.AudioFile, $row.AudioSha256) 'open' } })
$grid.Add_CellDoubleClick({ param($sender, $eventArgs) if ($eventArgs.RowIndex -ge 0 -and $openButton.Enabled) { $openButton.PerformClick() } })
$grid.Add_KeyDown({
    param($sender, $eventArgs)
    if ($eventArgs.KeyCode -eq [Windows.Forms.Keys]::Enter -and $openButton.Enabled) {
        $openButton.PerformClick(); $eventArgs.SuppressKeyPress = $true
    } elseif ($eventArgs.Control -and $eventArgs.KeyCode -eq [Windows.Forms.Keys]::C -and $copyButton.Enabled) {
        $copyButton.PerformClick(); $eventArgs.SuppressKeyPress = $true
    }
})
$folderButton.Add_Click({
    try {
        $row = Get-CatalogSelection
        if ($row) {
            $folder = Split-Path -Parent $row.File
            if (-not [IO.Directory]::Exists($folder)) { $folder = $script:CatalogUi.Model.Root }
            Open-CatalogDocument $folder
        }
    } catch { Set-CatalogUiStatus $_.Exception.Message $true }
})
$copyButton.Add_Click({
    try { $row = Get-CatalogSelection; if ($row) { [Windows.Forms.Clipboard]::SetText($row.Id); Set-CatalogUiStatus '已複製此版本、曲目與難度的完整識別。' } }
    catch { Set-CatalogUiStatus $_.Exception.Message $true }
})
$sourceButton.Add_Click({
    try {
        $row = Get-CatalogSelection; $uri = $null
        if (-not $row -or -not [Uri]::TryCreate($row.SourceUrl, [UriKind]::Absolute, [ref]$uri) -or
            $uri.Scheme -cne 'https' -or $uri.Host -notin @('github.com', 'raw.githubusercontent.com')) { throw '來源網址不是允許的 HTTPS GitHub 位址。' }
        Open-CatalogDocument $uri.AbsoluteUri
    } catch { Set-CatalogUiStatus $_.Exception.Message $true }
})
$linkLegacyButton.Add_Click({
    $dialog = [Windows.Forms.OpenFileDialog]::new()
    try {
        $dialog.Title = '選取本機實測面板的 launch-config.json'
        $dialog.Filter = '面板設定 (*.json)|*.json'; $dialog.CheckFileExists = $true
        if ($dialog.ShowDialog($form) -eq [Windows.Forms.DialogResult]::OK) {
            $script:CatalogUi.LegacyPath = $dialog.FileName
            $script:CatalogUi.Legacy = $null
            Start-CatalogJob 'legacy' @($dialog.FileName)
        }
    } finally { $dialog.Dispose() }
})
$launchLegacyButton.Add_Click({ if ($script:CatalogUi.Legacy) { Start-CatalogJob 'legacy' @($script:CatalogUi.Legacy.Configuration) 'launch' } })
$form.Add_KeyDown({
    param($sender, $eventArgs)
    if ($eventArgs.Control -and $eventArgs.KeyCode -eq [Windows.Forms.Keys]::F) { $search.Focus() | Out-Null; $search.SelectAll(); $eventArgs.SuppressKeyPress = $true }
    elseif ($eventArgs.KeyCode -eq [Windows.Forms.Keys]::F5) { if ($reloadButton.Enabled) { $reloadButton.PerformClick() }; $eventArgs.SuppressKeyPress = $true }
    elseif ($eventArgs.KeyCode -eq [Windows.Forms.Keys]::Escape) { Clear-CatalogFilters; $eventArgs.SuppressKeyPress = $true }
})
$poll = [Windows.Forms.Timer]::new(); $poll.Interval = 80
$poll.Add_Tick({
    $job = $script:CatalogUi.Job
    if (-not $job -or -not $job.Async.IsCompleted) { return }
    try {
        $values = $job.Shell.EndInvoke($job.Async)
        if ($job.Shell.HadErrors) { throw [string]($job.Shell.Streams.Error | Select-Object -First 1) }
        if ($values.Count -ne 1) { throw '背景核對沒有傳回唯一結果。' }
        $value = $values[0]
        if ($script:CatalogUi.Closing) { return }
        switch ($job.Operation) {
            'load' {
                $script:CatalogUi.Model = $value
                $statLabels.songs.Text = "$($value.SongCount) 首歌曲"
                $statLabels.charts.Text = "$($value.ChartCount) 份譜面"
                $statLabels.IN.Text = "$($value.IN) IN"; $statLabels.AT.Text = "$($value.AT) AT"
                $versions = @($value.Rows.Version | Sort-Object -Unique) -join '、'
                $subheading.Text = "公共來源 $versions · 原件身份已入索引；與本機遊戲版本仍須逐項核對。"
                $script:CatalogUi.Updating = $true
                $variantFilter.Items.Clear(); [void]$variantFilter.Items.Add('全部變體')
                foreach ($variant in $value.Variants) { [void]$variantFilter.Items.Add("變體 .$variant") }
                $variantFilter.SelectedIndex = 0; $script:CatalogUi.Updating = $false
                Update-CatalogFilter
                Set-CatalogUiStatus '曲庫已載入。可搜尋、篩選及核對原件；下載與入庫不代表已取得本機實測資格。'
            }
            'verify' {
                if ($job.After -eq 'open') { Open-CatalogDocument $value.File }
                Set-CatalogUiStatus "原件 SHA256 相符，已開啟：$([IO.Path]::GetFileName($value.File))"
            }
            'legacy' {
                $script:CatalogUi.Legacy = $value
                $legacyLabel.Text = '已連結既有面板；該面板會獨立核對版本、設定與目前開機證據。'
                if ($job.After -eq 'launch') { Start-CatalogLegacyPanel $value } else { Set-CatalogUiStatus '本機面板入口已核對；按「開啟既有實測面板」才會啟動。' }
            }
        }
    } catch {
        if (-not $script:CatalogUi.Closing) {
            Set-CatalogUiStatus $_.Exception.Message $true
            if ($job.Operation -eq 'legacy') { $script:CatalogUi.Legacy = $null; $legacyLabel.Text = '本機設定未通過核對；仍可瀏覽曲庫與開啟原件。' }
            if ($job.Operation -eq 'load' -and -not $script:CatalogUi.Model) { $resultsLabel.Text = '曲庫尚未載入'; $details.Text = $_.Exception.Message }
        }
    } finally {
        $job.Shell.Dispose(); $script:CatalogUi.Job = $null
        if ($script:CatalogUi.Closing) { $form.Close() }
        else {
            $reloadButton.Enabled = $true; $linkLegacyButton.Enabled = $true
            $launchLegacyButton.Enabled = $null -ne $script:CatalogUi.Legacy
            Update-CatalogSelection
            if ($script:CatalogUi.PendingLegacy) {
                $next = $script:CatalogUi.PendingLegacy; $script:CatalogUi.PendingLegacy = $null
                Start-CatalogJob 'legacy' @($next)
            }
        }
    }
})
$form.Add_Shown({ $poll.Start(); Start-CatalogJob 'load' @($script:CatalogUi.Library); $search.Focus() | Out-Null })
$form.Add_FormClosing({
    param($sender, $eventArgs)
    if ($script:CatalogUi.Job) {
        $script:CatalogUi.Closing = $true; $eventArgs.Cancel = $true
        $form.Enabled = $false; Set-CatalogUiStatus '正在完成唯讀檔案核對後關閉…'
    }
})
function Save-CatalogPreview([string]$Directory) {
    $output = Resolve-CatalogInputPath $Directory
    if (Test-Path -LiteralPath $output) { throw '預覽輸出必須是新的目錄。' }
    $form.ShowInTaskbar = $false
    $form.StartPosition = 'Manual'
    $form.Location = [Drawing.Point]::new(-32000, -32000)
    $form.Show()
    $deadline = [DateTime]::UtcNow.AddSeconds(40)
    while (-not $script:CatalogUi.Model -or $script:CatalogUi.Job) {
        [Windows.Forms.Application]::DoEvents()
        if ([DateTime]::UtcNow -ge $deadline) { throw '預覽等待曲庫載入逾時（40 秒）。' }
        if (-not $script:CatalogUi.Model -and -not $script:CatalogUi.Job) { throw "預覽無法載入曲庫：$($status.Text)" }
        [Threading.Thread]::Sleep(20)
    }
    [void][IO.Directory]::CreateDirectory($output)
    $artifacts = [Collections.Generic.List[object]]::new()
    foreach ($size in @(@('catalog-wide.png', 1440, 900, 850), @('catalog-compact.png', 1080, 720, 620))) {
        $form.ClientSize = [Drawing.Size]::new([int]$size[1], [int]$size[2])
        $split.SplitterDistance = [int]$size[3]
        $form.PerformLayout()
        [Windows.Forms.Application]::DoEvents()
        Set-CatalogUiStatus '離線畫面檢查 · 未啟動模擬器、原生實測或任何觸控。'
        $bitmap = [Drawing.Bitmap]::new($form.Width, $form.Height)
        try {
            $form.DrawToBitmap($bitmap, [Drawing.Rectangle]::new(0, 0, $form.Width, $form.Height))
            $path = Join-Path $output ([string]$size[0])
            $bitmap.Save($path, [Drawing.Imaging.ImageFormat]::Png)
            $artifacts.Add([pscustomobject]@{
                file = [string]$size[0]; width = $bitmap.Width; height = $bitmap.Height
                sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
            })
        } finally { $bitmap.Dispose() }
    }
    $report = [ordered]@{
        schema = 'pas.catalog-panel-preview.v1'; status = 'rendered'
        library = $script:CatalogUi.Library
        song_count = $script:CatalogUi.Model.SongCount; chart_count = $script:CatalogUi.Model.ChartCount
        IN = $script:CatalogUi.Model.IN; AT = $script:CatalogUi.Model.AT
        device_access = $false; native_processes_started = 0; gameplay_qualified = $false
        winforms_windows_created = 1; artifacts = $artifacts.ToArray()
        scope = 'Offline WinForms DrawToBitmap at two window sizes; no live game or touch behavior tested.'
    }
    $report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'preview-manifest.json') -Encoding utf8
    $report | ConvertTo-Json -Depth 5
    $form.Close()
}
try {
    if ($PreviewDirectory) { Save-CatalogPreview $PreviewDirectory }
    else { [void]$form.ShowDialog() }
}
finally { $poll.Stop(); $poll.Dispose(); $debounce.Stop(); $debounce.Dispose(); $tooltip.Dispose(); $form.Dispose() }
