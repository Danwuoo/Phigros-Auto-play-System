# Maintenance enumeration only; preserve the original limits and reserves.
function Capacity($stage,$phase){
 $mainOut=Join-Path $repo 'out'
 $roots=@(Get-ChildItem -LiteralPath $mainOut -Directory|Where-Object Name -like 'prelive*')
 foreach($relative in @('windows-handoff','retained/consolidation-20261008','consolidation-20261008-01','s55aa03')){$path=Join-Path $mainOut $relative;if(Test-Path -LiteralPath $path){$roots+=@(Get-Item -LiteralPath $path)}}
 $rows=@();$options=[IO.EnumerationOptions]::new();$options.RecurseSubdirectories=$true;$options.IgnoreInaccessible=$false;$options.AttributesToSkip=[IO.FileAttributes]::ReparsePoint
 foreach($root in $roots){$sum=[long]0;foreach($file in [IO.DirectoryInfo]::new($root.FullName).EnumerateFiles('*',$options)){$sum+=$file.Length};$rows+=@(@{path=$root.FullName;bytes=$sum})}
 $total=[long](($rows|Measure-Object bytes -Sum).Sum);$free=[long](Get-PSDrive C).Free
 $reserve=if($phase -eq 'before' -and $Operation -in @('pipeline','pixels','tests','family')){1073741824}elseif($phase -eq 'before' -and $Operation -eq 'build'){if($CoreDonor){536870912}else{1610612736}}else{0}
 $valid=$total+$reserve -le 51539607552 -and $free-$reserve -ge 21474836480
 NewJson (Join-Path $pkg ($stage+'-capacity-'+$phase+'.json')) @{schema='pas.prelive-capacity.v1';stage=$stage;phase=$phase;roots=$rows;bytes=$total;reserve=$reserve;free=$free;cap=51539607552;authorization='human approval 2026-10-08';minimum_free=21474836480;pass=$valid;metadata_seal_cap=67108864;enumeration='DirectoryInfo.EnumerateFiles, inaccessible errors retained, reparse excluded';short_source_projection_included=$true}
 if(-not $valid){throw 'capacity-gate'}
}
