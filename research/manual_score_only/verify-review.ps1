param([switch]$LocalArtifacts)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$manifest=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'review/current-approach.json') -Raw|ConvertFrom-Json
function Assert-ReviewHash([string]$RelativePath,[string]$Expected) {
    $path=[IO.Path]::GetFullPath((Join-Path $repo $RelativePath))
    if(-not $path.StartsWith($repo+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'review path must be inside workspace'}
    if($Expected -cnotmatch '^[0-9a-f]{64}$' -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $Expected){throw ('review SHA differs: '+$RelativePath)}
}
foreach($pin in @($manifest.source_pins)+@($manifest.asset_pins)+@($manifest.historical_result)){Assert-ReviewHash $pin.path $pin.sha256}
foreach($entry in $manifest.score_evidence) {
    Assert-ReviewHash $entry.path $entry.sha256
    $rows=@(Get-Content -LiteralPath (Join-Path $repo $entry.path) -Raw|ConvertFrom-Json)
    if($rows.Count -ne $entry.rows){throw 'score denominator differs'}
    foreach($row in $rows) {
        if((@($row.PSObject.Properties.Name|Sort-Object) -join ',') -cne 'in_level,score,song,status'){throw 'unexpected gameplay field'}
        if(-not $row.song -or $row.score -lt 0 -or $row.score -gt 1000000){throw 'invalid score row'}
    }
    if(@($rows.song|Sort-Object -Unique).Count -ne $rows.Count){throw 'duplicate song in score set'}
}
foreach($variant in @($manifest.baseline,$manifest.retained_latest)) {
    $build=Get-Content -LiteralPath (Join-Path $repo $variant.build_evidence) -Raw|ConvertFrom-Json
    if($build.exe_sha256 -cne $variant.exe_sha256 -or $build.base_binary_source -cne $manifest.preserved_original.source_commit){throw 'build identity differs'}
    foreach($pin in $build.modified_source_sha256.PSObject.Properties) {
        $source=if($pin.Name -eq 'src/manual_session.cpp'){$variant.source}else{$pin.Name}
        Assert-ReviewHash $source $pin.Value
    }
}
foreach($name in @('build.ps1','standby.ps1','validate-lazy.ps1','verify-review.ps1')) {
    $scriptTokens=$null;$scriptErrors=$null
    $ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $name),[ref]$scriptTokens,[ref]$scriptErrors)
    if($scriptErrors.Count){throw ($scriptErrors|Out-String)}
    if($name -in @('build.ps1','standby.ps1')) {
        $variantDefault=@($ast.ParamBlock.Parameters|Where-Object {$_.Name.VariablePath.UserPath -eq 'Variant'})
        if($variantDefault.Count -ne 1 -or $variantDefault[0].DefaultValue.Value -cne $manifest.selected_variant){throw 'default variant differs'}
    }
    if($name -eq 'standby.ps1') {
        foreach($expected in @{LeadMs=$manifest.selected_lead_ms;Rounds=$manifest.default_round_limit}.GetEnumerator()) {
            $parameter=@($ast.ParamBlock.Parameters|Where-Object {$_.Name.VariablePath.UserPath -eq $expected.Key})
            if($parameter.Count -ne 1 -or $parameter[0].DefaultValue.Value -ne $expected.Value){throw 'standby default differs'}
        }
    }
}
if($LocalArtifacts) {
    foreach($variant in @($manifest.baseline,$manifest.retained_latest,$manifest.preserved_original)){Assert-ReviewHash $variant.local_exe $variant.exe_sha256}
}
Write-Output 'PASS: exact baseline/lazy sources, original build identities, 20/1/3/3 score rows, four-field schema and baseline/35ms/20 defaults.'
Write-Output $(if($LocalArtifacts){'PASS: all three retained local executable hashes.'}else{'Local executables not checked; pass -LocalArtifacts on the retained Windows workspace.'})
Write-Output 'Read-only review verification; no native runtime, gameplay or cold readiness tests launched.'
