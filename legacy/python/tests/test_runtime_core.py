import json
from pathlib import Path
import tempfile
import threading
import time
import unittest

from pas.action_planner import tap, trajectory
from pas.capability import CapabilityReport
from pas.config import RuntimeConfig
from pas.contact_scheduler import ContactScheduler, SchedulerOwner
from pas.contracts import Frame, TouchReceipt
from pas.input import PixelCoordinateMap
from pas.input_grpc import EmulatorGrpcTouch
from pas.runtime import validate_frame_geometry


class Clock:
    def __init__(self): self.value = 1_000_000_000
    def __call__(self): return self.value
    def advance(self, ns): self.value += ns


class Backend:
    def __init__(self, clock, fail_phase=None, release_fails=False):
        self.clock, self.fail_phase, self.release_fails = clock, fail_phase, release_fails
        self.calls = []
        self.contacts = set()
        self.releases = 0

    def inject(self, command):
        self.calls.append(command)
        if command.phase == self.fail_phase:
            return TouchReceipt(command,self.clock(),self.clock(),False,"simulated_timeout")
        if command.phase == "down": self.contacts.add(command.contact_id)
        if command.phase == "up": self.contacts.remove(command.contact_id)
        return TouchReceipt(command,self.clock(),self.clock(),True)

    def release_all(self):
        self.releases += 1
        if self.release_fails: raise RuntimeError("release unavailable")
        self.contacts.clear()


class SchedulerTests(unittest.TestCase):
    def setUp(self):
        self.clock=Clock()
        self.backend=Backend(self.clock)
        self.events=[]
        self.scheduler=ContactScheduler(self.backend,clock=self.clock,max_contacts=2,
            max_plans=2,max_steps=8,evidence_max_age_ns=200_000_000,
            on_event=lambda event,**fields:self.events.append((event,fields)))
        self.scheduler.set_gate(1,True,self.clock())

    def test_replace_before_down_and_bounded_revisions(self):
        for i in range(500):
            plan=tap("same",i+1,1,self.clock(),self.clock()+50_000_000,10+i%5,20)
            self.assertTrue(self.scheduler.submit(plan))
        self.assertEqual(self.scheduler.pending_count,1)
        self.clock.advance(50_000_000)
        self.scheduler.run_due()
        self.assertEqual(self.backend.calls[0].x,14)
        self.clock.advance(20_000_000)
        self.scheduler.run_due()
        self.assertFalse(self.backend.contacts)

    def test_hold_move_revision_preserves_up(self):
        t=self.clock()+10_000_000
        first=trajectory("hold",1,1,self.clock(),((t,20,30),(t+40_000_000,25,30),
                   (t+100_000_000,25,30)),basis="pixel_motion")
        self.assertTrue(self.scheduler.submit(first))
        self.clock.advance(10_000_000); self.scheduler.run_due()
        replacement=trajectory("hold",2,1,self.clock(),((self.clock()+10_000_000,20,30),
                    (self.clock()+30_000_000,35,30),(self.clock()+60_000_000,35,30)),basis="updated_pixels")
        self.assertTrue(self.scheduler.submit(replacement))
        self.clock.advance(60_000_000); self.scheduler.run_due()
        self.assertEqual([c.phase for c in self.backend.calls],["down","move","up"])
        self.assertFalse(self.backend.contacts)

    def test_epoch_expiry_and_cancel(self):
        t=self.clock()+20_000_000
        self.assertTrue(self.scheduler.submit(tap("a",1,1,self.clock(),t,20,20)))
        self.scheduler.set_gate(2,False)
        self.clock.advance(40_000_000)
        self.assertEqual(self.scheduler.run_due(),[])
        self.assertFalse(self.scheduler.submit(tap("old",1,1,self.clock(),self.clock()+10_000_000,20,20)))
        self.scheduler.set_gate(2,True,self.clock())
        self.assertTrue(self.scheduler.submit(tap("late",1,2,self.clock(),self.clock()+10_000_000,20,20,
                                                  validity_ns=0)))
        self.clock.advance(30_000_000)
        self.scheduler.run_due()
        self.assertEqual(self.scheduler.fault,"stale_expired_or_late_step")
        self.assertFalse(self.backend.contacts)

    def test_capacity_fault_and_release_failure(self):
        t=self.clock()+10_000_000
        self.assertTrue(self.scheduler.submit(tap("a",1,1,self.clock(),t,20,20)))
        self.assertTrue(self.scheduler.submit(tap("b",1,1,self.clock(),t,30,20)))
        self.assertFalse(self.scheduler.submit(tap("c",1,1,self.clock(),t,40,20)))
        self.assertEqual(self.scheduler.fault,"scheduler_capacity")
        self.assertEqual(self.scheduler.pending_count,0)
        second=ContactScheduler(Backend(self.clock,release_fails=True),clock=self.clock)
        second.set_gate(1,False)
        self.assertTrue(second.fault.startswith("release_failed"))

    def test_timeout_does_not_continue_move(self):
        backend=Backend(self.clock,fail_phase="down")
        scheduler=ContactScheduler(backend,clock=self.clock)
        scheduler.set_gate(1,True,self.clock())
        t=self.clock()+10_000_000
        scheduler.submit(trajectory("flick",1,1,self.clock(),((t,20,20),(t+10_000_000,40,20),
                         (t+20_000_000,70,20),(t+30_000_000,20,20)),basis="fixture"))
        self.clock.advance(40_000_000); scheduler.run_due()
        self.assertEqual([c.phase for c in backend.calls],["down"])
        self.assertEqual(scheduler.fault,"rpc_result_unknown")
        self.assertGreaterEqual(backend.releases,1)

    def test_owner_stop_waits_for_inflight_down_then_releases(self):
        started=threading.Event()
        class SlowBackend:
            def __init__(self): self.contacts=set(); self.closed=False
            @property
            def active_ids(self): return tuple(self.contacts)
            def inject(self,command):
                if command.phase=="down":
                    started.set()
                    time.sleep(.03)
                    self.contacts.add(command.contact_id)
                else:
                    self.contacts.discard(command.contact_id)
                now=time.monotonic_ns()
                return TouchReceipt(command,now,now,True)
            def release_all(self): self.contacts.clear()
            def close(self): self.closed=True
        backend=SlowBackend()
        owner=SchedulerOwner(lambda:backend)
        owner.start()
        now=time.monotonic_ns()
        owner.call("set_gate",1,True,now)
        owner.call("submit",tap("slow",1,1,now,now+20_000_000,20,20,duration_ns=1_000_000_000))
        self.assertTrue(started.wait(1))
        self.assertIsNone(owner.stop())
        self.assertFalse(backend.contacts)
        self.assertTrue(backend.closed)


