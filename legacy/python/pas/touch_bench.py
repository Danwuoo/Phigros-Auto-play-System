"""Isolated Android touch Fixture benchmark; fixed sequences never target a game."""

from dataclasses import asdict
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import threading
import time
import uuid

from .action_planner import trajectory
from .adb import find_adb, probe
from .capture_grpc import GrpcEndpoint, discover_endpoint, rgb24_from_image
from .contact_scheduler import SchedulerOwner
from .input_grpc import EmulatorGrpcTouch
from .input import PixelCoordinateMap
from .telemetry import distribution


NAMES = ("magic", "sequence", "down", "up", "move", "cancel", "max_concurrent",
         "active", "last_x", "last_y", "last_pointer_id", "pairs", "moves_with_other", "last_action")


class FixtureReader:
    def __init__(self, serial: str, endpoint: GrpcEndpoint, width: int, height: int):
        import grpc
        from .emulator_proto import emulator_controller_pb2 as pb
        self.width, self.height = width, height
        self.endpoint = endpoint
        self.channel = grpc.insecure_channel(endpoint.target, options=[("grpc.max_receive_message_length",80*1024*1024)])
        self.request = pb.ImageFormat(format=pb.ImageFormat.RGB888, width=width, height=height)
        self.rpc = self.channel.unary_unary(
            "/android.emulation.control.EmulatorController/getScreenshot",
            request_serializer=lambda message: message.SerializeToString(),
            response_deserializer=pb.Image.FromString)

    def read(self):
        image = self.rpc(self.request, timeout=2,
                         metadata=(("authorization", f"Bearer {self.endpoint.token}"),))
        width, height, rgb = rgb24_from_image(image, self.request.format, "top-down")
        observed_ns = time.monotonic_ns()
        if (width, height) != (self.width, self.height):
            raise RuntimeError("Fixture display shape changed")
        values = {}
        for i, name in enumerate(NAMES):
            x, y = 24+i*30, 112
            pos = (y*width+x)*3
            values[name] = int.from_bytes(rgb[pos:pos+3], "big")
        if values["magic"] != 0x504153:
            raise RuntimeError("touch Fixture pixel signature absent")
        return observed_ns, values, (width, height, rgb)

    def close(self):
        self.channel.close()


def _foreground(serial: str):
    adb = find_adb()
    if not adb:
        raise RuntimeError("ADB unavailable for Fixture foreground guard")
    result = subprocess.run([adb,"-s",serial,"shell","dumpsys","window"],
                            capture_output=True, text=True, timeout=5, check=True)
    focus = next((line for line in result.stdout.splitlines() if "mCurrentFocus=" in line), "")
    if "org.pas.touchfixture/org.pas.touchfixture.MainActivity" not in focus:
        raise RuntimeError(f"touch Fixture is not focused: {focus.strip()}")


def _points(kind: str, t: int, index: int, width: int, height: int):
    grid = ((.12,.29),(.5,.29),(.88,.29),(.12,.58),(.5,.58),(.88,.58),
            (.12,.87),(.5,.87),(.88,.87))
    u,v = grid[index % len(grid)]
    x,y = round(width*u), round(height*v)
    if kind == "tap":
        return (((t,x,y),(t+40_000_000,x,y)),)
    if kind == "hold":
        return (((t,x,y),(t+250_000_000,x,y)),)
    if kind == "move":
        return (((t,x,y),(t+70_000_000,x+35,y),(t+140_000_000,x+70,y+20),
                 (t+230_000_000,x+70,y+20)),)
    if kind == "flick":
        return (((t,x,y),(t+35_000_000,x+45,y),(t+70_000_000,x+90,y),
                 (t+105_000_000,x+45,y),(t+150_000_000,x,y),
                 (t+210_000_000,x,y)),)
    if kind == "pair":
        return (((t,x,y),(t+110_000_000,x+35,y),(t+270_000_000,x+35,y)),
                ((t+50_000_000,min(width-30,x+140),min(height-25,y+30)),
                 (t+190_000_000,min(width-30,x+140),min(height-25,y+30))))
    if kind == "simultaneous":
        return (((t,x,y),(t+180_000_000,x,y)),
                ((t,min(width-30,x+140),min(height-25,y+30)),
                 (t+180_000_000,min(width-30,x+140),min(height-25,y+30))))
    if kind == "edge":
        corners=((0,0),(width-1,0),(0,height-1),(width-1,height-1))
        ex,ey=corners[index%4]
        return (((t,ex,ey),(t+80_000_000,ex,ey)),)
    raise ValueError(kind)


