param(
    [Parameter(Mandatory=$true)][string]$Serial,
    [Parameter(Mandatory=$true)][string]$Avd,
    [Parameter(Mandatory=$true)][string]$Adb
)
$ErrorActionPreference='Stop'
$package='org.pas.touchfixture.evidence.cpp'
$expectedSha='d4ae58c7f9561b4262791a1711e2688db233ec054b10400e88b5e60fccfc34a2'
if($Serial -notmatch '^emulator-[0-9]+$' -or [string]::IsNullOrWhiteSpace($Avd)){
    throw 'An explicit emulator serial and AVD name are required.'
}
if(-not (Test-Path -LiteralPath $Adb -PathType Leaf)){throw 'ADB executable does not exist.'}
function Invoke-FixtureAdb([string[]]$Arguments) {
    $response=& $Adb -s $Serial @Arguments
    if($LASTEXITCODE -ne 0){throw "ADB failed ($LASTEXITCODE)."}
    return $response
}
$actualAvd=@(Invoke-FixtureAdb @('emu','avd','name'))
if($actualAvd.Count -eq 0 -or $actualAvd[0].Trim() -ne $Avd){throw 'Selected AVD does not match.'}
$packages=@(Invoke-FixtureAdb @('shell','pm','path',$package))
if($packages.Count -ne 1 -or $packages[0] -notmatch '^package:(/data/app/[^\r\n]+/base\.apk)$'){
    throw 'Expected exactly one installed evidence-fixture APK.'
}
$apkPath=$Matches[1]
$hashReply=@(Invoke-FixtureAdb @('shell','sha256sum',$apkPath))
if($hashReply.Count -ne 1 -or ($hashReply[0] -split '\s+')[0] -ne $expectedSha){
    throw 'Installed fixture SHA does not match the verified v3 artifact.'
}
# This user-invoked launcher only restarts the private test fixture. It never
# launches/selects a song, injects touches, or removes existing evidence files.
Invoke-FixtureAdb @('shell','setprop','debug.pas.touch_evidence','1') | Out-Null
$recorderSwitch=@(Invoke-FixtureAdb @('shell','getprop','debug.pas.touch_evidence'))
if($recorderSwitch.Count -ne 1 -or $recorderSwitch[0].Trim() -ne '1'){
    throw 'Fixture timestamp recorder setting did not persist.'
}
Invoke-FixtureAdb @('shell','am','force-stop',$package) | Out-Null
Invoke-FixtureAdb @('shell','am','start','-W','-n',"$package/android.app.NativeActivity") | Out-Null
Write-Output 'PAS Touch Evidence v3 is open. Its bounded recorder runs for up to 60 seconds.'
Write-Output 'Leave this screen open without touching it while the host probe runs.'
