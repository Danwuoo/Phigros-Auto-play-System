[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Plan,
    [switch]$Run,
    [switch]$CheckOnly,
    [string]$BuildArchive,
    [string]$KernelProfile,
    [string]$CapacityEvidence,
    [string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
if ($Run -and $CheckOnly) { throw 'Choose Run or CheckOnly.' }
$Plan = (Resolve-Path -LiteralPath $Plan).Path
$sweep = Get-Content -LiteralPath $Plan -Raw -Encoding utf8 | ConvertFrom-Json
if ($sweep.schema -ne 'pas.chapter-IN-sweep-plan.v1' -or $sweep.difficulty -ne 'IN' -or
    $sweep.jobs.Count -ne $sweep.song_count -or $sweep.round_limit -ne $sweep.song_count -or
    $sweep.song_count -lt 1 -or $sweep.song_count -gt 64) { throw 'Invalid finite IN sweep plan.' }
function Assert-Reference($reference) {
    if ($null -eq $reference) { return }
    if ((Get-FileHash -LiteralPath $reference.file -Algorithm SHA256).Hash -ne $reference.sha256) {
        throw "Sweep artifact changed: $($reference.file)"
    }
}
Assert-Reference $sweep.chapter_index
Assert-Reference $sweep.readiness_summary
$seen = @{}
$ordinal = 0
foreach ($job in $sweep.jobs) {
    $ordinal++
    if ($job.order -ne $ordinal -or $job.rounds -ne 1 -or $job.difficulty -ne 'IN' -or $seen.ContainsKey($job.track_id)) {
        throw 'Duplicate, reordered or non-single-round job.'
    }
    $seen[$job.track_id] = $true
    Assert-Reference $job.chart
    Assert-Reference $job.profile
    Assert-Reference $job.state_calibration
    Assert-Reference $job.hud_reference
    $state = if ($job.ready_for_manual_preflight) { 'MANUAL PREFLIGHT CANDIDATE' } else { 'BLOCKED' }
    Write-Host ("{0,2}. {1} IN - {2}" -f $job.order, $job.track_id, $state)
    foreach ($blocker in $job.blockers) { Write-Host "    $($blocker.category): $($blocker.reason)" }
    foreach ($finding in $job.qualification_warnings) { Write-Host "    Qualification: $($finding.category): $($finding.reason)" }
}
if (-not $Run -and -not $CheckOnly) {
    Write-Host "Offline queue verification complete: $($sweep.song_count) jobs, $($sweep.blocked_count) blocked. No device discovery or input."
    return
}
if (@($sweep.jobs | Where-Object { -not $_.ready_for_manual_preflight -or $_.blockers.Count -gt 0 }).Count -gt 0) {
    throw 'Full sweep is not runnable while any job is blocked. No jobs were started.'
}
if (-not $BuildArchive -or -not $KernelProfile -or ($Run -and (-not $CapacityEvidence -or -not $OutputDirectory))) {
    throw 'Native preflight requires build archive/kernel profile; Run also requires capacity evidence/new output directory.'
}
# Validate every pinned chart/profile/model/policy with the actual archived
# executable before any device operation or interactive round is started.
foreach ($job in $sweep.jobs) {
    $arguments = @{
        BuildArchive=$BuildArchive; Chart=$job.chart.file; SongProfile=$job.profile.file
        StateCalibration=$job.state_calibration.file; HudReference=$job.hud_reference.file
        KernelProfile=$KernelProfile; CheckOnly=$true
    }
    if ($CapacityEvidence) { $arguments.CapacityEvidence=$CapacityEvidence }
    & (Join-Path $PSScriptRoot 'run-adaptive-song.ps1') @arguments
    if ($LASTEXITCODE -ne 0) { throw "Native preflight failed for $($job.track_id). No jobs were started." }
}
if ($CheckOnly) {
    Write-Host 'All native offline preflights passed. No device discovery or input.'
    return
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Sweep output directory must be new.' }
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$ledger = [ordered]@{
    schema='pas.chapter-IN-sweep-ledger.v1'; plan=$Plan
    plan_sha256=(Get-FileHash -LiteralPath $Plan -Algorithm SHA256).Hash.ToLowerInvariant()
    build_archive=[IO.Path]::GetFullPath($BuildArchive)
    status='running'; total_rounds=$sweep.song_count
    started_utc=[DateTime]::UtcNow.ToString('o'); automatic_retry=$false; rounds=@()
}
$ledgerPath = Join-Path $OutputDirectory 'ledger.json'
function Save-Ledger {
    $ledger | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $ledgerPath -Encoding utf8
}
Save-Ledger
foreach ($job in $sweep.jobs) {
    $round = Join-Path $OutputDirectory ('round-{0:D2}' -f [int]$job.order)
    $entry = [ordered]@{
        order=$job.order; track_id=$job.track_id; exit_code=$null; directory=$round
        status='waiting_for_operator'; judgment='unreviewed'; failure=$null
        runtime_failure=$null; kernel_final_zero=$null; manifest_file=$null
    }
    $ledger.rounds += $entry
    Save-Ledger
    try {
        Write-Host "Round $($job.order)/$($sweep.song_count): select $($job.track_id) IN manually. Leave it on selection/result."
        Read-Host 'Press Enter to launch this round; press game Play only after READY' | Out-Null
        $entry.status='running'
        Save-Ledger
        & (Join-Path $PSScriptRoot 'run-adaptive-song.ps1') -BuildArchive $BuildArchive -Chart $job.chart.file `
            -SongProfile $job.profile.file -StateCalibration $job.state_calibration.file -HudReference $job.hud_reference.file `
            -KernelProfile $KernelProfile -CapacityEvidence $CapacityEvidence -OutputDirectory $round
        $entry.exit_code=$LASTEXITCODE
        $entry.status=if ($LASTEXITCODE -eq 0) { 'sent_result_unreviewed' } else { 'failed_or_stopped' }
        Save-Ledger
        if ($entry.exit_code -ne 0) {
            $manifestPath = Join-Path $round 'manifest.json'
            if (Test-Path -LiteralPath $manifestPath -PathType Leaf) {
                if ((Get-Item -LiteralPath $manifestPath).Length -le 1048576) {
                    try {
                        $roundManifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding utf8 | ConvertFrom-Json
                        $entry.runtime_failure=$roundManifest.failure
                        $entry.kernel_final_zero=$roundManifest.kernel_final_zero
                        $entry.manifest_file=$manifestPath
                    } catch { $entry.runtime_failure='round_manifest_unreadable' }
                }
            }
            if ($entry.runtime_failure) { throw "$($entry.runtime_failure) (exit $($entry.exit_code)); see $manifestPath" }
            throw "Round exited $($entry.exit_code); no native failure manifest was available."
        }
    } catch {
        $entry.status='failed_or_stopped'; $entry.failure=$_.Exception.Message
        $ledger.status='stopped'; $ledger.stopped_utc=[DateTime]::UtcNow.ToString('o')
        Save-Ledger
        throw "Sweep stopped at $($job.track_id): $($entry.failure) No automatic retry."
    }
}
$ledger.status='completed'; $ledger.completed_utc=[DateTime]::UtcNow.ToString('o')
Save-Ledger
Write-Host "Finite sweep complete: $($sweep.song_count) rounds. Results remain unreviewed. Ledger: $ledgerPath"