def run_touch_bench(config, *, repetitions=30, kinds=("tap","hold","move","flick","pair","simultaneous"),
                    output_dir=None):
    if config.touch_kind != "emulator-grpc" or config.capture_kind != "emulator-grpc":
        raise ValueError("touch bench requires real gRPC Fixture profile")
    if repetitions < 1 or repetitions > 1000 or set(kinds) - {"tap","hold","move","flick","pair","simultaneous","edge"}:
        raise ValueError("invalid benchmark cases")
    _foreground(config.serial)
    device_report=probe(find_adb(),config.serial)
    endpoint = (GrpcEndpoint(config.endpoint, Path(config.token_file).read_text(encoding="utf-8").strip(),"explicit")
                if config.endpoint else discover_endpoint(config.serial))
    reader = FixtureReader(config.serial, endpoint, config.width, config.height)
    output = Path(output_dir or config.log_dir) / ("touch-"+time.strftime("%Y%m%d-%H%M%S")+"-"+uuid.uuid4().hex[:8])
    output.mkdir(parents=True, exist_ok=False)
    events = []
    event_lock = threading.Lock()
    def on_event(event, **fields):
        with event_lock:
            events.append({"event":event,**fields})
    owner = SchedulerOwner(
        lambda: EmulatorGrpcTouch(config.serial,config.touch_width,config.touch_height,endpoint=endpoint,
                                  timeout_s=config.touch_timeout_ms/1000,max_contacts=config.max_contacts,
                                  coordinate_map=PixelCoordinateMap(config.width,config.height,
                                                                    config.touch_width,config.touch_height,
                                                                    config.touch_rotation_deg)),
        max_messages=config.max_plans,
        scheduler_kwargs={"max_contacts":config.max_contacts,"max_plans":config.max_plans,
                          "max_steps":config.max_steps,"horizon_ns":round(config.horizon_ms*1e6),
                          "evidence_max_age_ns":2_000_000_000,"on_event":on_event})
    cases = []
    cancel_cases = []
    try:
        owner.start()
        reader.read()
        epoch = 1
        for kind in kinds:
            for i in range(repetitions):
                _foreground(config.serial)
                baseline_ns,before,_ = reader.read()
                if before["active"] != 0:
                    raise RuntimeError("Fixture reports a residual active contact")
                owner.call("set_gate",epoch,True,baseline_ns)
                t = time.monotonic_ns()+80_000_000
                paths = _points(kind,t,i,config.width,config.height)
                for j,points in enumerate(paths):
                    plan = trajectory(f"{kind}-{i}-{j}",1,epoch,baseline_ns,points,
                                      validity_ns=60_000_000,basis="isolated_touch_fixture_fixed_sequence")
                    if not owner.call("submit",plan):
                        raise RuntimeError("Fixture plan rejected")
                expected_down = 2 if kind in ("pair","simultaneous") else 1
                expected_up = expected_down
                first_effect_ns = None
                after = before
                max_active_seen = 0
                deadline = time.monotonic()+2.5
                while time.monotonic() < deadline:
                    observed_ns,after,snapshot = reader.read()
                    max_active_seen = max(max_active_seen,after["active"])
                    if kind in ("pair","simultaneous") and i == 0 and after["active"] >= 2:
                        evidence_path = output/f"{kind}-two-pointers.png"
                        if not evidence_path.exists():
                            from .cli import _write_rgb_png
                            from .contracts import Frame
                            _write_rgb_png(str(evidence_path),Frame(0,snapshot[0],snapshot[1],snapshot[2],observed_ns))
                    if after["sequence"] > before["sequence"] and first_effect_ns is None:
                        first_effect_ns = observed_ns
                    if after["down"]-before["down"] >= expected_down and after["up"]-before["up"] >= expected_up and after["active"] == 0:
                        break
                    time.sleep(.01)
                with event_lock:
                    receipts = [e for e in events if e["event"]=="touch_receipt" and
                                e.get("key","").startswith(f"{kind}-{i}-")]
                rpc_ok = len(receipts)==sum(len(p) for p in paths) and all(e["success"] for e in receipts)
                final_path = paths[-1] if kind == "simultaneous" else paths[0]
                coordinate_error = (abs(after["last_x"]-final_path[-1][1])+
                                    abs(after["last_y"]-final_path[-1][2]))
                visible_ok = (after["down"]-before["down"]==expected_down and
                              after["up"]-before["up"]==expected_up and after["active"]==0 and
                              coordinate_error <= 3 and
                              after["cancel"]==before["cancel"] and
                              (kind not in ("pair","simultaneous") or
                               after["pairs"]>before["pairs"] and max_active_seen>=2) and
                              (kind != "pair" or after["moves_with_other"]>before["moves_with_other"]) and
                              (kind != "hold" or max_active_seen>=1) and
                              (kind != "move" or after["move"]-before["move"]>=2) and
                              (kind != "flick" or after["move"]-before["move"]>=3))
                starts = [e["injection_start_ns"] for e in receipts if e["phase"]=="down"]
                durations = [(e["injection_return_ns"]-e["injection_start_ns"])/1e6 for e in receipts]
                schedule_errors = [(e["injection_start_ns"]-e["scheduled_ns"])/1e6 for e in receipts]
                cases.append({"kind":kind,"index":i,"rpc_ok":rpc_ok,"visible_ok":visible_ok,
                              "before":before,"after":after,"max_active_seen":max_active_seen,
                              "effect_first_observed_ns":first_effect_ns,
                              "effect_visible_latency_ms":((first_effect_ns-min(starts))/1e6 if first_effect_ns and starts else None),
                              "rpc_call_ms":durations,"schedule_error_ms":schedule_errors,
                              "coordinate_error_px":coordinate_error})
                if not rpc_ok or not visible_ok:
                    # No further capability claim after unexplained contact state.
                    raise RuntimeError(f"Fixture evidence failed: {kind} sample {i}")
        # An armed contact is revoked before its scheduled up. The real
        # Fixture must show the release; a returned RPC is insufficient.
        for i in range(repetitions):
            _foreground(config.serial)
            baseline_ns,before,_=reader.read()
            if before["active"]:
                raise RuntimeError("residual contact before cancellation")
            epoch+=1
            owner.call("set_gate",epoch,True,baseline_ns)
            t=time.monotonic_ns()+60_000_000
            x,y=round(config.width*.5),round(config.height*.55)
            plan=trajectory(f"cancel-{i}",1,epoch,baseline_ns,
                            ((t,x,y),(t+1_000_000_000,x,y)),
                            validity_ns=60_000_000,basis="isolated_fixture_cancel")
            if not owner.call("submit",plan):
                raise RuntimeError("cancellation plan rejected")
            saw_active=False
            deadline=time.monotonic()+.8
            while time.monotonic()<deadline:
                _,middle,_=reader.read()
                if middle["active"]==1 and middle["down"]>before["down"]:
                    saw_active=True
                    break
                time.sleep(.01)
            owner.call("set_gate",epoch,False)
            deadline=time.monotonic()+1
            after=middle
            while time.monotonic()<deadline:
                _,after,_=reader.read()
                if after["active"]==0 and after["up"]>before["up"]:
                    break
                time.sleep(.01)
            passed=(saw_active and after["down"]-before["down"]==1 and
                    after["up"]-before["up"]==1 and after["active"]==0)
            cancel_cases.append({"index":i,"visible_ok":passed,"before":before,"after":after})
            if not passed:
                raise RuntimeError(f"Fixture cancellation evidence failed: {i}")
        _,final,frame=reader.read()
        from .cli import _write_rgb_png
        from .contracts import Frame
        _write_rgb_png(str(output/"final.png"),Frame(0,frame[0],frame[1],frame[2],time.monotonic_ns()))
    except Exception as error:
        failure = str(error)
    else:
        failure = None
    finally:
        stop_error = owner.stop()
        reader.close()
        with (output/"events.jsonl").open("w",encoding="utf-8") as stream:
            for event in events:
                stream.write(json.dumps(event)+"\n")
        with (output/"cases.jsonl").open("w",encoding="utf-8") as stream:
            for case in cases:
                stream.write(json.dumps(case)+"\n")
        with (output/"cancel_cases.jsonl").open("w",encoding="utf-8") as stream:
            for case in cancel_cases:
                stream.write(json.dumps(case)+"\n")
    by_kind = {}
    for kind in kinds:
        selected=[c for c in cases if c["kind"]==kind]
        by_kind[kind]={"n":len(selected),"rpc_failures":sum(not c["rpc_ok"] for c in selected),
                       "visible_failures":sum(not c["visible_ok"] for c in selected),
                       "schedule_error_ms":distribution([v for c in selected for v in c["schedule_error_ms"]]),
                       "rpc_call_ms":distribution([v for c in selected for v in c["rpc_call_ms"]]),
                       "effect_visible_latency_ms":distribution([c["effect_visible_latency_ms"] for c in selected if c["effect_visible_latency_ms"] is not None]),
                       "coordinate_error_px":distribution([c["coordinate_error_px"] for c in selected])}
    from .runtime import _revision, display_refresh_hz
    apk = Path("measurements/touch_fixture_android/pas-touch-fixture.apk")
    summary = {"environment":{"serial":config.serial,"display":[config.width,config.height],
                               "touch_display":[config.touch_width,config.touch_height],
                               "rotation_deg":config.touch_rotation_deg,
                               "android_release":device_report["android_release"],
                               "android_sdk":device_report["android_sdk"],
                               "model":device_report["model"],
                               "wm_size":device_report["wm_size"],
                               "wm_density":device_report["wm_density"],
                               "display_refresh_hz_setting":display_refresh_hz(device_report["display_info"]),
                               "host_os":platform.platform(),"python":platform.python_version(),
                               "host_timer_resolution_ms":owner.timer_resolution_ms,
                               "git":_revision(),
                               "config_sha256":hashlib.sha256(json.dumps(config.public(),sort_keys=True).encode()).hexdigest(),
                               "fixture_apk_sha256":hashlib.sha256(apk.read_bytes()).hexdigest() if apk.exists() else None,
                               "capture":"gRPC getScreenshot RGB888 top-down",
                               "touch":"gRPC sendTouch","rpc_timeout_ms":config.touch_timeout_ms},
               "requested_per_kind":repetitions,"kinds":list(kinds),"completed_cases":len(cases),
               "rpc_failures":sum(not c["rpc_ok"] for c in cases),
               "visible_failures":sum(not c["visible_ok"] for c in cases),
               "schedule_error_ms":distribution([v for c in cases for v in c["schedule_error_ms"]]),
               "rpc_call_ms":distribution([v for c in cases for v in c["rpc_call_ms"]]),
               "effect_visible_latency_ms":distribution([c["effect_visible_latency_ms"] for c in cases if c["effect_visible_latency_ms"] is not None]),
               "coordinate_error_px":distribution([c["coordinate_error_px"] for c in cases if c["coordinate_error_px"] is not None]),
               "failure":failure,"stop_error":stop_error,"evidence_dir":str(output),
               "by_kind":by_kind,
               "cancel":{"n":len(cancel_cases),"visible_failures":sum(not c["visible_ok"] for c in cancel_cases),
                         "fixture_verified":failure is None and stop_error is None and len(cancel_cases)>=30 and
                                             all(c["visible_ok"] for c in cancel_cases)},
               "fixture_verified":{kind:failure is None and stop_error is None and repetitions>=30 and
                                   by_kind[kind]["n"]>=30 and by_kind[kind]["visible_failures"]==0
                                   and by_kind[kind]["rpc_failures"]==0 for kind in kinds},
               "gameplay_enabled":False,
               "limitations":["Android pointer IDs are independent of Emulator Touch identifiers",
                              "effect latency is first screenshot observation, not actual Android dispatch time",
                              "p99 is descriptive and weak at 30 samples; other rotations need separate validation",
                              "display corners are a separate four-sample smoke, not a 30-sample capability"]}
    (output/"summary.json").write_text(json.dumps(summary,indent=2),encoding="utf-8")
    from .capability import CapabilityReport
    CapabilityReport.from_touch_summary(summary).save(output/"capability.json")
    return summary