class ConfigAndGrpcTests(unittest.TestCase):
    def require_proto(self):
        try:
            import google.protobuf  # noqa: F401
        except ImportError:
            self.skipTest("optional emulator-grpc protobuf unavailable")

    def test_profile_rejects_unknown_nan_and_mmap(self):
        raw=json.loads(Path("configs/fake-observe.json").read_text())
        with tempfile.TemporaryDirectory() as temporary:
            path=Path(temporary)/"profile.json"
            for mutate in (lambda c:c.update(unknown=1),
                           lambda c:c.pop("scheduler"),
                           lambda c:c["capture"].update(transport="mmap"),
                           lambda c:c["capture"].pop("source_rotation"),
                           lambda c:c["capture"].update(source_rotation=True),
                           lambda c:c["scheduler"].update(horizon_ms=float("nan"))):
                copy=json.loads(json.dumps(raw)); mutate(copy)
                path.write_text(json.dumps(copy))
                with self.assertRaises(ValueError): RuntimeConfig.load(path)

    def test_profile_rejects_same_size_with_wrong_source_rotation(self):
        config=RuntimeConfig.load("configs/avd-observe.json")
        frame=Frame(0,1280,720,bytes(1280*720*3),1,source_rotation=3)
        with self.assertRaisesRegex(RuntimeError,"new epoch/profile required"):
            validate_frame_geometry(frame,config)

    def test_capability_fingerprint_requires_same_mapping(self):
        config=RuntimeConfig.load("configs/avd-fixture.json")
        report=CapabilityReport(1,config.serial,(config.width,config.height),
                                (config.touch_width,config.touch_height),config.touch_rotation_deg,
                                "emulator-grpc",None,("tap",),{"tap":30},{"tap":0},"evidence")
        self.assertTrue(report.matches(config))
        from dataclasses import replace
        self.assertFalse(replace(report,rotation_deg=0).matches(config))
        self.assertFalse(replace(report,serial="emulator-5556").matches(config))

    def test_rotated_inclusive_pixel_endpoints(self):
        mapping=PixelCoordinateMap(1280,720,720,1280,90)
        self.assertEqual(mapping.map(0,0),(719,0))
        self.assertEqual(mapping.map(1279,719),(0,1279))

    def test_rpc_timeout_marks_uncertain_and_reports_release_failure(self):
        self.require_proto()
        calls=[]
        def rpc(event,**kwargs):
            calls.append(event)
            raise TimeoutError("simulated")
        backend=EmulatorGrpcTouch("test",720,1280,rpc=rpc,max_contacts=2)
        from pas.contracts import TouchCommand
        command=TouchCommand("a",0,"down",100,100,1,0)
        receipt=backend.inject(command)
        self.assertFalse(receipt.success)
        self.assertEqual(backend.active_ids,(0,))
        report=backend.release_all()
        self.assertEqual(report.failed_ids,(0,))
        self.assertEqual(report.unknown_ids,(0,))
        self.assertEqual(len(calls),2)

    def test_failed_batch_marks_every_identifier_uncertain(self):
        self.require_proto()
        def rpc(event,**kwargs): raise TimeoutError("batch outcome unknown")
        backend=EmulatorGrpcTouch("test",720,1280,rpc=rpc,max_contacts=2)
        from pas.contracts import TouchCommand
        batch=(TouchCommand("a",0,"down",10,10,1,0),
               TouchCommand("b",1,"down",20,20,1,0))
        receipts=backend.inject_batch(batch)
        self.assertEqual(len(receipts),2)
        self.assertTrue(all(not item.success for item in receipts))
        self.assertEqual(backend.active_ids,(0,1))
        report=backend.release_all()
        self.assertEqual(report.failed_ids,(0,1))
        self.assertEqual(report.unknown_ids,(0,1))
        with self.assertRaises(RuntimeError): backend.close()


if __name__ == "__main__": unittest.main()
