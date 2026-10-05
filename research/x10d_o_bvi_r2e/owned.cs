using System;
using System.IO;
using System.Text;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using System.Collections.Generic;
using Microsoft.Win32.SafeHandles;
public class R2EOwned {
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
 public class Member {public uint pid; public long? creation_filetime; public string image,missing_reason;public bool? membership_before,membership_after,exited;public bool identity_complete;}
 public class Snapshot {public string stage,utc,missing_reason;public long qpc_ticks;public double elapsed_s;public uint? active,assigned,listed;public bool complete;public List<Member> members=new List<Member>();}
 public class Result {
  public string launch="not_launched",reason="",cleanup="unknown",root_image,descendants="unknown";
  public bool? create_attempted,created,assigned,resume_attempted,resumed,root_exited,natural_quiescence,identity_trusted,streams_completed;
  public uint? pid,exit_code,active_at_exit,active_final;public long? creation_filetime,root_exit_qpc;
  public long qpc_frequency=Stopwatch.Frequency;public double elapsed_s,quiescence_s;public int runner_exit=126;
  public long stdout_seen,stderr_seen;public int stdout_written,stderr_written;public bool stdout_overflow,stderr_overflow;
  public List<Snapshot> snapshots=new List<Snapshot>();public List<string> errors=new List<string>();public bool errors_overflow;public List<uint> held_members_exited=new List<uint>();
 }
 public static void Error(Result r,string stage,Exception e){if(r.errors.Count<16)r.errors.Add(stage+":"+e.Message);else r.errors_overflow=true;}
 public static void Try(Result r,string stage,Action a){try{a();}catch(Exception e){Error(r,stage,e);}}
 public static void Created(Result r,uint pid){r.created=true;r.launch="launched";r.pid=pid;}
 public static void Exited(Result r,uint code){r.root_exited=true;r.exit_code=code;r.root_exit_qpc=Stopwatch.GetTimestamp();}
 static void Need(bool ok,string op){if(!ok)throw new Exception(op+":"+Marshal.GetLastWin32Error());}
 static uint Active(IntPtr j){Accounting a;Need(QueryInformationJobObject(j,1,out a,Marshal.SizeOf(typeof(Accounting)),IntPtr.Zero),"job-accounting");return a.active;}
 static Snapshot Members(IntPtr job,string stage,Stopwatch sw,Dictionary<uint,long> known,Dictionary<uint,IntPtr> held){
  var s=new Snapshot{stage=stage,utc=DateTime.UtcNow.ToString("o"),qpc_ticks=Stopwatch.GetTimestamp(),elapsed_s=sw.Elapsed.TotalSeconds};
  IntPtr buffer=Marshal.AllocHGlobal(8+64*IntPtr.Size);
  try{
   s.active=Active(job);Need(QueryMembers(job,3,buffer,8+64*IntPtr.Size,IntPtr.Zero),"job-members");
   s.assigned=unchecked((uint)Marshal.ReadInt32(buffer,0));s.listed=unchecked((uint)Marshal.ReadInt32(buffer,4));
   if(s.assigned>64||s.listed!=s.assigned||s.active!=s.listed)throw new Exception("incomplete-member-list");
   s.complete=true;
   for(int i=0;i<s.listed;i++){
    long raw=IntPtr.Size==8?Marshal.ReadInt64(buffer,8+i*8):unchecked((uint)Marshal.ReadInt32(buffer,8+i*4));
    var m=new Member{pid=unchecked((uint)raw)};s.members.Add(m);IntPtr p=OpenProcess(0x00101000,false,m.pid);
    try{
     Need(p!=IntPtr.Zero,"member-open");bool before,after;long a,b,c,d,verify;
     Need(IsProcessInJob(p,job,out before)&&before,"membership-before");m.membership_before=true;
     Need(GetProcessTimes(p,out a,out b,out c,out d),"member-creation");m.creation_filetime=a;
     uint n=32768;var image=new StringBuilder((int)n);Need(QueryFullProcessImageName(p,0,image,ref n),"member-image");m.image=image.ToString();
     var wait=WaitForSingleObject(p,0);if(wait!=0&&wait!=258)throw new Exception("member-wait");m.exited=wait==0;
     Need(GetProcessTimes(p,out verify,out b,out c,out d)&&verify==a,"creation-recheck");Need(IsProcessInJob(p,job,out after)&&after,"membership-after");m.membership_after=true;
     long old;if(known.TryGetValue(m.pid,out old)&&old!=a)throw new Exception("pid-reused");known[m.pid]=a;m.identity_complete=true;
    }catch(Exception e){m.missing_reason=e.Message;s.complete=false;}
    finally{if(p!=IntPtr.Zero){if(m.identity_complete&&!held.ContainsKey(m.pid))held[m.pid]=p;else CloseHandle(p);}}
   }
  }catch(Exception e){s.missing_reason=e.Message;s.complete=false;}finally{Marshal.FreeHGlobal(buffer);}return s;
 }
 static void Observe(Result r,IntPtr job,string stage,Stopwatch sw,Dictionary<uint,long> known,Dictionary<uint,IntPtr> held){
  if(r.snapshots.Count>=8)throw new Exception("snapshot-cap8");var s=Members(job,stage,sw,known,held);r.snapshots.Add(s);
  r.identity_trusted=r.identity_trusted!=false&&s.complete;if(!s.complete)throw new Exception("member-identity-untrusted:"+stage);
 }
 class Pump {
  public long seen;public int written,overflow;public Task task;
  public Pump(IntPtr h,FileStream output,int cap){task=Task.Run(()=>{using(var input=new FileStream(new SafeFileHandle(h,true),FileAccess.Read,4096,false)){var b=new byte[4096];int n;while((n=input.Read(b,0,b.Length))>0){seen+=n;int k=Math.Min(n,cap-written);if(k>0){output.Write(b,0,k);written+=k;}if(seen>cap)Interlocked.Exchange(ref overflow,1);}output.Flush(true);}});}
 }
 static string Quote(string s){if(s.Length>0&&s.IndexOfAny(new char[]{' ','\t','"'})<0)return s;var b=new StringBuilder("\"");int sl=0;foreach(char c in s){if(c=='\\'){sl++;continue;}if(c=='"'){b.Append('\\',sl*2+1);b.Append(c);}else{b.Append('\\',sl);b.Append(c);}sl=0;}b.Append('\\',sl*2);b.Append('"');return b.ToString();}
 public static void Run(Result r,string exe,string[] args,string cwd,FileStream so,FileStream se,int total,int cap,Action<string,Result> checkpoint){
  var known=new Dictionary<uint,long>();var held=new Dictionary<uint,IntPtr>();var sw=Stopwatch.StartNew();IntPtr job=IntPtr.Zero,or=IntPtr.Zero,ow=IntPtr.Zero,er=IntPtr.Zero,ew=IntPtr.Zero;PI p=new PI();Pump op=null,ep=null;
  try{
   checkpoint("not-launched",r);
   job=CreateJobObject(IntPtr.Zero,null);Need(job!=IntPtr.Zero,"create-job");var ex=new Extended();ex.basic.flags=0x2000;Need(SetInformationJobObject(job,9,ref ex,Marshal.SizeOf(typeof(Extended))),"kill-on-close");
   var sa=new SA{n=Marshal.SizeOf(typeof(SA)),inherit=1};Need(CreatePipe(out or,out ow,ref sa,4096),"stdout-pipe");Need(CreatePipe(out er,out ew,ref sa,4096),"stderr-pipe");Need(SetHandleInformation(or,1,0),"stdout-noninherit");Need(SetHandleInformation(er,1,0),"stderr-noninherit");
   var si=new SI{cb=Marshal.SizeOf(typeof(SI)),flags=0x100,output=ow,error=ew,input=IntPtr.Zero};var cmd=new StringBuilder(Quote(exe));foreach(string arg in args)cmd.Append(" ").Append(Quote(arg));
   r.launch="possibly_launched";checkpoint("launch-intent",r);r.create_attempted=true;
   if(!CreateProcess(exe,cmd,IntPtr.Zero,IntPtr.Zero,true,0x08000004,IntPtr.Zero,cwd,ref si,out p)){r.created=false;r.launch="not_launched";throw new Exception("create:"+Marshal.GetLastWin32Error());}
   Created(r,p.pid);r.resumed=false;checkpoint("created",r);
   long a,b,c,d;Need(GetProcessTimes(p.process,out a,out b,out c,out d),"root-creation");r.creation_filetime=a;
   uint size=32768;var image=new StringBuilder((int)size);Need(QueryFullProcessImageName(p.process,0,image,ref size),"root-image");r.root_image=image.ToString();
   Need(AssignProcessToJobObject(job,p.process),"assign");r.assigned=true;Observe(r,job,"assigned-suspended",sw,known,held);checkpoint("assigned",r);
   Need(CloseHandle(ow),"stdout-write-close");ow=IntPtr.Zero;Need(CloseHandle(ew),"stderr-write-close");ew=IntPtr.Zero;
   op=new Pump(or,so,cap);or=IntPtr.Zero;ep=new Pump(er,se,cap);er=IntPtr.Zero;
   r.resume_attempted=true;Need(ResumeThread(p.thread)!=0xffffffff,"resume");r.resumed=true;checkpoint("resumed",r);
   while(true){var wait=WaitForSingleObject(p.process,0);if(wait==0)break;if(wait!=258)throw new Exception("root-wait");if(sw.Elapsed.TotalSeconds>=total-27)throw new Exception("root-timeout");if(op.overflow!=0||ep.overflow!=0)throw new Exception("stream-cap");Thread.Sleep(25);}
   uint code;Need(GetExitCodeProcess(p.process,out code),"root-exit");Exited(r,code);r.active_at_exit=Active(job);Observe(r,job,"root-exit",sw,known,held);checkpoint("root-exited",r);
   var q=Stopwatch.StartNew();while(Active(job)>0&&q.Elapsed.TotalSeconds<15){Thread.Sleep(25);}
   r.quiescence_s=q.Elapsed.TotalSeconds;r.natural_quiescence=Active(job)==0;r.descendants=r.natural_quiescence==true?"natural_zero":"observed_nonzero";Observe(r,job,"quiescence-end",sw,known,held);checkpoint("quiescence",r);
   if(r.natural_quiescence!=true)throw new Exception("descendants-nonquiescent");
   r.cleanup="not_needed_verified_zero";
  }catch(Exception e){r.reason=e.Message;
   if(job!=IntPtr.Zero&&r.assigned==true){
    Try(r,"cleanup-before",()=>Observe(r,job,"before-owned-cleanup",sw,known,held));
    Try(r,"terminate-owned",()=>Need(TerminateJobObject(job,125),"terminate-owned"));
    Try(r,"cleanup-zero",()=>{var q=Stopwatch.StartNew();while(Active(job)>0&&q.Elapsed.TotalSeconds<5)Thread.Sleep(25);r.active_final=Active(job);Observe(r,job,"after-owned-cleanup",sw,known,held);if(r.active_final!=0)throw new Exception("cleanup-nonzero");r.cleanup="cleanup_verified";});
   }else if(p.process!=IntPtr.Zero){Try(r,"terminate-suspended-root",()=>{Need(TerminateProcess(p.process,125),"terminate-root");if(WaitForSingleObject(p.process,5000)!=0)throw new Exception("root-cleanup-wait");r.cleanup="cleanup_verified";});}
  }finally{
   if(job!=IntPtr.Zero)Try(r,"final-members",()=>{r.active_final=Active(job);Observe(r,job,"final-before-close",sw,known,held);});
   foreach(var member in held){Try(r,"held-member-exit",()=>{if(WaitForSingleObject(member.Value,0)!=0)throw new Exception("held-member-not-exited:"+member.Key);r.held_members_exited.Add(member.Key);});Try(r,"held-member-close",()=>Need(CloseHandle(member.Value),"held-close"));}
   foreach(IntPtr h in new[]{ow,ew})if(h!=IntPtr.Zero)Try(r,"pipe-write-close",()=>Need(CloseHandle(h),"close"));ow=ew=IntPtr.Zero;
   if(op!=null&&ep!=null)Try(r,"stream-drain",()=>{r.streams_completed=Task.WaitAll(new Task[]{op.task,ep.task},Math.Max(0,Math.Min(5000,(int)((total-sw.Elapsed.TotalSeconds)*1000))));if(r.streams_completed!=true)throw new Exception("stream-drain-timeout");});
   if(op!=null){r.stdout_seen=op.seen;r.stdout_written=op.written;r.stdout_overflow=op.overflow!=0;}if(ep!=null){r.stderr_seen=ep.seen;r.stderr_written=ep.written;r.stderr_overflow=ep.overflow!=0;}
   if(job!=IntPtr.Zero)Try(r,"job-close",()=>Need(CloseHandle(job),"close"));
   foreach(IntPtr h in new[]{p.thread,p.process,or,er})if(h!=IntPtr.Zero)Try(r,"handle-close",()=>Need(CloseHandle(h),"close"));
   r.elapsed_s=sw.Elapsed.TotalSeconds;
   r.runner_exit=r.reason!=""||r.errors.Count>0||r.errors_overflow||r.cleanup=="unknown"||r.streams_completed!=true||r.stdout_overflow||r.stderr_overflow?125:r.exit_code.HasValue?(int)r.exit_code.Value:126;
   Try(r,"final-checkpoint",()=>checkpoint("final",r));
  }
 }
}
