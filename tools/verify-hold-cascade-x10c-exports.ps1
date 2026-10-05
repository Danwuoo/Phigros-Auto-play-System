$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$batchPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue/hold-cascade-x10c'
$binding=Get-Content -LiteralPath "$batchPath/patch-bindings.json" -Raw|ConvertFrom-Json
$checks=@()
foreach($e in $binding.exports){
  $changed=@();$unchanged=0
  foreach($f in Get-ChildItem -LiteralPath $e.actual_export_root -File -Recurse){
    $rel=$f.FullName.Substring($e.actual_export_root.Length+1).Replace('\','/')
    $a=[IO.File]::ReadAllText((Join-Path $e.parent_root $rel)).Replace("`r`n","`n")
    $b=[IO.File]::ReadAllText($f.FullName).Replace("`r`n","`n")
    if($a -eq $b){++$unchanged;continue}
    if($rel -eq 'src/game.cpp'){
      $b=$b.Replace('#include "x10c_diagnostics.hpp"'+"`n",'')
      $evaluation=if($e.role -eq 'variant'){'x10b::current_interior_claim(f,note,*line,claimed_outlines)'}else{'x10c::enabled&&x10b::current_interior_claim(f,note,*line,claimed_outlines)'}
      $b=$b.Replace('        const bool x10c_claim='+$evaluation+';'+"`n",'')
      $applied=if($e.role -eq 'variant'){'x10c_claim'}else{'false'}
      $b=$b.Replace('        x10c::fallback(f,note,*line,claimed_outlines,track,notes,approaching,x10c_claim,'+$applied+',x1_trace_enabled);'+"`n",'')
      if($e.role -eq 'variant'){$b=$b.Replace('        if(x10c_claim) continue;','        if(x10b::current_interior_claim(f,note,*line,claimed_outlines)) continue;')}
    }elseif($rel -eq 'include/pas/game.hpp'){$b=$b.Replace(' const std::vector<GameTrackHistory>& replay_x10c_tracks() const { return tracks_; }','')
    }elseif($rel -eq 'include/pas/game_session.hpp'){$b=$b.Replace(' const std::vector<GameTrackHistory>& replay_x10c_tracks() const { return observer_.replay_x10c_tracks(); }','')
    }else{throw "unexpected changed source $rel"}
    if($a -ne $b){throw "non-diagnostic source difference $($e.role)/$rel"}
    $changed+=$rel
  }
  $checks+=@{role=$e.role;unchanged_files=$unchanged;diagnostic_only_changed_files=$changed;inverse_patch_equals_parent=$true}
}
$v=@{schema=1;checks=$checks;note='Inverse patch restores each corresponding frozen parent exactly after newline normalization. Variant evaluates same predicate once and uses same continue; baseline evaluation is trace-gated and read-only. Const getters and diagnostics do not update state.'}
$data=[Text.UTF8Encoding]::new($false).GetBytes(($v|ConvertTo-Json -Depth 10)+"`n");$s=[IO.File]::Open("$batchPath/diagnostic-only-export-audit.json",[IO.FileMode]::CreateNew,[IO.FileAccess]::Write);try{$s.Write($data,0,$data.Length)}finally{$s.Dispose()}
$v|ConvertTo-Json -Depth 8
