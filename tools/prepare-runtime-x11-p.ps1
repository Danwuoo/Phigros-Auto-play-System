$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$campaignPath="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue"
$batchPath="$campaignPath/runtime-x11-p"
$outPath="$repoPath/out/x11-p"
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Bytes($p){if(!(Test-Path -LiteralPath $p)){return 0};[long](Get-ChildItem -LiteralPath $p -File -Recurse|Measure-Object Length -Sum).Sum}
function Save($name,$value){[IO.File]::WriteAllText("$batchPath/$name",($value|ConvertTo-Json -Depth 60)+"`n",[Text.UTF8Encoding]::new($false))}
if((Test-Path $batchPath) -or (Test-Path $outPath)){throw 'new roots required'}
$existing=(Bytes $campaignPath)+(Bytes "$repoPath/measurements/research-next-20261001")
$free=(Get-PSDrive C).Free
if($existing+128MB -gt 8GB -or $free -lt 3GB+128MB+5GB){throw 'capacity reserve'}
New-Item -ItemType Directory $batchPath,$outPath|Out-Null
$status=@(git status --short)
$dirty=@();foreach($l in $status){$p=$l.Substring(3);if(Test-Path -LiteralPath "$repoPath/$p" -PathType Leaf){$dirty+=@{path=$p;sha256=(Hash "$repoPath/$p")}}}
Save 'workspace-before.json' @{head=(git rev-parse HEAD);branch=(git branch --show-current);worktrees=@(git worktree list);status=$status;dirty_files=$dirty;main_strategy='50/27/11 donor/live0';reference='C36h tint1 37/19 experimental';campaign_plus_prior_bytes=$existing;free_bytes=$free;limits=@{batch=128MB;out=3GB;campaign=8GB;reserve=5GB}}
Copy-Item -LiteralPath "$repoPath/docs/RUNTIME_X11_P_PROTOCOL_20261003.md" -Destination "$batchPath/protocol-before.md"
$saved='98169a575bd7c3b7503849ceb4b91a934d50ce0f'
$freezeRoot="$campaignPath/acceptance36h-01"
$freeze=Get-Content "$freezeRoot/candidate36h-tint1-freeze.json" -Raw|ConvertFrom-Json
$base="$outPath/B0"
New-Item -ItemType Directory $base|Out-Null
& git archive --format=tar "--output=$outPath/saved-source.tar" $saved
if($LASTEXITCODE){throw 'archive'}
& tar -xf "$outPath/saved-source.tar" -C $base
if($LASTEXITCODE){throw 'extract'}
$checks=@()
foreach($p in $freeze.source_sha256.PSObject.Properties){
 $snapshot="$freezeRoot/candidate36h-tint1-source/$($p.Name)"
 if((Hash $snapshot) -ne $p.Value){throw 'snapshot hash'}
 $equal=$null
 if($p.Name -notlike 'out/*'){
  $dest="$base/$($p.Name)"
  $equal=[IO.File]::ReadAllText($snapshot).Replace("`r`n","`n") -ceq [IO.File]::ReadAllText($dest).Replace("`r`n","`n")
  if(!$equal -and $p.Name -ne 'CMakeLists.txt' -and $p.Name -notlike 'docs/*'){throw "saved content mismatch $($p.Name)"}
  Copy-Item -LiteralPath $snapshot -Destination $dest
 }
 $checks+=@{path=$p.Name;sha256=$p.Value;saved_normalized_equal=$equal}
}
$parentFiles=@();foreach($f in Get-ChildItem "$base/src","$base/include","$base/apps","$base/proto","$base/cmake","$base/tests" -File -Recurse){$p=$f.FullName.Substring($base.Length+1).Replace('\','/');$parentFiles+=@{path=$p;sha256=(Hash $f.FullName);listed_live_freeze=($freeze.source_sha256.PSObject.Properties.Name -contains $p)}}
Save 'parent-source-audit.json' @{saved_commit=$saved;live_freeze_sha256=(Hash "$freezeRoot/candidate36h-tint1-freeze.json");listed_checks=$checks;complete_saved_inputs=$parentFiles;unlisted_live_inputs_claim='saved commit reconstruction; original live compiled SHA not recorded for unlisted inputs';original_live_binary_sha256=$freeze.binary_sha256;original_generated_provenance_not_reused=$true}
Copy-Item -LiteralPath $base -Destination "$outPath/B1" -Recurse
$hook=@'
        // A newer complete snapshot with no current Note must withdraw a
        // pending Down immediately. Missing grace applies only after a
        // contact has started; it cannot license a new touch from old pixels.
        if(identity.submitted) if(const auto cursor=scheduler_.executed_steps(identity.intent);
           cursor&&*cursor==0) {
            cancel_contact(id,identity,"pending_down_current_object_missing");
            continue;
        }
'@
$hook=$hook.Replace("`r`n","`n")+"`n"
$anchor='        const auto grace=identity.kind==NoteKind::hold?60''000''000:identity.kind==NoteKind::flick?75''000''000:40''000''000;'
$p="$outPath/B1/src/game.cpp";$s=[IO.File]::ReadAllText($p).Replace("`r`n","`n")
if($s.Split(@($anchor),[StringSplitOptions]::None).Count -ne 2){throw 'hook anchor'}
$s=$s.Replace($anchor,$hook+$anchor)
if($s.Replace($hook,'') -cne [IO.File]::ReadAllText("$base/src/game.cpp").Replace("`r`n","`n")){throw 'inverse hook'}
if(![IO.File]::ReadAllText("$repoPath/src/game.cpp").Replace("`r`n","`n").Contains($hook.TrimEnd())){throw 'donor exact'}
[IO.File]::WriteAllText($p,$s,[Text.UTF8Encoding]::new($false))
foreach($role in @('B0','B1')){
 $export="$outPath/$role"
 $cm=[IO.File]::ReadAllText("$export/CMakeLists.txt").Replace("`r`n","`n")
 $begin=$cm.IndexOf('execute_process(COMMAND git rev-parse HEAD')
 $end=$cm.IndexOf('set(PAS_BUILD_HASH_JSON "{")',$begin)
 if($begin -lt 0 -or $end -lt 0){throw 'provenance anchor'}
 $cm=$cm.Substring(0,$begin)+"set(PAS_BUILD_COMMIT $saved)`nset(PAS_BUILD_DIRTY_JSON true)`nset(PAS_X11_VARIANT C36h-tint1-X11-P-$role)`n"+$cm.Substring($end)
 $hashBegin=$cm.IndexOf('foreach(PAS_BUILD_FILE src/game.cpp')
 $hashEnd=$cm.IndexOf('  file(SHA256',$hashBegin)
 $cm=$cm.Substring(0,$hashBegin)+@'
file(GLOB_RECURSE PAS_ALL_INPUTS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" src/*.cpp include/*.hpp apps/pas/*.cpp proto/* cmake/*)
list(APPEND PAS_ALL_INPUTS CMakeLists.txt vcpkg.json vcpkg-configuration.json)
list(SORT PAS_ALL_INPUTS)
foreach(PAS_BUILD_FILE IN LISTS PAS_ALL_INPUTS)
'@+"`n"+$cm.Substring($hashEnd)
 $cm=$cm.Replace('set(CMAKE_CXX_EXTENSIONS OFF)',"set(CMAKE_CXX_EXTENSIONS OFF)`nadd_compile_options(/utf-8)")
 $cm+="`ninclude(`"`${CMAKE_CURRENT_SOURCE_DIR}/x11.cmake`")`n"
 [IO.File]::WriteAllText("$export/CMakeLists.txt",$cm,[Text.UTF8Encoding]::new($false))
 $template=[IO.File]::ReadAllText("$export/cmake/session_build_provenance.hpp.in")
 $template+="`ninline constexpr auto pas_x11_variant=`"@PAS_X11_VARIANT@`";`n"
 [IO.File]::WriteAllText("$export/cmake/session_build_provenance.hpp.in",$template,[Text.UTF8Encoding]::new($false))
 $main=[IO.File]::ReadAllText("$export/apps/pas/main.cpp").Replace("`r`n","`n")
 $main="#include `"session_build_provenance.hpp`"`n"+$main
 $main=$main.Replace('    CLI::App app{',@'
    if(argc==2&&std::string_view(argv[1])=="x11-provenance") {
        std::cout<<nlohmann::json{{"variant",pas_x11_variant},{"parent_saved_commit",pas_session_build_commit},
            {"dirty_export",pas_session_build_dirty},{"compiled_source_sha256",nlohmann::json::parse(pas_session_source_hashes)}}.dump(2)<<'\n';
        return 0;
    }
    CLI::App app{
'@)
 [IO.File]::WriteAllText("$export/apps/pas/main.cpp",$main,[Text.UTF8Encoding]::new($false))
 $manual=[IO.File]::ReadAllText("$export/src/manual_session.cpp").Replace("`r`n","`n").Replace('{"compiled_source_sha256",json::parse(pas_session_source_hashes)},','{"experiment_variant",pas_x11_variant},{"parent_saved_commit",pas_session_build_commit},{"compiled_source_sha256",json::parse(pas_session_source_hashes)},')
 [IO.File]::WriteAllText("$export/src/manual_session.cpp",$manual,[Text.UTF8Encoding]::new($false))
 Copy-Item -LiteralPath "$repoPath/apps/runtime_x11_p/x11.cmake" -Destination "$export/x11.cmake"
}
Save 'export-contract.json' @{inverse_pending_hook=$true;donor_sha256=(Hash "$repoPath/src/game.cpp");metadata_common_changes=@('CMakeLists.txt','cmake/session_build_provenance.hpp.in','apps/pas/main.cpp','src/manual_session.cpp','x11.cmake');only_decision_change='B1/src/game.cpp 8 line pending hook';suppression=$false;unlisted_live_source_identity='unknown; complete saved reconstruction available'}
Copy-Item -LiteralPath "$freezeRoot/profile.json" -Destination "$batchPath/profile-parent.json"
Copy-Item -LiteralPath "$repoPath/measurements/game-semantics-20260927/touch-five/summary.json" -Destination "$batchPath/capability-reference.json"
Save 'capacity-before.json' @{batch=(Bytes $batchPath);out=(Bytes $outPath);campaign_plus_prior=(Bytes $campaignPath)+(Bytes "$repoPath/measurements/research-next-20261001");free=(Get-PSDrive C).Free}
