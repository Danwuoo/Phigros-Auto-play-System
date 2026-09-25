"""Bounded single-owner contact scheduler with a host monotonic deadline."""

from dataclasses import dataclass
from contextlib import nullcontext
import queue
import sys
import threading
import time

from .action_planner import ContactPlan
from .contracts import TouchCommand, TouchReceipt


@dataclass
class _Pending:
    plan: ContactPlan
    contact_id: int
    next_step: int = 0
    active: bool = False


class ContactScheduler:
    """All methods except revoke are called by one owner thread."""

    def __init__(self, backend, *, max_contacts=2, max_plans=64, max_steps=16,
                 horizon_ns=2_000_000_000, evidence_max_age_ns=150_000_000,
                 max_late_ns=30_000_000,
                 clock=time.monotonic_ns, on_event=None, dispatch_guard=None,
                 stop_requested=lambda: False):
        self.backend = backend
        self.max_contacts, self.max_plans, self.max_steps = max_contacts, max_plans, max_steps
        self.horizon_ns, self.evidence_max_age_ns = horizon_ns, evidence_max_age_ns
        self.max_late_ns = max_late_ns
        self.clock = clock
        self.on_event = on_event or (lambda _event, **_fields: None)
        self.dispatch_guard = dispatch_guard
        self.stop_requested = stop_requested
        self.epoch = 0
        self.armed = False
        self.latest_evidence_ns = 0
        self._pending: dict[str, _Pending] = {}
        self._completed: dict[str, tuple[int, int]] = {}
        self._owner = None
        self.fault: str | None = None
        self.last_release = None

    def _own(self):
        ident = threading.get_ident()
        if self._owner is None:
            self._owner = ident
        elif ident != self._owner:
            raise RuntimeError("scheduler has one owner thread")

    @property
    def pending_count(self):
        return len(self._pending)

    @property
    def active_ids(self):
        return tuple(sorted(p.contact_id for p in self._pending.values() if p.active))

    def set_gate(self, epoch: int, armed: bool, evidence_ns: int = 0):
        self._own()
        if epoch < self.epoch:
            return False
        if epoch != self.epoch or not armed:
            self.cancel("epoch_or_gate_revoked")
            self.epoch = epoch
        self.armed = armed
        if armed:
            if evidence_ns < self.latest_evidence_ns:
                self.armed = False
                self.cancel("evidence_time_regressed")
                return False
            self.latest_evidence_ns = evidence_ns
        return True

    def submit(self, plan: ContactPlan):
        self._own()
        now = self.clock()
        if self.fault or not self.armed or plan.epoch != self.epoch:
            return False
        if len(plan.steps) > self.max_steps or len(self._pending) >= self.max_plans and plan.key not in self._pending:
            self._fail("scheduler_capacity")
            return False
        if plan.steps[0].due_ns > now + self.horizon_ns or plan.valid_until_ns < now:
            return False
        if plan.evidence_ns > now:
            return False
        if now - plan.evidence_ns > self.evidence_max_age_ns:
            return False
        old = self._pending.get(plan.key)
        if old is None:
            completed = self._completed.get(plan.key)
            if completed is not None and plan.revision <= completed[0]:
                return False
            used = {p.contact_id for p in self._pending.values()}
            available = next((i for i in range(self.max_contacts) if i not in used), None)
            if available is None:
                self._fail("contact_capacity")
                return False
            self._pending[plan.key] = _Pending(plan, available)
        else:
            if plan.revision <= old.plan.revision:
                return False
            if old.active:
                # The new down is only a template. An existing contact retains
                # its identifier and the replacement must still contain an up.
                if plan.steps[-1].due_ns < now:
                    return False
                old.next_step = 1
            else:
                old.next_step = 0
            old.plan = plan
        self.on_event("plan_accepted", key=plan.key, revision=plan.revision,
                      epoch=plan.epoch, basis=plan.basis, due_ns=plan.steps[0].due_ns)
        return True

    def next_due_ns(self):
        self._own()
        times = [p.plan.steps[p.next_step].due_ns for p in self._pending.values()]
        return min(times) if times else None

    def _fail(self, reason):
        self.fault = reason
        self.on_event("scheduler_fault", reason=reason, monotonic_ns=self.clock())
        self.cancel(reason)

    def _release(self):
        try:
            self.last_release = self.backend.release_all()
            if self.last_release is not None and (self.last_release.failed_ids or self.last_release.unknown_ids):
                self.fault = "release_failed"
        except Exception as error:
            self.fault = f"release_failed:{type(error).__name__}"
        self.on_event("release", report=str(self.last_release), fault=self.fault,
                      monotonic_ns=self.clock())

    def cancel(self, reason: str):
        self._own()
        self.armed = False
        self._pending.clear()
        self._completed.clear()
        self._release()
        self.on_event("scheduler_cancelled", reason=reason, monotonic_ns=self.clock())

    def run_due(self):
        self._own()
        receipts: list[TouchReceipt] = []
        while self._pending and self.armed and not self.fault:
            now = self.clock()
            due = [(p.plan.steps[p.next_step].due_ns, key) for key, p in self._pending.items()]
            due_ns, key = min(due)
            if due_ns > now:
                break
            pending = self._pending[key]
            step = pending.plan.steps[pending.next_step]
            if step.phase != "up" and (now - self.latest_evidence_ns > self.evidence_max_age_ns or
                                        now - step.due_ns > self.max_late_ns or
                                        now > pending.plan.valid_until_ns and step.phase == "down"):
                self._fail("stale_expired_or_late_step")
                break
            command = TouchCommand(key, pending.contact_id, step.phase, step.x, step.y,
                                   step.due_ns, pending.plan.source_frame_sequence)
            try:
                with self.dispatch_guard if self.dispatch_guard is not None else nullcontext():
                    if self.stop_requested():
                        break
                    receipt = self.backend.inject(command)
            except Exception as error:
                self._fail(f"inject_exception:{type(error).__name__}")
                break
            receipts.append(receipt)
            self.on_event("touch_receipt", key=key, epoch=self.epoch, phase=step.phase,
                          scheduled_ns=step.due_ns, injection_start_ns=receipt.injection_start_ns,
                          injection_return_ns=receipt.injection_return_ns,
                          success=receipt.success, reason=receipt.reason)
            if not receipt.success:
                self._fail("rpc_result_unknown")
                break
            pending.active = step.phase != "up"
            pending.next_step += 1
            if step.phase == "up":
                self._completed[key] = (pending.plan.revision, self.clock())
                del self._pending[key]
                if len(self._completed) > 2 * self.max_plans:
                    oldest = min(self._completed, key=lambda k: self._completed[k][1])
                    del self._completed[oldest]
        return receipts


