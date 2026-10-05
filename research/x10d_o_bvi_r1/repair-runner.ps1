$ErrorActionPreference='Stop'
$repoPath=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$batchPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi-r1'
$target=Join-Path $PSScriptRoot 'run.ps1'
if(Test-Path -LiteralPath $target){throw 'one runner repair already materialized'}
$s=Get-Content -LiteralPath (Join-Path $batchPath 'runner-original.ps1') -Raw
$s=$s.Replace("'hold-ownership-x10d-o-bvi'","'hold-ownership-x10d-o-bvi-r1'").Replace("'out/x10d-o-bvi'","'out/x10d-o-bvi-r1'")
$old='$campaign=Bytes (Split-Path $batchPath);$external=Bytes $PSScriptRoot;$batch=Bytes $batchPath;$out=Bytes (Join-Path $repoPath ''out/x10d-o-bvi-r1'');$aggregate=$campaign+45307809+72115+9546+54056+11851+$external'
$new='$campaign=Bytes (Split-Path $batchPath);$external=Bytes $PSScriptRoot;foreach($f in Get-ChildItem -LiteralPath (Join-Path $repoPath ''docs'') -Filter ''HOLD_OWNERSHIP_X10D_O_BVI_R1_*'' -File){$external+=$f.Length};$batch=4144966+(Bytes $batchPath);$out=374705+(Bytes (Join-Path $repoPath ''out/x10d-o-bvi-r1''));$aggregate=$campaign+45307809+72115+9546+54056+11851+104033+12289+$external'
if(-not $s.Contains($old)){throw 'capacity substitution not found'};$s=$s.Replace($old,$new)
$s=$s.Replace('using System.Threading.Tasks;','using System.Threading.Tasks;'+"`n"+'using System.Collections.Generic;')
$s=$s.Replace('public class BviRun','public class BviR1Run').Replace('[BviRun]::Run','[BviR1Run]::Run')
$anchor=' public class Result {'
$imports=@'
 [DllImport("kernel32.dll",SetLastError=true,EntryPoint="QueryInformationJobObject")] static extern bool QueryMembers(IntPtr h,int c,IntPtr data,int n,IntPtr ret);
 [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr OpenProcess(uint access,bool inherit,uint pid);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool IsProcessInJob(IntPtr process,IntPtr job,out bool member);
 [DllImport("kernel32.dll",SetLastError=true,CharSet=CharSet.Unicode)] static extern bool QueryFullProcessImageName(IntPtr process,uint flags,StringBuilder image,ref uint size);
 public class Member {public uint pid;public long creation_filetime;public string image="",missing_reason="";public bool membership_before,membership_after,exited,identity_complete;}
 public class Snapshot {public string stage="",utc="",missing_reason="";public long qpc_ticks;public double elapsed_s;public uint active,assigned,listed;public bool complete;public List<Member> members=new List<Member>();}
 static Snapshot Members(IntPtr job,string stage,Stopwatch sw,Dictionary<uint,long> known){
  var s=new Snapshot{stage=stage,utc=DateTime.UtcNow.ToString("o"),qpc_ticks=Stopwatch.GetTimestamp(),elapsed_s=sw.Elapsed.TotalSeconds};
  IntPtr buffer=Marshal.AllocHGlobal(8+64*IntPtr.Size);
  try{
   s.active=Active(job);
   if(!QueryMembers(job,3,buffer,8+64*IntPtr.Size,IntPtr.Zero)){s.missing_reason="job PID-list query error:"+Marshal.GetLastWin32Error();return s;}
   s.assigned=unchecked((uint)Marshal.ReadInt32(buffer,0));s.listed=unchecked((uint)Marshal.ReadInt32(buffer,4));
   if(s.listed>64||s.assigned>64){s.missing_reason="owned member evidence cap64 exceeded";return s;}
   s.complete=true;
   for(int i=0;i<s.listed;i++){
    long raw=IntPtr.Size==8?Marshal.ReadInt64(buffer,8+i*8):unchecked((uint)Marshal.ReadInt32(buffer,8+i*4));
    var m=new Member{pid=unchecked((uint)raw)};s.members.Add(m);IntPtr p=OpenProcess(0x00101000,false,m.pid);
    if(p==IntPtr.Zero){m.missing_reason="OpenProcess error:"+Marshal.GetLastWin32Error()+"; exited/inaccessible identity not inferred";s.complete=false;continue;}
    try{
     bool before=false,after=false;long a,b,c,d,verify;
     if(!IsProcessInJob(p,job,out before)||!before){m.missing_reason="PID not verified in this owned job (exit/reuse possible)";s.complete=false;continue;}m.membership_before=true;
     if(!GetProcessTimes(p,out a,out b,out c,out d)){m.missing_reason="creation unavailable:"+Marshal.GetLastWin32Error();s.complete=false;continue;}m.creation_filetime=a;
     uint n=32768;var image=new StringBuilder((int)n);
     if(!QueryFullProcessImageName(p,0,image,ref n)){m.missing_reason="image unavailable:"+Marshal.GetLastWin32Error();s.complete=false;continue;}m.image=image.ToString();m.exited=WaitForSingleObject(p,0)==0;
     if(!GetProcessTimes(p,out verify,out b,out c,out d)||verify!=a||!IsProcessInJob(p,job,out after)||!after){m.missing_reason="handle identity/membership changed during observation";s.complete=false;continue;}m.membership_after=true;
     long previous;if(known.TryGetValue(m.pid,out previous)&&previous!=a){m.missing_reason="PID reused; creation changed, no same-process claim";s.complete=false;continue;}known[m.pid]=a;m.identity_complete=true;
    }finally{CloseHandle(p);}
   }
   return s;
  }finally{Marshal.FreeHGlobal(buffer);}
 }
 static void Observe(Result r,IntPtr job,string stage,Stopwatch sw,Dictionary<uint,long>known){if(r.snapshots.Count>=8)throw new Exception("snapshot evidence cap8");var s=Members(job,stage,sw,known);r.snapshots.Add(s);r.identity_trusted &= s.complete;}
'@
if(-not $s.Contains($anchor)){throw 'result anchor'};$s=$s.Replace($anchor,$imports+"`n"+$anchor)
$s=$s.Replace('public uint pid,exit_code;','public bool identity_trusted=true;public long qpc_frequency=Stopwatch.Frequency;public List<Snapshot> snapshots=new List<Snapshot>();public uint pid,exit_code;')
$s=$s.Replace('var r=new Result();var sw=Stopwatch.StartNew();','var r=new Result();var known=new Dictionary<uint,long>();var sw=Stopwatch.StartNew();')
$s=$s.Replace('r.assigned=true;CloseHandle(ow);','r.assigned=true;Observe(r,job,"assigned-suspended",sw,known);if(!r.identity_trusted)throw new Exception("member-identity-untrusted");CloseHandle(ow);')
$s=$s.Replace('r.active_at_exit=Active(job);var q=Stopwatch.StartNew();','r.active_at_exit=Active(job);Observe(r,job,"root-exit",sw,known);var q=Stopwatch.StartNew();')
$s=$s.Replace('r.natural_quiescence=Active(job)==0;if(!r.natural_quiescence)','r.natural_quiescence=Active(job)==0;Observe(r,job,"quiescence-end",sw,known);if(!r.natural_quiescence)')
$s=$s.Replace('r.streams_completed=Task.WaitAll','if(!r.identity_trusted)throw new Exception("member-identity-untrusted");r.streams_completed=Task.WaitAll')
$s=$s.Replace('if(job!=IntPtr.Zero){TerminateJobObject(job,125);','if(job!=IntPtr.Zero){Observe(r,job,"before-owned-cleanup",sw,known);Need(TerminateJobObject(job,125),"owned-job-terminate");')
$s=$s.Replace('r.cleanup_zero=Active(job)==0;}if(p.process','r.cleanup_zero=Active(job)==0;Observe(r,job,"after-owned-cleanup",sw,known);}if(p.process')
$s=$s.Replace('r.active_final=Active(job);CloseHandle(job);','r.active_final=Active(job);Observe(r,job,"final-before-close",sw,known);CloseHandle(job);')
$s=$s.Replace('-not $result.natural_quiescence){exit 125}','-not $result.natural_quiescence -or -not $result.identity_trusted){exit 125}')
$s=$s.Replace('Write-Output ($result|ConvertTo-Json -Compress)','Write-Output (@{name=$Name;native_exit=$result.exit_code;reason=$result.reason;natural_quiescence=$result.natural_quiescence;identity_trusted=$result.identity_trusted;cleanup_zero=$result.cleanup_zero;active_final=$result.active_final;snapshots=$result.snapshots.Count;elapsed_s=$result.elapsed_s}|ConvertTo-Json -Compress)')
[IO.File]::WriteAllText($target,$s,[Text.UTF8Encoding]::new($false))
$diff=& git diff --no-index -- (Join-Path $batchPath 'runner-original.ps1') $target
if($LASTEXITCODE -gt 1){throw 'diff error'}
[IO.File]::WriteAllText((Join-Path $batchPath 'runner-repair.diff'),($diff -join "`n")+"`n",[Text.UTF8Encoding]::new($false))
$v=@{runner_repair=1;original_sha256=(Get-FileHash (Join-Path $batchPath 'runner-original.ps1')).Hash.ToLowerInvariant();new_sha256=(Get-FileHash $target).Hash.ToLowerInvariant();scope='new root/carry plus owned member PID/creation/image/observation timing/membership verification; same15s gate';utc=[DateTime]::UtcNow.ToString('o')}
[IO.File]::WriteAllText((Join-Path $batchPath 'runner-repair.json'),($v|ConvertTo-Json)+"`n",[Text.UTF8Encoding]::new($false))
Write-Output ($v|ConvertTo-Json -Compress)
