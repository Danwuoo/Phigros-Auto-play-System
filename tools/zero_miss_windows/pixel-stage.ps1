param([Parameter(Mandatory)][string]$StageName,
      [Parameter(Mandatory)][string]$BuildStage,
      [Parameter(Mandatory)][string]$BuildRoot,
      [Parameter(Mandatory)][string]$Selection,
      [Parameter(Mandatory)][string]$Report,
      [Parameter(Mandatory)][string]$GateReceipt,
      [ValidateRange(30,1800)][int]$TotalSeconds=300)
$ErrorActionPreference='Stop'
$taskRepo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if($StageName -notmatch '^[a-z0-9-]{1,90}$' -or $BuildStage -notmatch '^[a-z0-9-]{1,90}$'){throw 'stage-name'}
$control=Join-Path $taskRepo ('out/windows-handoff/'+$StageName+'-pixel-controls')
if(Test-Path -LiteralPath $control){throw 'fresh-pixel-controls'}
[IO.Directory]::CreateDirectory($control)|Out-Null
function WriteNewJson($path,$value,[int]$cap=262144) {
  $bytes=[Text.Encoding]::UTF8.GetBytes(($value|ConvertTo-Json -Depth 35 -Compress)+"`n")
  if($bytes.Length -gt $cap){throw 'pixel-metadata-cap'}
  $handle=[IO.FileStream]::new($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
  try{$handle.Write($bytes);$handle.Flush($true)}finally{$handle.Dispose()}
}
function CheckSha($path,$sha,[long]$bytes=-1) {
  $file=Get-Item -LiteralPath $path
  if($file.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'pixel-reparse'}
  if(($bytes -ge 0 -and $file.Length -ne $bytes) -or
     (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $sha){throw 'external-input-sha'}
}
$pre=$false;$post=$false;$nativeOk=$false;$failure=$null;$count=0;$modules=@()
try {
  $Selection=[IO.Path]::GetFullPath($Selection)
  if((Get-Item -LiteralPath $Selection).Length -gt 2097152){throw 'selection-byte-cap'}
  $selected=Get-Content -LiteralPath $Selection -Raw|ConvertFrom-Json
  if($selected.schema -cne 'pas.windows-offline-pixels-selection.v1' -or $selected.human_gold -ne 0 -or
     $selected.device_read -ne $false -or $selected.future_frames_used_for_decisions -ne $false -or
     $selected.frames.Count -ne $selected.selected_count -or $selected.frames.Count -lt 1 -or $selected.frames.Count -gt 256){throw 'selection-contract'}
  $root=[IO.Path]::GetFullPath($selected.record_root)
  $leaves=@($selected.frames|ForEach-Object {
    $path=[IO.Path]::GetFullPath($_.path)
    if(-not $path.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'pixel-containment'}
    $parent=[IO.DirectoryInfo]::new([IO.Path]::GetDirectoryName($path))
    while($parent -and $parent.FullName.Length -ge $root.Length){
      if($parent.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'pixel-parent-reparse'}
      $parent=$parent.Parent
    }
    @{path=$path;bytes=[long]$_.bytes;sha256=$_.sha256}
  })
  $freezePath=Join-Path $taskRepo ('out/windows-handoff/'+$BuildStage+'/freeze.json')
  $buildReceipt=Join-Path $taskRepo ('out/windows-handoff/'+$BuildStage+'/'+$BuildStage+'-verification.json')
  $verification=Get-Content -LiteralPath $buildReceipt -Raw|ConvertFrom-Json
  $buildResult=Join-Path $taskRepo ('out/windows-handoff/'+$BuildStage+'/'+$BuildStage+'-result.json')
  $facts=(Get-Content -LiteralPath $buildResult -Raw|ConvertFrom-Json).facts
  if($verification.pending -or $verification.verification_exit -ne 0 -or $verification.runner_exit -ne 0 -or
     $verification.native_exit -ne 0 -or $facts.active_final -ne 0 -or -not $facts.identity_trusted -or
     -not $facts.streams_completed -or -not $facts.held_all_signaled){throw 'build-receipt'}
  foreach($entry in $verification.entries){CheckSha $entry.path $entry.sha256 $entry.bytes}
  $buildInputs=(Get-Content -LiteralPath $freezePath -Raw|ConvertFrom-Json).files
  $leafPath=Join-Path $control 'pixel-leaves.json'
  WriteNewJson $leafPath @{schema='pas.windows-pixel-leaf-freeze.v1';files=$leaves;source_selection=$Selection}
  function CheckAll {
    foreach($f in $buildInputs){CheckSha $f.path $f.sha256 $f.bytes}
    foreach($f in $leaves){CheckSha $f.path $f.sha256 $f.bytes}
    CheckSha $selected.index_path $selected.index_sha256
    CheckSha (Join-Path $root 'manifest.json') $selected.record_manifest_sha256
  }
  CheckAll;$pre=$true;$count=$leaves.Count
  # The unchanged native safety core keeps its 64 KiB freeze cap. Leaf PNGs
  # have a bounded separate manifest checked before AND after the owned stage;
  # the manifest and prior complete build freeze are direct frozen inputs.
  $nativeInputs=@($PSCommandPath,$leafPath,$Selection,$freezePath,$buildReceipt,$buildResult,$selected.index_path,
    (Join-Path $root 'manifest.json'))+@(Get-ChildItem -LiteralPath $BuildRoot -File|
       Where-Object Extension -in '.exe','.lib','.dll'|ForEach-Object FullName)
  try {
    & (Join-Path $PSScriptRoot 'native-stage.ps1') -StageName $StageName -Executable (Join-Path $BuildRoot 'pixel_chain.exe') `
      -Arguments @($Selection,$Report) -GateReceipt $GateReceipt -Inputs $nativeInputs -TotalSeconds $TotalSeconds
    if($LASTEXITCODE -ne 0){throw 'pixel-native-stage'}
    $nativeOk=$true
  } finally {CheckAll;$post=$true}
  $pixelReport=Get-Content -LiteralPath $Report -Raw|ConvertFrom-Json
  if($pixelReport.loaded_module_paths.Count -gt 128){throw 'loaded-module-cap'}
  $modules=@($pixelReport.loaded_module_paths|ForEach-Object {
    $file=Get-Item -LiteralPath $_
    @{path=$file.FullName;bytes=$file.Length;sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
  })
} catch {$failure=$_.Exception.Message}
WriteNewJson (Join-Path $control 'verification.json') @{schema='pas.windows-pixel-input-verification.v1';
  stage=$StageName;before_verified=$pre;after_verified=$post;frames_verified=$count;
  native_receipt_verified=$nativeOk;passed=($pre -and $post -and $nativeOk -and $null -eq $failure);
  reason=$failure;physical_human_gold=0;device_commands=0;loaded_module_backing_files_after_run=$modules;
  module_hash_scope='post-run backing files, not a hash of in-memory mapped images'}
if($failure){throw $failure}
exit 0