class SchedulerOwner:
    """Message boundary for a live scheduler. The backend is created on this thread."""

    def __init__(self, factory, *, max_messages=64, scheduler_kwargs=None):
        self.factory = factory
        self.messages = queue.Queue(maxsize=max_messages)
        self.scheduler_kwargs = scheduler_kwargs or {}
        self._thread = None
        self._stopping = threading.Event()
        self._dispatch_guard = threading.Lock()
        self.error = None
        self.timer_resolution_ms = None

    def start(self):
        if self._thread is not None:
            raise RuntimeError("owner already started")
        self._thread = threading.Thread(target=self._run, name="touch-owner")
        self._thread.start()

    def _run(self):
        backend = None
        timer_period_set = False
        try:
            if sys.platform == "win32":
                import ctypes
                timer_period_set = ctypes.windll.winmm.timeBeginPeriod(1) == 0
                self.timer_resolution_ms = 1 if timer_period_set else None
            backend = self.factory()
            engine = ContactScheduler(backend,dispatch_guard=self._dispatch_guard,
                                      stop_requested=self._stopping.is_set,**self.scheduler_kwargs)
            while not self._stopping.is_set():
                deadline = engine.next_due_ns()
                timeout = 0.05 if deadline is None else max(0, min(0.05, (deadline - time.monotonic_ns()) / 1e9))
                try:
                    operation, args, reply = self.messages.get(timeout=timeout)
                except queue.Empty:
                    operation = None
                if operation:
                    try:
                        result = getattr(engine, operation)(*args)
                        if reply:
                            reply.put((True, result))
                    except Exception as error:
                        if reply:
                            reply.put((False, error))
                        else:
                            raise
                if self._stopping.is_set():
                    break
                engine.run_due()
            engine.cancel("owner_stop")
            if engine.fault:
                self.error = engine.fault
        except Exception as error:
            self.error = str(error)
        finally:
            if backend is not None:
                try:
                    if getattr(backend, "active_ids", ()):
                        backend.release_all()
                    backend.close()
                except Exception as error:
                    self.error = f"backend_cleanup:{error}"
            if timer_period_set:
                ctypes.windll.winmm.timeEndPeriod(1)

    def _request_stop(self):
        with self._dispatch_guard:
            self._stopping.set()

    def call(self, operation, *args, timeout_s=2):
        if self._thread is None or not self._thread.is_alive():
            raise RuntimeError(f"touch owner unavailable: {self.error}")
        reply = queue.Queue(maxsize=1)
        try:
            self.messages.put_nowait((operation, args, reply))
        except queue.Full:
            self._request_stop()
            raise RuntimeError("touch owner mailbox full")
        try:
            ok, value = reply.get(timeout=timeout_s)
        except queue.Empty:
            self._request_stop()
            raise TimeoutError("touch owner did not answer")
        if not ok:
            raise value
        return value

    def stop(self, timeout_s=5):
        self._request_stop()
        if self._thread:
            self._thread.join(timeout_s)
            if self._thread.is_alive():
                raise TimeoutError("touch owner failed to stop")
        return self.error
