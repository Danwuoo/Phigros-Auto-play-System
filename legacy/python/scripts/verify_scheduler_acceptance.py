"""Deterministic acceptance regressions; fake clock/backend, never emulator input.

Exit 1 means at least one scheduler contract is still violated. Results are
intended to turn green when the implementation is corrected.
"""
import argparse
import json
from pathlib import Path

from pas.action_planner import tap, trajectory
from pas.contact_scheduler import ContactScheduler
from pas.contracts import TouchReceipt


class Clock:
    value = 1_000_000_000
    def __call__(self): return self.value


class Backend:
    def __init__(self, clock):
        self.clock, self.contacts, self.commands = clock, set(), []
    def inject(self, command):
        self.commands.append(command)
        if command.phase == "down": self.contacts.add(command.contact_id)
        elif command.phase == "up": self.contacts.discard(command.contact_id)
        return TouchReceipt(command, self.clock(), self.clock(), True)
    def release_all(self): self.contacts.clear()


def make():
    clock=Clock()
    backend=Backend(clock)
    scheduler=ContactScheduler(backend,clock=clock,evidence_max_age_ns=150_000_000)
    scheduler.set_gate(1,True,clock())
    return clock,backend,scheduler


def verify():
    results=[]
    clock,backend,scheduler=make()
    start=clock()
    accepted=scheduler.submit(trajectory("hold",1,1,start,
        ((start+10_000_000,10,10),(start+1_000_000_000,10,10)),basis="pixels"))
    clock.value=start+10_000_000
    scheduler.run_due()
    clock.value=start+300_000_000
    scheduler.run_due()
    results.append({"case":"release_hold_on_evidence_expiry","passed":not backend.contacts,
        "accepted":accepted,"evidence_age_ms":300,"limit_ms":150,
        "active_ids":list(backend.contacts),"armed":scheduler.armed,"fault":scheduler.fault,
        "expected":"No contact remains held after freshness deadline, even with no due command"})

    clock,backend,scheduler=make()
    start=clock()
    accepted=scheduler.submit(tap("old-target",1,1,start,start+300_000_000,10,10))
    clock.value=start+299_000_000
    scheduler.set_gate(1,True,clock())
    clock.value=start+300_000_000
    scheduler.run_due()
    results.append({"case":"recheck_plan_evidence_at_dispatch","passed":not backend.commands,
        "accepted":accepted,"plan_evidence_age_ms":300,"gate_evidence_age_ms":1,"limit_ms":150,
        "phases":[c.phase for c in backend.commands],
        "expected":"Fresh UI gate alone does not refresh evidence for an old target plan"})

    clock,backend,scheduler=make()
    start=clock()
    scheduler.submit(tap("same-note",1,1,start,start+10_000_000,10,10))
    clock.value=start+10_000_000
    scheduler.run_due()
    clock.value=start+30_000_000
    scheduler.run_due()
    scheduler.set_gate(1,True,clock())
    accepted=scheduler.submit(tap("same-note",2,1,clock(),clock()+10_000_000,10,10))
    clock.value+=10_000_000
    scheduler.run_due()
    downs=sum(c.phase=="down" for c in backend.commands)
    results.append({"case":"do_not_retrigger_completed_intent","passed":downs==1,
        "second_revision_accepted":accepted,"down_count":downs,
        "expected":"New revision of an already completed intent must not generate a second down; new intent uses new key"})
    return results


if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--output",type=Path)
    args=parser.parse_args()
    results=verify()
    report={"all_passed":all(case["passed"] for case in results),"cases":results}
    text=json.dumps(report,indent=2)
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(text,encoding="utf-8")
    print(text)
    raise SystemExit(0 if report["all_passed"] else 1)