def run_batch_bench(config, *, repetitions=30, output_dir=None):
    """One gRPC TouchEvent containing two pointers, checked in Android pixels."""
    if config.touch_kind != "emulator-grpc" or config.max_contacts < 2 or repetitions < 1:
        raise ValueError("batch bench requires gRPC Fixture profile and samples")
    _foreground(config.serial)
    device_report=probe(find_adb(),config.serial)
    endpoint=(GrpcEndpoint(config.endpoint,Path(config.token_file).read_text().strip(),"explicit")
              if config.endpoint else discover_endpoint(config.serial))
    reader=FixtureReader(config.serial,endpoint,config.width,config.height)
    mapping=PixelCoordinateMap(config.width,config.height,config.touch_width,
                               config.touch_height,config.touch_rotation_deg)
    touch=EmulatorGrpcTouch(config.serial,config.touch_width,config.touch_height,
                            endpoint=endpoint,max_contacts=2,
                            timeout_s=config.touch_timeout_ms/1000,coordinate_map=mapping)
    output=Path(output_dir or config.log_dir)/("batch-"+time.strftime("%Y%m%d-%H%M%S")+"-"+uuid.uuid4().hex[:8])
    output.mkdir(parents=True,exist_ok=False)
    from .contracts import TouchCommand, Frame
    from .cli import _write_rgb_png
    cases=[]
    failure=None
    try:
        for i in range(repetitions):
            _foreground(config.serial)
            _,before,_=reader.read()
            if before["active"]:
                raise RuntimeError("residual contact before batch")
            x,y=round(config.width*.3),round(config.height*.45)
            bx,by=round(config.width*.7),round(config.height*.55)
            def command(id,phase,cx,cy):
                return TouchCommand(f"batch-{i}",id,phase,cx,cy,time.monotonic_ns(),-1)
            receipts=[]
            receipts.append(touch.inject_batch((command(0,"down",x,y),command(1,"down",bx,by))))
            first_ns=None
            max_active=0
            deadline=time.monotonic()+1.5
            while time.monotonic()<deadline:
                observed,middle,frame=reader.read()
                max_active=max(max_active,middle["active"])
                if middle["down"]>before["down"] and first_ns is None:
                    first_ns=observed
                if middle["active"]==2:
                    if i==0:
                        _write_rgb_png(str(output/"two-pointers.png"),Frame(0,frame[0],frame[1],frame[2],observed))
                    break
                time.sleep(.01)
            if max_active!=2:
                raise RuntimeError(f"batch down invisible at sample {i}")
            receipts.append(touch.inject_batch((command(0,"move",x+40,y),command(1,"move",bx,by))))
            deadline=time.monotonic()+1
            while time.monotonic()<deadline:
                _,middle,_=reader.read()
                if middle["moves_with_other"]>before["moves_with_other"]:
                    break
                time.sleep(.01)
            receipts.append(touch.inject_batch((command(0,"up",x+40,y),command(1,"up",bx,by))))
            deadline=time.monotonic()+1.5
            while time.monotonic()<deadline:
                _,after,_=reader.read()
                if after["up"]-before["up"]==2 and after["active"]==0:
                    break
                time.sleep(.01)
            rpc_ok=all(receipt.success for batch in receipts for receipt in batch)
            visible_ok=(after["down"]-before["down"]==2 and after["up"]-before["up"]==2
                        and after["active"]==0 and after["pairs"]>before["pairs"]
                        and after["moves_with_other"]>before["moves_with_other"]
                        and after["cancel"]==before["cancel"])
            cases.append({"index":i,"rpc_ok":rpc_ok,"visible_ok":visible_ok,
                          "max_active_seen":max_active,"before":before,"after":after,
                          "rpc_call_ms":[(batch[0].injection_return_ns-batch[0].injection_start_ns)/1e6 for batch in receipts],
                          "effect_visible_latency_ms":(first_ns-receipts[0][0].injection_start_ns)/1e6 if first_ns else None})
            if not rpc_ok or not visible_ok:
                raise RuntimeError(f"batch Fixture evidence failed at sample {i}")
    except Exception as error:
        failure=str(error)
    finally:
        release=touch.release_all()
        try: touch.close()
        except Exception as error: failure=f"{failure}; cleanup:{error}"
        reader.close()
        with (output/"cases.jsonl").open("w",encoding="utf-8") as stream:
            for case in cases: stream.write(json.dumps(case)+"\n")
    from .runtime import display_refresh_hz, _revision
    summary={"kind":"two_pointer_grpc_batch","n":len(cases),"requested":repetitions,
             "environment":{"serial":config.serial,"display":[config.width,config.height],
                            "touch_display":[config.touch_width,config.touch_height],
                            "rotation_deg":config.touch_rotation_deg,"rpc_timeout_ms":config.touch_timeout_ms,
                            "android_release":device_report["android_release"],
                            "model":device_report["model"],
                            "wm_density":device_report["wm_density"],
                            "display_refresh_hz_setting":display_refresh_hz(device_report["display_info"]),
                            "host_os":platform.platform(),"python":platform.python_version(),
                            "git":_revision()},
             "rpc_failures":sum(not c["rpc_ok"] for c in cases),
             "visible_failures":sum(not c["visible_ok"] for c in cases),
             "rpc_call_ms":distribution([v for c in cases for v in c["rpc_call_ms"]]),
             "effect_visible_latency_ms":distribution([c["effect_visible_latency_ms"] for c in cases if c["effect_visible_latency_ms"] is not None]),
             "release_failed_ids":release.failed_ids,"release_effect_unverified_ids":release.effect_unverified_ids,
             "fixture_verified":failure is None and len(cases)>=30 and not release.failed_ids and not release.unknown_ids,
             "failure":failure,"evidence_dir":str(output)}
    (output/"summary.json").write_text(json.dumps(summary,indent=2),encoding="utf-8")
    return summary


