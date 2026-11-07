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
$Repository=[IO.Path]::GetFullPath($Repository)
if(-not $ManualConfiguration){$ManualConfiguration=Join-Path $Repository 'local\manual-launch.json'}
$ManualConfiguration=[IO.Path]::GetFullPath($ManualConfiguration)

# Stable bootstrap only. Application code is installed as one versioned bundle;
# editing the development tools cannot invalidate the installed desktop GUI.
$runtime=Join-Path $Repository 'tools\desktop-start-runtime.ps1'
if(Test-Path -LiteralPath $ManualConfiguration -PathType Leaf){
    if((Get-Item -LiteralPath $ManualConfiguration).Length -gt 1048576){throw '人工模式設定超出大小限制。'}
    $configuration=[IO.File]::ReadAllText($ManualConfiguration,[Text.Encoding]::UTF8)|ConvertFrom-Json -Depth 100
    if($configuration.PSObject.Properties['frontend_release']){
        $reference=$configuration.frontend_release
        if($reference.file -isnot [string] -or -not [IO.Path]::IsPathFullyQualified($reference.file) -or
           $reference.sha256 -cnotmatch '^[a-f0-9]{64}$'){throw '桌面介面版本參照不完整。'}
        $manifestPath=[IO.Path]::GetFullPath($reference.file)
        $releaseRoot=Split-Path -Parent $manifestPath
        $releasesRoot=[IO.Path]::GetFullPath((Join-Path $Repository 'local\desktop-app\releases'))
        if(-not (Split-Path -Parent $releaseRoot).Equals($releasesRoot,[StringComparison]::OrdinalIgnoreCase) -or
           -not (Split-Path -Leaf $manifestPath).Equals('manifest.json',[StringComparison]::Ordinal) -or
           -not(Test-Path -LiteralPath $manifestPath -PathType Leaf)){
            throw '已安裝的桌面介面版本遺失或路徑不正確。'
        }
        foreach($directory in @($releasesRoot,$releaseRoot,(Join-Path $releaseRoot 'tools'))){
            if(-not(Test-Path -LiteralPath $directory -PathType Container) -or
               ((Get-Item -LiteralPath $directory).Attributes -band [IO.FileAttributes]::ReparsePoint)){
                throw '桌面介面版本的目錄不完整。'
            }
        }
        $manifestFile=Get-Item -LiteralPath $manifestPath
        if($manifestFile.Length -gt 65536 -or ($manifestFile.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
           (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash -ine $reference.sha256){
            throw '桌面介面版本清單不完整；請重新安裝桌面介面版本。'
        }
        $manifest=[IO.File]::ReadAllText($manifestPath,[Text.Encoding]::UTF8)|ConvertFrom-Json -Depth 16
        $expected=@('tools/desktop-start-runtime.ps1','tools/sweep-panel.ps1','tools/catalog-panel-core.ps1',
                    'tools/song-library-panel.ps1','tools/start-song-library.ps1')
        if($manifest.schema -cne 'pas.desktop-frontend-release.v1' -or
           $manifest.runtime -cne $expected[0] -or @($manifest.artifacts).Count -ne $expected.Count){
            throw '桌面介面版本格式不正確。'
        }
        $members=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
        [long]$total=0
        foreach($artifact in $manifest.artifacts){
            if($artifact.file -isnot [string] -or $artifact.file -cnotin $expected -or
               $artifact.sha256 -cnotmatch '^[a-f0-9]{64}$' -or
               ($artifact.bytes -isnot [int] -and $artifact.bytes -isnot [long]) -or
               $artifact.bytes -lt 1 -or $artifact.bytes -gt 524288){throw '桌面介面版本包含無效檔案。'}
            $target=[IO.Path]::GetFullPath((Join-Path $releaseRoot $artifact.file))
            if($members.ContainsKey($target) -or -not(Test-Path -LiteralPath $target -PathType Leaf)){
                throw '桌面介面版本檔案遺失或重複。'
            }
            $file=Get-Item -LiteralPath $target
            if($file.Length -ne $artifact.bytes -or ($file.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
               (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ine $artifact.sha256){
                throw '已安裝的桌面介面檔案不完整；請重新安裝桌面介面版本。'
            }
            $total+=$file.Length
            $members.Add($target,$artifact.sha256)
        }
        if($total -gt 2097152 -or @($configuration.scripts).Count -ne $expected.Count){throw '桌面介面版本相依檔案不完整。'}
        $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        foreach($script in $configuration.scripts){
            if($script.file -isnot [string] -or -not [IO.Path]::IsPathFullyQualified($script.file)){
                throw '桌面設定的介面腳本路徑無效。'
            }
            $target=[IO.Path]::GetFullPath($script.file)
            if(-not $members.ContainsKey($target) -or $members[$target] -cne $script.sha256 -or -not $seen.Add($target)){
                throw '桌面設定與已安裝的介面版本不同。'
            }
        }
        $gui=[IO.Path]::GetFullPath((Join-Path $releaseRoot 'tools/sweep-panel.ps1'))
        if($configuration.gui_script.file -isnot [string] -or
           -not [IO.Path]::IsPathFullyQualified($configuration.gui_script.file) -or
           -not [IO.Path]::GetFullPath($configuration.gui_script.file).Equals($gui,[StringComparison]::OrdinalIgnoreCase) -or
           $configuration.gui_script.sha256 -cne $members[$gui]){throw '曲庫面板與已安裝的介面版本不同。'}
        $runtime=Join-Path $releaseRoot $manifest.runtime
    }
}
if(-not(Test-Path -LiteralPath $runtime -PathType Leaf)){throw '桌面啟動元件遺失；請重新安裝桌面介面版本。'}
$parameters=@{Repository=$Repository;ManualConfiguration=$ManualConfiguration;BootTimeoutSeconds=$BootTimeoutSeconds
    PlanOnly=$PlanOnly;LoadCoreOnly=$LoadCoreOnly;InProcessPanel=$InProcessPanel}
if($PSBoundParameters.ContainsKey('EmulatorAudioBackend')){
    if(-not (Get-Command -Name $runtime).Parameters.ContainsKey('EmulatorAudioBackend')){throw '已安裝介面尚不支援音訊選項；請先更新桌面介面版本。'}
    $parameters['EmulatorAudioBackend']=$EmulatorAudioBackend
}
if($LoadCoreOnly){. $runtime @parameters; return}
& $runtime @parameters
