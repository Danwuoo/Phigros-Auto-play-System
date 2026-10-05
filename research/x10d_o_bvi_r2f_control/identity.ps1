function InspectIdentity($path,$expected,[long]$now,[long]$deadline){
 if($now -ge $deadline){return @{state='timeout';ready=$false;qpc=$now}}
 try{$h=ReadStream $path;try{if($h.Length -gt 4096){return @{state='over-cap';ready=$false;qpc=$now}};$reader=[IO.StreamReader]::new($h,[Text.Encoding]::UTF8,$true,4096,$true);try{$raw=$reader.ReadToEnd()}finally{$reader.Dispose()}}finally{$h.Dispose()}}catch{return @{state='read-incomplete';ready=$false;qpc=$now}}
 if([string]::IsNullOrWhiteSpace($raw)){return @{state='empty';ready=$false;qpc=$now}}
 try{$v=$raw|ConvertFrom-Json -Depth 8}catch{return @{state='parse-incomplete';ready=$false;qpc=$now}}
 if($null -eq $v){return @{state='null';ready=$false;qpc=$now}}
 if($v.pending){return @{state='pending';ready=$false;qpc=$now}}
 if($v.schema -cne 'r2f.identity.v1' -or $v.status -cne 'published' -or $v.pending -ne $false -or -not $v.pid -or -not $v.creation_filetime -or -not $v.image -or -not $v.publish_qpc -or $v.qpc_frequency -ne [Diagnostics.Stopwatch]::Frequency){return @{state='incomplete-schema';ready=$false;qpc=$now}}
 if($v.attempt -cne $expected.attempt -or $v.pid -ne $expected.pid -or $v.creation_filetime -ne $expected.creation_filetime -or $v.image -cne $expected.image){return @{state='identity-mismatch';ready=$false;qpc=$now}}
 if($v.publish_qpc -gt $now -or $now-$v.publish_qpc -gt 3*[Diagnostics.Stopwatch]::Frequency){return @{state='stale-publication';ready=$false;qpc=$now}}
 @{state='ready';ready=$true;qpc=$now;value=$v}
}
function PublishIdentity($path,$v){
 $reservation=Json $path;if($null -eq $reservation -or $reservation.attempt -cne $Attempt -or -not $reservation.pending){throw 'identity-reservation'}
 $v.schema='r2f.identity.v1';$v.status='published';$v.pending=$false;$v.publish_qpc=[Diagnostics.Stopwatch]::GetTimestamp();$v.qpc_frequency=[Diagnostics.Stopwatch]::Frequency
 $h=[IO.FileStream]::new($path,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::ReadWrite)
 try{PutHandle $h $v 4096}finally{$h.Dispose()}
}