def run_disconnect_recovery_smoke(config, *, output_dir=None):
    """Break an input channel with a visible contact, then rescue all IDs."""
    _foreground(config.serial)
    endpoint=(GrpcEndpoint(config.endpoint,Path(config.token_file).read_text().strip(),"explicit")
              if config.endpoint else discover_endpoint(config.serial))
    reader=FixtureReader(config.serial,endpoint,config.width,config.height)
    mapping=PixelCoordinateMap(config.width,config.height,config.touch_width,
                               config.touch_height,config.touch_rotation_deg)
    output=Path(output_dir or config.log_dir)/("disconnect-"+time.strftime("%Y%m%d-%H%M%S")+"-"+uuid.uuid4().hex[:8])
    output.mkdir(parents=True,exist_ok=False)
    from .contracts import TouchCommand
    touch=EmulatorGrpcTouch(config.serial,config.touch_width,config.touch_height,
                            endpoint=endpoint,max_contacts=config.max_contacts,
                            timeout_s=config.touch_timeout_ms/1000,coordinate_map=mapping)
    failure=None
    rpc_failed=False
    rpc_failure_reason=None
    rescue_reports=[]
    initial_release=None
    before=after=None
    try:
        _,before,_=reader.read()
        if before["active"]: raise RuntimeError("residual contact before timeout test")
        x,y=round(config.width*.5),round(config.height*.5)
        down=touch.inject(TouchCommand("timeout-smoke",0,"down",x,y,time.monotonic_ns(),-1))
        if not down.success: raise RuntimeError("setup down RPC failed")
        deadline=time.monotonic()+1
        while time.monotonic()<deadline:
            _,middle,_=reader.read()
            if middle["active"]==1: break
            time.sleep(.01)
        if middle["active"]!=1: raise RuntimeError("setup contact not visible")
        # A closed transport gives a deterministic uncertain RPC result while
        # the Android contact is known to be visibly down.
        touch._channel.close()
        uncertain=touch.inject(TouchCommand("disconnect-smoke",0,"up",x,y,time.monotonic_ns(),-1))
        rpc_failed=not uncertain.success
        rpc_failure_reason=uncertain.reason
        initial_release=touch.release_all()
        try: touch.close()
        except RuntimeError: pass  # Channel is closed; unresolved IDs are reported.
        touch=None
        rescue=EmulatorGrpcTouch(config.serial,config.touch_width,config.touch_height,
                                 endpoint=endpoint,max_contacts=config.max_contacts,
                                 timeout_s=config.touch_timeout_ms/1000)
        for _ in range(2):
            rescue_reports.append(rescue.emergency_release_all())
            time.sleep(.1)
        rescue.close()
        stable_zero=True
        for _ in range(5):
            _,after,_=reader.read()
            stable_zero &= after["active"]==0
            time.sleep(.1)
        if not rpc_failed or not stable_zero or any(r.failed_ids for r in rescue_reports):
            raise RuntimeError("disconnect or visible recovery was not established")
    except Exception as error:
        failure=str(error)
    finally:
        if touch is not None:
            try:
                touch.timeout_s=config.touch_timeout_ms/1000
                touch.release_all()
                touch.close()
            except Exception as error:
                failure=f"{failure}; cleanup:{error}"
        reader.close()
    summary={"kind":"fixture_disconnect_recovery_smoke","n":1,"rpc_failure_triggered":rpc_failed,
             "rpc_failure_reason":rpc_failure_reason,
             "before":before,"after":after,"initial_release":asdict(initial_release) if initial_release else None,
             "rescue_reports":[asdict(r) for r in rescue_reports],
             "visible_zero_after_recovery":after is not None and after["active"]==0,
             "failure":failure,"evidence_dir":str(output),
             "limitation":"real transport disconnect tested; real RPC deadline timeout was not reproduced; recovery observed for 0.5 seconds"}
    (output/"summary.json").write_text(json.dumps(summary,indent=2),encoding="utf-8")
    return summary
