param([Parameter(Mandatory)][string]$Name,[Parameter(Mandatory)][string]$Exe,[string[]]$Arguments=@(),[int]$TimeoutSeconds=180)
$ErrorActionPreference='Stop'
$repoPath=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$batchPath=Join-Path $repoPath 'measurements/game-assist/2026-09-30-m0-manual-continue/hold-ownership-x10d-o-bvi'
function Save($suffix,$v){$p=Join-Path $batchPath "$Name-$suffix.json";if(Test-Path -LiteralPath $p){throw 'receipt exists'};[IO.File]::WriteAllText($p,($v|ConvertTo-Json -Depth 20)+"`n",[Text.UTF8Encoding]::new($false))}
function Bytes($p){$s=[long]0;if(Test-Path -LiteralPath $p){foreach($f in Get-ChildItem -LiteralPath $p -File -Recurse){$s+=$f.Length}};return $s}
if($Name -notmatch '^[a-z0-9-]{1,60}$' -or $TimeoutSeconds -lt 1 -or $TimeoutSeconds -gt 300){throw 'command bound'}
if(@(Get-ChildItem -LiteralPath $batchPath -Filter '*-command.json').Count -ge 24){throw 'attempt cap'}
$campaign=Bytes (Split-Path $batchPath);$external=Bytes $PSScriptRoot;foreach($f in Get-ChildItem -LiteralPath (Join-Path $repoPath 'docs') -Filter 'HOLD_OWNERSHIP_X10D_O_BVI_R1_*' -File){$external+=$f.Length};$batch=4144966+(Bytes $batchPath);$out=374705+(Bytes (Join-Path $repoPath 'out/x10d-o-bvi-r1'));$aggregate=$campaign+45307809+72115+9546+54056+11851+104033+12289+$external
if($aggregate -gt 8589934592 -or $batch+$external -gt 58720256 -or $out -gt 268435456 -or (Get-PSDrive C).Free -lt 5704253440){throw 'capacity'}
Save 'command' @{exe=$Exe;argv=$Arguments;timeout_s=$TimeoutSeconds;workers_max=2;stdout_cap=4194304;stderr_cap=4194304;job='suspended launch, owned kill-on-close job, natural quiescence15s, failure cleanup5s';capacity=@{batch=$batch;external=$external;out=$out;aggregate=$aggregate};runner_sha256=(Get-FileHash -LiteralPath $PSCommandPath).Hash.ToLowerInvariant()}
try {
Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Text;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using System.Collections.Generic;
using Microsoft.Win32.SafeHandles;
public class BviR1Run {
 [StructLayout(LayoutKind.Sequential)] struct SA { public int n; public IntPtr sd; public int inherit; }
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct SI {public int cb; public string reserved,desktop,title; public uint x,y,xs,ys,xc,yc,fill,flags; public ushort show,reserved2; public IntPtr reservedp,input,output,error;}
 [StructLayout(LayoutKind.Sequential)] struct PI {public IntPtr process,thread;public uint pid,tid;}
 [StructLayout(LayoutKind.Sequential)] struct BasicLimit {public long perProcess,perJob; public uint flags; public UIntPtr min,max; public uint active; public UIntPtr affinity; public uint priority,scheduling;}
 [StructLayout(LayoutKind.Sequential)] struct IO {public ulong ro,wo,oo,rb,wb,ob;}
 [StructLayout(LayoutKind.Sequential)] struct Extended {public BasicLimit basic;public IO io;public UIntPtr processMemory,jobMemory,peakProcess,peakJob;}
 [StructLayout(LayoutKind.Sequential)] struct Accounting {public long user,kernel,userPeriod,kernelPeriod;public uint faults,total,active,terminated;}
 [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr CreateJobObject(IntPtr s,string name);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool SetInformationJobObject(IntPtr h,int c,ref Extended i,int n);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool QueryInformationJobObject(IntPtr h,int c,out Accounting i,int n,IntPtr ret);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool AssignProcessToJobObject(IntPtr h,IntPtr p);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool TerminateJobObject(IntPtr h,uint code);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool CreatePipe(out IntPtr r,out IntPtr w,ref SA s,int n);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool SetHandleInformation(IntPtr h,uint mask,uint f);
 [DllImport("kernel32.dll",SetLastError=true,CharSet=CharSet.Unicode)] static extern bool CreateProcess(string app,StringBuilder cmd,IntPtr p,IntPtr t,bool inherit,uint flags,IntPtr env,string cwd,ref SI si,out PI pi);
 [DllImport("kernel32.dll",SetLastError=true)] static extern uint ResumeThread(IntPtr h);
 [DllImport("kernel32.dll")] static extern uint WaitForSingleObject(IntPtr h,uint ms);
 [DllImport("kernel32.dll")] static extern bool GetExitCodeProcess(IntPtr h,out uint code);
 [DllImport("kernel32.dll")] static extern bool GetProcessTimes(IntPtr h,out long creation,out long exit,out long kernel,out long user);
 [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
 [DllImport("kernel32.dll")] static extern bool TerminateProcess(IntPtr h,uint code);
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
 public class Result {public bool identity_trusted=true;public long qpc_frequency=Stopwatch.Frequency;public List<Snapshot> snapshots=new List<Snapshot>();public uint pid,exit_code;public long creation_filetime;public string reason="";public bool assigned,root_exited,natural_quiescence,cleanup_zero,streams_completed;public uint active_at_exit,active_final;public double elapsed_s;public long stdout_seen,stderr_seen;public int stdout_written,stderr_written;}
 class Pump {public long seen;public int written;public int overflow;public Task task;public Pump(IntPtr h,string path){task=Task.Run(()=>{using(var input=new FileStream(new SafeFileHandle(h,true),FileAccess.Read,4096,false))using(var output=new FileStream(path,FileMode.CreateNew,FileAccess.Write,FileShare.Read)){var b=new byte[4096];int n;while((n=input.Read(b,0,b.Length))>0){seen+=n;int k=Math.Min(n,4194304-written);if(k>0){output.Write(b,0,k);written+=k;}if(seen>4194304)Interlocked.Exchange(ref overflow,1);}}});}}
 static void Need(bool ok,string op){if(!ok)throw new Exception(op+":"+Marshal.GetLastWin32Error());}
 static uint Active(IntPtr j){Accounting a;Need(QueryInformationJobObject(j,1,out a,Marshal.SizeOf(typeof(Accounting)),IntPtr.Zero),"job-accounting");return a.active;}
 static string Quote(string s){if(s.Length>0&&s.IndexOfAny(new char[]{' ','\t','"'})<0)return s;var b=new StringBuilder("\"");int sl=0;foreach(char c in s){if(c=='\\'){sl++;continue;}if(c=='"'){b.Append('\\',sl*2+1);b.Append(c);}else{b.Append('\\',sl);b.Append(c);}sl=0;}b.Append('\\',sl*2);b.Append('"');return b.ToString();}
 public static Result Run(string exe,string[] args,string cwd,string so,string se,int timeout){
  var r=new Result();var known=new Dictionary<uint,long>();var sw=Stopwatch.StartNew();IntPtr job=IntPtr.Zero,or=IntPtr.Zero,ow=IntPtr.Zero,er=IntPtr.Zero,ew=IntPtr.Zero;PI p=new PI();Pump op=null,ep=null;
  try{job=CreateJobObject(IntPtr.Zero,null);Need(job!=IntPtr.Zero,"create-job");var ex=new Extended();ex.basic.flags=0x2000;Need(SetInformationJobObject(job,9,ref ex,Marshal.SizeOf(typeof(Extended))),"job-kill-on-close");
   var sa=new SA{n=Marshal.SizeOf(typeof(SA)),inherit=1};Need(CreatePipe(out or,out ow,ref sa,4096),"stdout-pipe");Need(CreatePipe(out er,out ew,ref sa,4096),"stderr-pipe");Need(SetHandleInformation(or,1,0),"stdout-noninherit");Need(SetHandleInformation(er,1,0),"stderr-noninherit");
   var si=new SI{cb=Marshal.SizeOf(typeof(SI)),flags=0x100,output=ow,error=ew,input=IntPtr.Zero};var cmd=new StringBuilder(Quote(exe));foreach(string arg in args)cmd.Append(" ").Append(Quote(arg));
   Need(CreateProcess(exe,cmd,IntPtr.Zero,IntPtr.Zero,true,0x08000004,IntPtr.Zero,cwd,ref si,out p),"suspended-create");r.pid=p.pid;long a,b,c,d;Need(GetProcessTimes(p.process,out a,out b,out c,out d),"creation-time");r.creation_filetime=a;
   Need(AssignProcessToJobObject(job,p.process),"job-assign");r.assigned=true;Observe(r,job,"assigned-suspended",sw,known);if(!r.identity_trusted)throw new Exception("member-identity-untrusted");CloseHandle(ow);ow=IntPtr.Zero;CloseHandle(ew);ew=IntPtr.Zero;op=new Pump(or,so);or=IntPtr.Zero;ep=new Pump(er,se);er=IntPtr.Zero;Need(ResumeThread(p.thread)!=0xffffffff,"resume");
   while(WaitForSingleObject(p.process,0)==258){if(sw.Elapsed.TotalSeconds>=timeout)throw new Exception("timeout");if(op.overflow!=0||ep.overflow!=0)throw new Exception("log-cap");Thread.Sleep(50);}
   r.root_exited=true;Need(GetExitCodeProcess(p.process,out r.exit_code),"exit-code");r.active_at_exit=Active(job);Observe(r,job,"root-exit",sw,known);var q=Stopwatch.StartNew();while(Active(job)>0&&q.Elapsed.TotalSeconds<15){Thread.Sleep(50);}r.natural_quiescence=Active(job)==0;Observe(r,job,"quiescence-end",sw,known);if(!r.natural_quiescence)throw new Exception("descendants-nonquiescent");
   if(!r.identity_trusted)throw new Exception("member-identity-untrusted");r.streams_completed=Task.WaitAll(new Task[]{op.task,ep.task},5000);if(!r.streams_completed)throw new Exception("streams-nonquiescent");if(op.overflow!=0||ep.overflow!=0)throw new Exception("log-cap");r.cleanup_zero=true;
  }catch(Exception e){r.reason=e.Message;if(job!=IntPtr.Zero){Observe(r,job,"before-owned-cleanup",sw,known);Need(TerminateJobObject(job,125),"owned-job-terminate");var q=Stopwatch.StartNew();while(Active(job)>0&&q.Elapsed.TotalSeconds<5){Thread.Sleep(50);}r.cleanup_zero=Active(job)==0;Observe(r,job,"after-owned-cleanup",sw,known);}if(p.process!=IntPtr.Zero&&!r.assigned)TerminateProcess(p.process,125);}
  finally{if(job!=IntPtr.Zero){r.active_final=Active(job);Observe(r,job,"final-before-close",sw,known);CloseHandle(job);}if(p.thread!=IntPtr.Zero)CloseHandle(p.thread);if(p.process!=IntPtr.Zero)CloseHandle(p.process);foreach(IntPtr h in new[]{or,ow,er,ew})if(h!=IntPtr.Zero)CloseHandle(h);if(op!=null&&ep!=null){Task.WaitAll(new Task[]{op.task,ep.task},5000);r.stdout_seen=op.seen;r.stderr_seen=ep.seen;r.stdout_written=op.written;r.stderr_written=ep.written;}r.elapsed_s=sw.Elapsed.TotalSeconds;}return r;
 }
}
'@
$result=[BviR1Run]::Run($Exe,$Arguments,$repoPath,(Join-Path $batchPath "$Name.stdout.log"),(Join-Path $batchPath "$Name.stderr.log"),$TimeoutSeconds)
Save 'exit' $result
Write-Output (@{name=$Name;native_exit=$result.exit_code;reason=$result.reason;natural_quiescence=$result.natural_quiescence;identity_trusted=$result.identity_trusted;cleanup_zero=$result.cleanup_zero;active_final=$result.active_final;snapshots=$result.snapshots.Count;elapsed_s=$result.elapsed_s}|ConvertTo-Json -Compress)
if($result.reason -or -not $result.cleanup_zero -or -not $result.natural_quiescence -or -not $result.identity_trusted){exit 125};exit ([int]$result.exit_code)
}catch{Save 'tool-failure' @{error=$_.Exception.Message;native_launch_possible=$false;trusted=$false};Write-Error $_;exit 126}
